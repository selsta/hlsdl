#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

#ifndef _MSC_VER
#if !defined(__APPLE__) && !defined(__MINGW32__)
#include <sys/prctl.h>
#endif
#include <unistd.h>
#else
#include <Windows.h>
#define sleep Sleep
#endif

#include <inttypes.h>

#include "curl.h"
#include "hls.h"
#include "msg.h"
#include "misc.h"
#include "aes.h"
#include "mpegts.h"

static uint64_t get_duration_ms(const char *ptr)
{
    uint64_t v1 = 0;
    uint64_t v2 = 0;
    uint32_t n = 0;
    bool hasDot = false;

    while (*ptr == ' ' || *ptr == '\t' ) ++ptr;

    while (*ptr != '\0') {
        if (*ptr >= '0' && *ptr <= '9') {
            uint32_t digit = (uint32_t)((*ptr) - '0');
            if (!hasDot)
                v1 = v1 * 10 + digit;
            else if (n < 3) {
                ++n;
                v2 = v2 * 10 + digit;
            }
            else
                break;
        }
        else if (*ptr == '.' && !hasDot) {
            hasDot = true;
        }
        else
            break;
        ++ptr;
    }

    if (v2 > 0)
    while (n < 3) {
        ++n;
        v2 *= 10;
    }

    return v1 * 1000 + v2;
}

static void set_hls_http_header(void *session)
{
    if (hls_args.user_agent) {
        set_user_agent_http_session(session, hls_args.user_agent);
    }

    if (hls_args.proxy_uri) {
        set_proxy_uri_http_session(session, hls_args.proxy_uri);
    }

    if (hls_args.cookie_file) {
        set_cookie_file_session(session, hls_args.cookie_file, hls_args.cookie_file_mutex);
    }

    for (int i=0; i<HLSDL_MAX_NUM_OF_CUSTOM_HEADERS; ++i) {
        if (hls_args.custom_headers[i]) {
            add_custom_header_http_session(session, hls_args.custom_headers[i]);
        }
        else {
            break;
        }
    }
}

static void * init_hls_session(void)
{
    void *session = init_http_session();
    assert(session);
    set_hls_http_header(session);
    return session;
}

long get_hls_data_from_url(char *url, char **out, size_t *size, int type, char **new_url)
{
    void *session = init_hls_session();
    long http_code = get_data_from_url_with_session(&session, url, out, size, type, new_url, NULL);
    clean_http_session(session);
    return http_code;
}

int get_playlist_type(char *source)
{
    if (strncmp("#EXTM3U", source, 7) != 0) {
        MSG_WARNING("Not a valid M3U8 file. Exiting.\n");
        return -1;
    }

    if (strstr(source, "#EXT-X-STREAM-INF")) {
        return 0;
    }

    return 1;
}

static int extend_url(char **url, const char *baseurl)
{
    static const char proxy_marker[] = "englandproxy.co.uk"; // ugly workaround to be fixed
    static const char proxy_url[] = "http://www.englandproxy.co.uk/";
    size_t max_length = strlen(*url) + strlen(baseurl) + 10;

    if (!strncmp(*url, "http://", 7) || !strncmp(*url, "https://", 8)) {
        if (strstr(baseurl, proxy_marker) && !strstr(*url, proxy_marker)) {
            max_length = strlen(*url) + strlen(proxy_url);
            char *buffer = malloc(max_length);
            snprintf(buffer, max_length, "%s%s", proxy_url, strstr(*url, "://") + 3);
            *url = realloc(*url, strlen(buffer) + 1);
            strcpy(*url, buffer);
            free(buffer);
        }
        return 0;
    }
    else if (**url == '/') {
        char *domain = malloc(max_length);
        strcpy(domain, baseurl);
        char proto[6];
        if( 2 == sscanf(baseurl, "%5[^:]://%[^/]", proto, domain))
        {
            char *buffer = malloc(max_length);
            if ( (*url)[1] == '/') // url start with "//"
            {
                snprintf(buffer, max_length, "%s:%s", proto, *url);
            }
            else
            {
                snprintf(buffer, max_length, "%s://%s%s", proto, domain, *url);
            }
            *url = realloc(*url, strlen(buffer) + 1);
            strcpy(*url, buffer);
            free(buffer);
        }
        free(domain);
        return 0;
    }

    else {
        // URLs can have '?'. To make /../ work, remove it.
        char *domain = strdup(baseurl);
        char *find_questionmark = strchr(domain, '?');
        if (find_questionmark) {
            *find_questionmark = '\0';
        }

        char *buffer = malloc(max_length);
        snprintf(buffer, max_length, "%s/../%s", domain, *url);
        *url = realloc(*url, strlen(buffer) + 1);
        strcpy(*url, buffer);
        free(buffer);
        free(domain);
        return 0;
    }
}

static int parse_tag(hls_media_playlist_t *me, struct hls_media_segment *ms, char *tag, int64_t *seg_offset, int64_t *seg_size)
{
    int enc_type;

    if (!strncmp(tag, "#EXT-X-KEY:METHOD=AES-128", 25)) {
        enc_type = ENC_AES128;
    } else if (!strncmp(tag, "#EXT-X-KEY:METHOD=SAMPLE-AES", 28)) {
        enc_type = ENC_AES_SAMPLE;
    } else  {
        if (!strncmp(tag, "#EXTINF:", 8)){
            ms->duration_ms = get_duration_ms(tag+8);
            return 0;
        } else if (!strncmp(tag, "#EXT-X-ENDLIST", 14)){
            me->is_endlist = true;
            return 0;
        } else if (!strncmp(tag, "#EXT-X-MEDIA-SEQUENCE:", 22)){
            if(sscanf(tag+22, "%d",  &(me->first_media_sequence)) == 1){
                return 0;
            }
        } else if (!strncmp(tag, "#EXT-X-TARGETDURATION:", 22)){
            me->target_duration_ms = get_duration_ms(tag+22);
            return 0;
        } else if (!strncmp(tag, "#EXT-X-BYTERANGE:", 17)) {
            *seg_size = strtoll(tag+17, NULL, 10);
            tag = strchr(tag+17, '@');
            if (tag) {
                *seg_offset = strtoll(tag+1, NULL, 10);
            }
            return 0;
        }
        return 1;
    }

    me->encryption = true;
    me->encryptiontype = enc_type;
    me->enc_aes.iv_is_static = false;

    char *link_to_key = malloc(strlen(tag) + strlen(me->url) + 10);
    char iv_str[STRLEN_BTS(KEYLEN)] = "\0";
    char sep = '\0';
    if ((sscanf(tag, "#EXT-X-KEY:METHOD=AES-128,URI=\"%[^\"]\",IV=0%c%32[0-9a-f]", link_to_key, &sep, iv_str) > 0 ||
         sscanf(tag, "#EXT-X-KEY:METHOD=SAMPLE-AES,URI=\"%[^\"]\",IV=0%c%32[0-9a-f]", link_to_key, &sep, iv_str) > 0))
    {
        if (sep == 'x' || sep == 'X')
        {
            uint8_t *iv_bin = malloc(KEYLEN);
            str_to_bin(iv_bin, iv_str, KEYLEN);
            memcpy(me->enc_aes.iv_value, iv_bin, KEYLEN);
            me->enc_aes.iv_is_static = true;
            free(iv_bin);
        }

        extend_url(&link_to_key, me->url);

        free(me->enc_aes.key_url);
        me->enc_aes.key_url = strdup(link_to_key);
    }
    free(link_to_key);
    return 0;
}

static int media_playlist_get_links(hls_media_playlist_t *me)
{
    struct hls_media_segment *ms = NULL;
    struct hls_media_segment *curr_ms = NULL;
    char *src = me->source;
    int64_t seg_offset = 0;
    int64_t seg_size = -1;

    MSG_PRINT("> START media_playlist_get_links\n");

    int i = 0;
    while(src != NULL){
        if (ms == NULL)
        {
            ms = malloc(sizeof(struct hls_media_segment));
            memset(ms, 0x00, sizeof(struct hls_media_segment));
        }

        while ((src = (strchr(src, '\n')))) {
            src++;
            if (*src == '\n') {
                continue;
            }
            if (*src == '\r') {
                continue;
            }
            if (*src == '#') {
                parse_tag(me, ms, src, &seg_offset, &seg_size);
                continue;
            }
            if (*src == '\0') {
                goto finish;
            }

            char *end_ptr = strchr(src, '\n');
            if (end_ptr != NULL) {
                int url_size = (int)(end_ptr - src) + 1;
                ms->url = malloc(url_size);
                strncpy(ms->url, src, url_size-1);
                ms->url[url_size-1] = '\0';
                ms->sequence_number = i + me->first_media_sequence;
                if (me->encryptiontype == ENC_AES128 || me->encryptiontype == ENC_AES_SAMPLE) {
                    memcpy(ms->enc_aes.key_value, me->enc_aes.key_value, KEYLEN);
                    memcpy(ms->enc_aes.iv_value, me->enc_aes.iv_value, KEYLEN);
                    ms->enc_aes.key_url = strdup(me->enc_aes.key_url);
                    if (me->enc_aes.iv_is_static == false) {
                        char iv_str[STRLEN_BTS(KEYLEN)];
                        snprintf(iv_str, STRLEN_BTS(KEYLEN), "%032x\n", ms->sequence_number);
                        uint8_t *iv_bin = malloc(KEYLEN);
                        str_to_bin(iv_bin, iv_str, KEYLEN);
                        memcpy(ms->enc_aes.iv_value, iv_bin, KEYLEN);
                        free(iv_bin);
                    }
                }

                /* Get full url */
                extend_url(&(ms->url), me->url);

                ms->size = seg_size;
                if (seg_size >= 0) {
                    ms->offset = seg_offset;
                    seg_offset += seg_size;
                    seg_size = -1;
                } else {
                    ms->offset = 0;
                    seg_offset = 0;
                }

                /* Add new segment to segment list */
                if (me->first_media_segment == NULL)
                {
                    me->first_media_segment = ms;
                    curr_ms = ms;
                }
                else
                {
                    curr_ms->next = ms;
                    ms->prev = curr_ms;
                    curr_ms = ms;
                }
                ms = NULL;
                i += 1;
                break;
            }
        }
    }

finish:
    me->last_media_segment = curr_ms;

    if (i > 0) {
        me->last_media_sequence = me->first_media_sequence + i - 1;
    }

    media_segment_cleanup(ms);

    MSG_PRINT("> END media_playlist_get_links\n");

    return 0;
}

static uint64_t get_duration_hls_media_playlist(hls_media_playlist_t *me)
{
    uint64_t duration_ms = 0;
    struct hls_media_segment *ms = me->first_media_segment;
    while(ms) {
        duration_ms += ms->duration_ms;
        ms = ms->next;
    }
    return duration_ms;
}

int handle_hls_media_playlist(hls_media_playlist_t *me)
{
    me->encryption = false;
    me->encryptiontype = ENC_NONE;

    if (!me->source) {
        size_t size = 0;
        long http_code = 0;
        int tries = hls_args.open_max_retries;

        while (tries) {
            http_code = get_hls_data_from_url(me->orig_url, &me->source, &size, STRING, &me->url);
            if (200 != http_code || size == 0) {
                MSG_ERROR("%s %d tries[%d]\n", me->orig_url, (int)http_code, (int)tries);
                --tries;
                sleep(1);
                continue;
            }
            break;
        }
    }

    me->first_media_segment = NULL;
    me->last_media_segment = NULL;
    me->target_duration_ms = 0;
    me->is_endlist = false;
    me->last_media_sequence = 0;

    if (media_playlist_get_links(me)) {
        MSG_ERROR("Could not parse links. Exiting.\n");
        return 1;
    }
    me->total_duration_ms = get_duration_hls_media_playlist(me);
    return 0;
}

static bool get_next_attrib(char **source, char **tag, char **val)
{
    bool ret = false;
    char *ptr = NULL;
    char *token = NULL;
    char *value = NULL;
    char end_val_marker = '\0';
    char *src = *source;
    while (*src != '\0' && strchr(", \t\n\r", *src)) {
        ++src;
        continue;
    }

    ptr = src;
    while (*ptr != '=' && *ptr != '\0' && !strchr("\n\r", *ptr)) ++ptr;
    if (*ptr != '\0') {
        token = src;
        *ptr = '\0';

        ptr += 1;
        if (*ptr == '"') {
            ++ptr;
            end_val_marker = '"';
        } else {
            end_val_marker = ',';
        }

        value = ptr;
        while (*ptr != end_val_marker && *ptr != '\0' && !strchr("\n\r", *ptr)) ++ptr;
        src = ptr;
        if (*ptr) {
            ++src;
            *ptr = '\0';
        }

        if (*value) {
            *val = value;
            *tag = token;
            ret = true;
        }
        *source = src;
    } else {
        *source = ptr;
    }

    return ret;
}

int handle_hls_master_playlist(struct hls_master_playlist *ma)
{
    bool url_expected = false;
    unsigned int bitrate = 0;
    char *res = NULL;
    char *codecs = NULL;
    char *audio_grp = NULL;

    char *token = NULL;
    char *value = NULL;

    char *src = ma->source;
    while(*src != '\0'){
        char *end_ptr = strchr(src, '\n');
        if (!end_ptr) {
            goto finish;
        }
        *end_ptr = '\0';
        if (*src == '#') {
            url_expected = false;
            bitrate = 0;
            res = NULL;
            codecs = NULL;
            audio_grp = NULL;

            if (!strncmp(src, "#EXT-X-STREAM-INF:", 18)) {
                src += 18;
                while (get_next_attrib(&src, &token, &value)) {
                    if (!strncmp(token, "BANDWIDTH", 9)) {
                        sscanf(value, "%u", &bitrate);
                    } else if (!strncmp(token, "AUDIO", 5)) {
                        audio_grp = value;
                    } else if (!strncmp(token, "RESOLUTION", 10)) {
                        res = value;
                    } else if (!strncmp(token, "CODECS", 6)) {
                        codecs = value;
                    }
                }
                url_expected = true;
            } else if (!strncmp(src, "#EXT-X-MEDIA:TYPE=AUDIO,", 24)) {
                src += 24;
                char *grp_id = NULL;
                char *name = NULL;
                char *lang = NULL;
                char *url = NULL;
                bool is_default = false;

                while (get_next_attrib(&src, &token, &value)) {
                    if (!strncmp(token, "GROUP-ID", 8)) {
                        grp_id = value;
                    } else if (!strncmp(token, "NAME", 4)) {
                        name = value;
                    } else if (!strncmp(token, "LANGUAGE", 8)) {
                        lang = value;
                    } else if (!strncmp(token, "URI", 3)) {
                        url = value;
                    } else if (!strncmp(token, "DEFAULT", 7)) {
                        if (!strncmp(value, "YES", 3)) {
                            is_default = true;
                        }
                    }
                }

                if (grp_id && name && url) {
                    size_t len = strlen(url);

                    hls_audio_t *audio = malloc(sizeof(hls_audio_t));
                    memset(audio, 0x00, sizeof(hls_audio_t));
                    audio->url = malloc(len + 1);
                    memcpy(audio->url, url, len);
                    audio->url[len] = '\0';
                    extend_url(&(audio->url), ma->url);

                    audio->grp_id = strdup(grp_id);
                    audio->name = strdup(name);
                    audio->lang = lang ? strdup(lang) : NULL;
                    audio->is_default = is_default;

                    if (ma->audio) {
                        audio->next = ma->audio;
                    }
                    ma->audio = audio;
                }
            }
        } else if (url_expected) {
            size_t len = strlen(src);

            // here we will fill new playlist
            hls_media_playlist_t *me = malloc(sizeof(hls_media_playlist_t));
            memset(me, 0x00, sizeof(hls_media_playlist_t));

            me->url = malloc(len + 1);
            memcpy(me->url, src, len);
            me->url[len] = '\0';
            extend_url(&(me->url), ma->url);
            me->bitrate = bitrate;
            me->audio_grp = audio_grp ? strdup(audio_grp) : NULL;
            me->resolution = res ? strdup(res) : strdup("unknown");
            me->codecs = codecs ? strdup(codecs) : strdup("unknown");

            if (ma->media_playlist) {
                me->next = ma->media_playlist;
            }
            ma->media_playlist = me;

            url_expected = false;
        }

        src = end_ptr + 1;
    }

finish:
    return 0;
}

static int sample_aes_append_av_data(ByteBuffer_t *out, ByteBuffer_t *in, const uint8_t *pcr, uint16_t pid, uint8_t *cont_count)
{
    uint8_t *av_data = in->data;
    uint32_t av_size = in->pos;

    uint8_t ts_header[4] = {TS_SYNC_BYTE, 0x40, 0x00, 0x10};
    ts_header[1] = ((pid >> 8) & 0x1F) | 0x40; // 0x40 - set payload_unit_start_indicator
    ts_header[2] = pid & 0xFF;

    uint8_t adapt_header[8] = {0x00};
    uint8_t adapt_header_size = 0;
    uint32_t payload_size = TS_PACKET_LENGTH - sizeof(ts_header);
    if (pcr[0] & 0x10) {
        adapt_header_size = 8;
        adapt_header[1] = pcr[0] & 0xF0; // set previus flags: discontinuity_indicator, random_access_indicator, elementary_stream_priority_indicator, PCR_flag
    } else if (pcr[0] & 0x20) {
        adapt_header_size = 2;
        adapt_header[1] = pcr[0] & 0xF0; // restore flags as described above
    } else if (av_size < payload_size) {
        adapt_header_size = payload_size - av_size == 1 ? 1 : 2;
    }

    payload_size -= adapt_header_size;

    if (adapt_header_size) {
        adapt_header[0] = adapt_header_size - 1; // size without field size
        if (av_size < payload_size) {
            adapt_header[0] += payload_size - av_size;
        }

        if (adapt_header[0]) {
            ts_header[3] = (ts_header[3] & 0xcf) | 0x30; // set addaptation filed flag
        }
    }

    // ts header
    ts_header[3] = (ts_header[3] & 0xf0) | (*cont_count);
    *cont_count = (*cont_count + 1) % 16;
    memcpy(&out->data[out->pos], ts_header, sizeof(ts_header));
    out->pos += sizeof(ts_header);

    // adaptation field
    if (adapt_header_size) {
        memcpy(&out->data[out->pos], adapt_header, adapt_header_size);
        out->pos += adapt_header_size;
    }

    if (av_size < payload_size) {
        uint32_t s;
        for(s=0; s < payload_size - av_size; ++s) {
            out->data[out->pos + s] = 0xff;
        }
        out->pos += payload_size - av_size;
        payload_size = av_size;
    }

    memcpy(&out->data[out->pos], av_data, payload_size);
    out->pos += payload_size;
    av_data += payload_size;
    av_size -= payload_size;

    if (av_size > 0) {
        uint32_t packets_num = av_size / (TS_PACKET_LENGTH - 4);
        uint32_t p;
        ts_header[1] &= 0xBF; // unset payload_unit_start_indicator
        ts_header[3] = (ts_header[3] & 0xcf) | 0x10; // unset addaptation filed flag
        for (p=0; p < packets_num; ++p) {
           ts_header[3] = (ts_header[3] & 0xf0) | (*cont_count);
           *cont_count = (*cont_count + 1) % 16;
           memcpy(&out->data[out->pos], ts_header, sizeof(ts_header));
           memcpy(&out->data[out->pos+4], av_data, TS_PACKET_LENGTH - sizeof(ts_header));
           out->pos += TS_PACKET_LENGTH;
           av_data += TS_PACKET_LENGTH - sizeof(ts_header);
           av_size -= TS_PACKET_LENGTH - sizeof(ts_header);
        }

        ts_header[3] = (ts_header[3] & 0xcf) | 0x30; // set addaptation filed flag to add aligment
        adapt_header[1] = 0; // none flags set, only for alignment
        if (av_size > 0) {
            ts_header[3] = (ts_header[3] & 0xf0) | (*cont_count);
            *cont_count = (*cont_count + 1) % 16;
            // add ts_header
            memcpy(&out->data[out->pos], ts_header, sizeof(ts_header));

            // add adapt header
            adapt_header_size = TS_PACKET_LENGTH - 4 - av_size == 1 ? 1 : 2;
            adapt_header[0] = adapt_header_size - 1; // size without field size
            if (adapt_header[0]) {
                adapt_header[0] +=  TS_PACKET_LENGTH - 4 - 2 - av_size;
            }

            memcpy(&out->data[out->pos+4], adapt_header, adapt_header_size);
            out->pos += 4 + adapt_header_size;

            // add alignment
            if (adapt_header[0]) {
                int32_t s;
                for(s=0; s < adapt_header[0] - 1; ++s) {
                    out->data[out->pos + s] = 0xff;
                }
                out->pos += adapt_header[0] -1;
            }

            // add payload
            memcpy(out->data + out->pos, av_data, av_size);
            out->pos += av_size;
            av_data += av_size;
            av_size -= av_size;
        }
    }

    return 0;
}


static uint8_t * remove_emulation_prev(const uint8_t  *src,
                                       const uint8_t  *src_end,
                                             uint8_t  *dst,
                                             uint8_t  *dst_end)
{
    while (src + 2 < src_end)
        if (!*src && !*(src + 1) && *(src + 2) == 3) {
            *dst++ = *src++;
            *dst++ = *src++;
            src++; // remove emulation_prevention_three_byte
        } else
            *dst++ = *src++;

    while (src < src_end)
        *dst++ = *src++;

    return dst;
}

static uint8_t *ff_avc_find_startcode_internal(uint8_t *p, uint8_t *end)
{
    uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(uint32_t*)p;
        if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

static uint8_t *ff_avc_find_startcode(uint8_t *p, uint8_t *end){
    uint8_t *out= ff_avc_find_startcode_internal(p, end);
    if(p<out && out<end && !out[-1]) out--;
    return out;
}

static int sample_aes_decrypt_nal_units(hls_media_segment_t *s, uint8_t *buf_in, int size)
{
    uint8_t *end = buf_in + size;
    uint8_t *nal_start;
    uint8_t *nal_end;

    end = remove_emulation_prev(buf_in, end, buf_in, end);

    nal_start = ff_avc_find_startcode(buf_in, end);
    for (;;) {
        while (nal_start < end && !*(nal_start++));
        if (nal_start == end)
            break;

        nal_end = ff_avc_find_startcode(nal_start, end);
        // NAL unit with length of 48 bytes or fewer is completely unencrypted.
        if (nal_end - nal_start > 48) {
            nal_start += 32;
            void *ctx = AES128_CBC_CTX_new();
            AES128_CBC_DecryptInit(ctx, s->enc_aes.key_value, s->enc_aes.iv_value, false);
            while (nal_start + 16 < nal_end) {
                AES128_CBC_DecryptUpdate(ctx, nal_start, nal_start, 16);
                nal_start += 16 * 10; // Each 16-byte block of encrypted data is followed by up to nine 16-byte blocks of unencrypted data.
            }
            AES128_CBC_free(ctx);
        }
        nal_start = nal_end;
    }
    return (int)(end - buf_in);
}

static int sample_aes_decrypt_audio_data(hls_media_segment_t *s, uint8_t *ptr, uint32_t size, audiotype_t audio_codec)
{
    bool (* get_next_frame)(const uint8_t **, const uint8_t *, uint32_t *);
    switch (audio_codec)
    {
    case AUDIO_ADTS:
        get_next_frame = adts_get_next_frame;
        break;
    case AUDIO_AC3:
        get_next_frame = ac3_get_next_frame;
        break;
    case AUDIO_EC3:
        get_next_frame = ec3_get_next_frame;
        break;
    case AUDIO_UNKNOWN:
    default:
        MSG_ERROR("Wrong audio_codec! Should never happen here > EXIT!\n");
        exit(1);
    }

    uint8_t *audio_frame = ptr;
    uint8_t *end_ptr = ptr + size;
    uint32_t frame_length = 0;
    while (get_next_frame((const uint8_t **)&audio_frame, end_ptr, &frame_length)) {
        // The IV must be reset at the beginning of every packet.
        uint8_t leaderSize = 0;

        if (audio_codec == AUDIO_ADTS) {
            // ADTS headers can contain CRC checks.
            // If the CRC check bit is 0, CRC exists.
            //
            // Header (7 or 9 byte) + unencrypted leader (16 bytes)
            leaderSize = (audio_frame[1] & 0x01) ? 23 : 25;
        } else { // AUDIO_AC3, AUDIO_EC3
            // AC3 Audio is untested. Sample streams welcome.
            //
            // unencrypted leader
            leaderSize = 16;
        }

        int tmp_size = frame_length > leaderSize ? (frame_length - leaderSize) & 0xFFFFFFF0  : 0;
        if (tmp_size) {
            void *ctx = AES128_CBC_CTX_new();
            AES128_CBC_DecryptInit(ctx, s->enc_aes.key_value, s->enc_aes.iv_value, false);
            AES128_CBC_DecryptUpdate(ctx, audio_frame + leaderSize, audio_frame + leaderSize, tmp_size);
            AES128_CBC_free(ctx);
        }

        audio_frame += frame_length;
    }

    return 0;
}


static int sample_aes_handle_pes_data(hls_media_segment_t *s, ByteBuffer_t *out, ByteBuffer_t *in, uint8_t *pcr, uint16_t pid, audiotype_t audio_codec, uint8_t *counter)
{
    uint16_t pes_header_size = 0;

    // we need to skip PES header it is not part of NAL unit
    if (in->pos <= PES_HEADER_SIZE || in->data[0] != 0x00 || in->data[1] != 0x00 || in->data[1] == 0x01) {
        MSG_ERROR("Wrong or missing PES header!\n");
        return -1;
    }

    pes_header_size = in->data[8] + 9;
    if (pes_header_size >= in->pos) {
        MSG_ERROR("Wrong PES header size %hu!\n", &pes_header_size);
        return -1;
    }

    if (AUDIO_UNKNOWN == audio_codec) {
        // handle video data in NAL units
        int size = sample_aes_decrypt_nal_units(s, in->data + pes_header_size, in->pos - pes_header_size) + pes_header_size;

        // to check if I did not any mistake in offset calculation
        if (size > in->pos) {
            MSG_ERROR("NAL size after decryption is grater then before - before: %d, after: %d - should never happen!\n", size);
            exit(-1);
        }

        // output size could be less then input because the start code emulation prevention could be removed if available
        if (size < in->pos) {
            // we need to update size in the PES header if it was set
            int32_t payload_size = ((uint16_t)(in->data[4]) << 8) | in->data[5];
            if (payload_size > 0) {
                payload_size -=  in->pos - size;
                in->data[4] = (payload_size >> 8) & 0xff;
                in->data[5] = payload_size & 0xff;
            }
            in->pos = size;
        }
    } else {
        sample_aes_decrypt_audio_data(s, in->data + pes_header_size, in->pos - pes_header_size, audio_codec);
    }

    return sample_aes_append_av_data(out, in, pcr, pid, counter);
}

static int decrypt_sample_aes(hls_media_segment_t *s, ByteBuffer_t *buf)
{
    int ret = 0;
    fill_key_value(&(s->enc_aes));
    if (buf->len > TS_PACKET_LENGTH && buf->data[0] == TS_SYNC_BYTE) {
        pmt_data_t pmt = {0};
        if (find_pmt(buf->data, buf->len, &pmt)) {
            bool write_pmt = true;
            uint16_t audio_PID = PID_UNSPEC;
            uint16_t video_PID = PID_UNSPEC;
            audiotype_t audio_codec = AUDIO_UNKNOWN;
            uint32_t i;
            // https://developer.apple.com/library/archive/documentation/AudioVideo/Conceptual/HLS_Sample_Encryption/TransportStreamSignaling/TransportStreamSignaling.html
            for (i=0; i < pmt.component_num; ++i) {
                uint8_t stream_type = pmt.components[i].stream_type;
                switch (stream_type) {
                    case 0xdb:
                        video_PID = pmt.components[i].elementary_PID;
                        stream_type = 0x1B; // AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video, or AVC base layer of an HEVC video stream as defined in ITU-T H.265 | ISO/IEC 23008-2
                        break;
                    case 0xcf:
                        audio_codec = AUDIO_ADTS;
                        audio_PID = pmt.components[i].elementary_PID;
                        stream_type = 0x0F; // ISO/IEC 13818-7 Audio with ADTS transport syntax
                        break;
                    case 0xc1:
                        audio_codec = AUDIO_AC3;
                        audio_PID = pmt.components[i].elementary_PID;
                        stream_type = 0x81; // User Private / AC-3 (ATSC)
                        break;
                    case 0xc2:
                        audio_codec = AUDIO_EC3;
                        audio_PID = pmt.components[i].elementary_PID;
                        stream_type = 0x87; // User Private / E-AC-3 (ATSC)
                        break;
                    default:
                        MSG_DBG("Unknown component type: 0x%02hhx, pid: 0x%03hx\n", pmt.components[i].stream_type, pmt.components[i].elementary_PID);
                        break;
                }

                if (stream_type != pmt.components[i].stream_type) {
                    // we update stream type to reflect unencrypted data
                    pmt.components[i].stream_type = stream_type;
                    pmt.data[pmt.components[i].offset] = stream_type;
                }
            }

            if (audio_PID != PID_UNSPEC || video_PID != PID_UNSPEC) {
                uint8_t audio_counter = 0;
                uint8_t video_counter = 0;
                uint8_t audio_pcr[7]; // first byte is adaptation filed flags
                uint8_t video_pcr[7]; // - || -
                ByteBuffer_t outBuffer = {NULL};
                outBuffer.data = malloc(buf->len);
                outBuffer.len = buf->len;

                ByteBuffer_t audioBuffer = {NULL};
                ByteBuffer_t videoBuffer = {NULL};

                if (audio_PID != PID_UNSPEC) {
                    audioBuffer.data = malloc(buf->len);
                    audioBuffer.len = buf->len;
                }

                if (video_PID != PID_UNSPEC) {
                    videoBuffer.data = malloc(buf->len);
                    videoBuffer.len = buf->len;
                }

                // collect all audio and video data
                uint32_t packet_id = 0;
                uint8_t *ptr = buf->data;
                uint8_t *end = ptr + buf->len;
                while (ptr + TS_PACKET_LENGTH <= end) {
                    if (*ptr != TS_SYNC_BYTE) {
                        MSG_WARNING("Expected sync byte but got 0x%02hhx!\n", *ptr);
                        ptr += 1;
                        continue;
                    }
                    ts_packet_t packed = {0};
                    parse_ts_packet(ptr, &packed);

                    if (packed.pid == pmt.pid) {
                        if (write_pmt) {
                            write_pmt = false;
                            pmt_update_crc(&pmt);
                            memcpy(&outBuffer.data[outBuffer.pos], pmt.data, TS_PACKET_LENGTH);
                            outBuffer.pos += TS_PACKET_LENGTH;
                        }
                    } else if (packed.pid == audio_PID || packed.pid == video_PID) {
                        ByteBuffer_t *pCurrBuffer = packed.pid == audio_PID ? &audioBuffer : &videoBuffer;
                        uint8_t *pcr = packed.pid == audio_PID ? audio_pcr : video_pcr;
                        uint8_t *counter = packed.pid == audio_PID ? &audio_counter : &video_counter;

                        if (packed.unitstart) {
                            // consume previous data if any
                            if (pCurrBuffer->pos) {
                                sample_aes_handle_pes_data(s, &outBuffer, pCurrBuffer, pcr, packed.pid, packed.pid == audio_PID ? audio_codec : AUDIO_UNKNOWN, counter);
                            }

                            if ((packed.afc & 2) && (ptr[5] & 0x10)) { // remember PCR if available
                                memcpy(pcr, ptr + 4 + 1, 6);
                            } else if ((packed.afc & 2) && (ptr[5] & 0x20)) { // remember discontinuity_indicator if set
                                pcr[0] = ptr[5];
                            } else {
                                pcr[0] = 0;
                            }
                            pCurrBuffer->pos = 0;
                        }

                        if (packed.payload_offset < TS_PACKET_LENGTH) {
                            memcpy(&(pCurrBuffer->data[pCurrBuffer->pos]), ptr + packed.payload_offset, TS_PACKET_LENGTH - packed.payload_offset);
                            pCurrBuffer->pos += TS_PACKET_LENGTH - packed.payload_offset;
                        }
                    } else {
                        memcpy(&outBuffer.data[outBuffer.pos], ptr, TS_PACKET_LENGTH);
                        outBuffer.pos += TS_PACKET_LENGTH;
                    }

                    ptr += TS_PACKET_LENGTH;
                    packet_id += 1;
                }

                if (audioBuffer.pos) {
                    sample_aes_handle_pes_data(s, &outBuffer, &audioBuffer, audio_pcr, audio_PID, audio_codec, &audio_counter);
                }

                if (videoBuffer.pos) {
                    sample_aes_handle_pes_data(s, &outBuffer, &videoBuffer, video_pcr, video_PID, AUDIO_UNKNOWN, &video_counter);
                }

                if (outBuffer.pos > buf->len ) {
                    MSG_ERROR("decrypt_sample_aes - buffer overflow detected!\n");
                    exit(-1);
                }

                free(videoBuffer.data);
                free(audioBuffer.data);

                // replace encrypted data with decrypted one
                free(buf->data);
                buf->data = outBuffer.data;
                buf->len = outBuffer.pos;
            } else {
                MSG_WARNING("None audio nor video component found!\n");
                ret = -3;
            }
        } else {
            MSG_WARNING("PMT could not be found!\n");
            ret = -2;
        }
    } else {
        MSG_WARNING("Unknown segment type!\n");
        ret = -1;
    }

    return ret;
}

static int decrypt_aes128(hls_media_segment_t *s, ByteBuffer_t *buf)
{
    // The AES128 method encrypts whole segments.
    // Simply decrypting them is enough.
    fill_key_value(&(s->enc_aes));

    void *ctx = AES128_CBC_CTX_new();
    /* some AES-128 encrypted segments could be not correctly padded
     * and decryption with padding will fail - example stream with such problem is welcome
     * From other hand dump correctly padded segment will contain trashes, which will cause many
     * errors during processing such TS, for example by DVBInspector,
     * if padding will be not removed.
     */
#if 1
    int out_size = 0;
    AES128_CBC_DecryptInit(ctx, s->enc_aes.key_value, s->enc_aes.iv_value, true);
    AES128_CBC_DecryptPadded(ctx, buf->data, buf->data, buf->len, &out_size);
    // decoded data size could be less then input because of the padding
    buf->len = out_size;
#else
    AES128_CBC_DecryptInit(ctx, s->enc_aes.key_value, s->enc_aes.iv_value, false);
    AES128_CBC_DecryptUpdate(ctx, buf->data, buf->data, buf->len);
#endif
    AES128_CBC_free(ctx);
    return 0;
}

static void *hls_playlist_update_thread(void *arg)
{
#ifndef _MSC_VER
    char threadname[50];
    strncpy(threadname, __func__, sizeof(threadname));
    threadname[49] = '\0';
#if !defined(__APPLE__) && !defined(__MINGW32__)
    prctl(PR_SET_NAME, (unsigned long)&threadname);
#endif
#endif

    hls_playlist_updater_params *updater_params = arg;

    hls_media_playlist_t *me = updater_params->media_playlist;
    pthread_mutex_t *media_playlist_mtx         = (pthread_mutex_t *)(updater_params->media_playlist_mtx);

    pthread_cond_t  *media_playlist_refresh_cond = (pthread_cond_t *)(updater_params->media_playlist_refresh_cond);
    pthread_cond_t  *media_playlist_empty_cond   = (pthread_cond_t *)(updater_params->media_playlist_empty_cond);

    void *session = init_hls_session();
    set_timeout_session(session, 2L, 3L);
    bool is_endlist = false;
    //char *url = NULL;
    int refresh_delay_s = 0;

    // no lock is needed here because download_live_hls not change this fields
    //pthread_mutex_lock(media_playlist_mtx);
    is_endlist = me->is_endlist;
    if (hls_args.refresh_delay_sec < 0) {
        refresh_delay_s = (int)(me->target_duration_ms / 1000);
        //pthread_mutex_unlock(media_playlist_mtx);

        if (refresh_delay_s > HLSDL_MAX_REFRESH_DELAY_SEC) {
            refresh_delay_s = HLSDL_MAX_REFRESH_DELAY_SEC;
        } else if (refresh_delay_s < HLSDL_MIN_REFRESH_DELAY_SEC) {
            refresh_delay_s = HLSDL_MIN_REFRESH_DELAY_SEC;
        }
    } else {
        refresh_delay_s = hls_args.refresh_delay_sec;
    }

    struct timespec ts;
    memset(&ts, 0x00, sizeof(ts));
    MSG_VERBOSE("Update thread started\n");
    while (!is_endlist) {
        // download live hls can interrupt waiting
        ts.tv_sec =  time(NULL) + refresh_delay_s;
        pthread_mutex_lock(media_playlist_mtx);
        pthread_cond_timedwait(media_playlist_refresh_cond, media_playlist_mtx, &ts);
        pthread_mutex_unlock(media_playlist_mtx);

        // update playlist
        hls_media_playlist_t new_me;
        memset(&new_me, 0x00, sizeof(new_me));

        size_t size = 0;
        MSG_PRINT("> START DOWNLOAD LIST url[%s]\n", me->url);
        long http_code = get_data_from_url_with_session(&session, me->url, &new_me.source, &size, STRING, &(new_me.url), NULL);
        MSG_PRINT("> END DOWNLOAD LIST\n");
        if (200 == http_code && 0 == media_playlist_get_links(&new_me)) {
            // no mutex is needed here because download_live_hls not change this fields
            if (new_me.is_endlist ||
                new_me.first_media_sequence != me->first_media_sequence ||
                new_me.last_media_sequence != me->last_media_sequence)
            {
                bool list_extended = false;
                // we need to update list
                pthread_mutex_lock(media_playlist_mtx);
                me->is_endlist = new_me.is_endlist;
                is_endlist = new_me.is_endlist;
                me->first_media_sequence = new_me.first_media_sequence;

                if (new_me.last_media_sequence > me->last_media_sequence)
                {
                    // add new segments
                    struct hls_media_segment *ms = new_me.first_media_segment;
                    while (ms) {
                        if (ms->sequence_number > me->last_media_sequence) {
                            if (ms->prev) {
                                ms->prev->next = NULL;
                            }
                            ms->prev = NULL;

                            if (me->last_media_segment) {
                                me->last_media_segment->next = ms;
                            } else {
                                assert(me->first_media_segment == NULL);
                                me->first_media_segment = ms;
                            }

                            me->last_media_segment = new_me.last_media_segment;
                            me->last_media_sequence = new_me.last_media_sequence;

                            if (ms == new_me.first_media_segment) {
                                // all segments are new
                                new_me.first_media_segment = NULL;
                                new_me.last_media_segment = NULL;
                            }

                            while (ms) {
                                me->total_duration_ms += ms->duration_ms;
                                ms = ms->next;
                            }

                            list_extended = true;
                            break;
                        }

                        ms = ms->next;
                    }
                }
                if (list_extended) {
                    pthread_cond_signal(media_playlist_empty_cond);
                }
                pthread_mutex_unlock(media_playlist_mtx);
            }
        } else {
            MSG_WARNING("Fail to update playlist \"%s\". http_code[%d].\n", me->url, (int)http_code);
            clean_http_session(session);
            sleep(1);
            session = init_hls_session();
            if (session) {
                set_timeout_session(session, 2L, 15L);
                set_fresh_connect_http_session(session, 1);
            }
        }
        media_playlist_cleanup(&new_me);
    }

    clean_http_session(session);
    pthread_exit(NULL);
    return NULL;
}

int download_live_hls(write_ctx_t *out_ctx, hls_media_playlist_t *me)
{
    MSG_API("{\"d_t\":\"live\"}\n");

    hls_playlist_updater_params updater_params;

    /* declaration synchronization prymitives */
    pthread_mutex_t media_playlist_mtx;
    pthread_mutex_t cookie_file_mtx;

    pthread_cond_t  media_playlist_refresh_cond;
    pthread_cond_t  media_playlist_empty_cond;

    /* init synchronization prymitives */
    pthread_mutex_init(&media_playlist_mtx, NULL);
    pthread_mutex_init(&cookie_file_mtx, NULL);

    pthread_cond_init(&media_playlist_refresh_cond, NULL);
    pthread_cond_init(&media_playlist_empty_cond, NULL);

    memset(&updater_params, 0x00, sizeof(updater_params));
    updater_params.media_playlist = me;
    updater_params.media_playlist_mtx = (void *)&media_playlist_mtx;
    updater_params.media_playlist_refresh_cond = (void *)&media_playlist_refresh_cond;
    updater_params.media_playlist_empty_cond   = (void *)&media_playlist_empty_cond;

    hls_args.cookie_file_mutex = (void *)&cookie_file_mtx;

    // skip first segments
    if (me->first_media_segment != me->last_media_segment) {
        struct hls_media_segment *ms = me->last_media_segment;
        uint64_t duration_ms = 0;
        uint64_t duration_offset_ms = hls_args.live_start_offset_sec * 1000;
        while (ms) {
            duration_ms += ms->duration_ms;
            if (duration_ms >= duration_offset_ms) {
                break;
            }
            ms = ms->prev;
        }

        if (ms && ms != me->first_media_segment){
            // remove segments
            while (me->first_media_segment != ms) {
                struct hls_media_segment *tmp_ms = me->first_media_segment;
                me->first_media_segment = me->first_media_segment->next;
                media_segment_cleanup(tmp_ms);
            }
            ms->prev = NULL;
            me->first_media_segment = ms;
        }

        me->total_duration_ms = get_duration_hls_media_playlist(me);
    }

    // start update thread
    pthread_t thread;
    void *ret;

    pthread_create(&thread, NULL, hls_playlist_update_thread, &updater_params);

    void *session = init_hls_session();
    set_timeout_session(session, 2L, 3L);
    uint64_t downloaded_duration_ms = 0;
    int64_t download_size = 0;
    time_t repTime = 0;
    bool download = true;
    char range_buff[22];
    while(download) {

        pthread_mutex_lock(&media_playlist_mtx);
        struct hls_media_segment *ms = me->first_media_segment;
        if (ms != NULL) {
            me->first_media_segment = ms->next;
            if (me->first_media_segment) {
                me->first_media_segment->prev = NULL;
            }
        }
        else {
            me->last_media_segment = NULL;
            download = !me->is_endlist;
        }
        if (ms == NULL) {
            if (download) {
                pthread_cond_signal(&media_playlist_refresh_cond);
                pthread_cond_wait(&media_playlist_empty_cond, &media_playlist_mtx);
            }
        }
        pthread_mutex_unlock(&media_playlist_mtx);
        if (ms == NULL) {
            continue;
        }

        MSG_PRINT("Downloading part %d\n", ms->sequence_number);
        int retries = 0;
        char *range = NULL;
        if (ms->size > -1) {
            snprintf(range_buff, sizeof(range_buff), "%"PRId64"-%"PRId64, ms->offset, ms->offset + ms->size - 1);
            range = range_buff;
        }
        do {
            struct ByteBuffer seg;
            memset(&seg, 0x00, sizeof(seg));
            size_t size = 0;
            long http_code = get_data_from_url_with_session(&session, ms->url, (char **)&(seg.data), &size, BINARY, NULL, range);
            seg.len = (int)size;
            if (!(http_code == 200 || (http_code == 206 && (range != NULL || hls_args.accept_partial_content)))) {
                int first_media_sequence = 0;
                if (seg.data) {
                    free(seg.data);
                    seg.data  = NULL;
                }

                pthread_mutex_lock(&media_playlist_mtx);
                first_media_sequence = me->first_media_sequence;
                pthread_mutex_unlock(&media_playlist_mtx);

                if(http_code != 403 && http_code != 401 &&  http_code != 410 && retries <= hls_args.segment_download_retries && ms->sequence_number > first_media_sequence) {
                    clean_http_session(session);
                    sleep(1);
                    session = init_hls_session();
                    if (session) {
                        set_timeout_session(session, 2L, 5L);
                        set_fresh_connect_http_session(session, 1);
                        MSG_WARNING("Live retry segment %d download, due to previous error. http_code[%d].\n", ms->sequence_number, (int)http_code);
                        retries += 1;
                        continue;
                    }
                }
                else
                {
                    MSG_WARNING("Live mode skipping segment %d. http_code[%d].\n", ms->sequence_number, (int)http_code);
                    break;
                }
            }

            downloaded_duration_ms += ms->duration_ms;

            if (me->encryption == true && me->encryptiontype == ENC_AES128) {
                decrypt_aes128(ms, &seg);
            } else if (me->encryption == true && me->encryptiontype == ENC_AES_SAMPLE) {
                decrypt_sample_aes(ms, &seg);
            }
            download_size += out_ctx->write(seg.data, seg.len, out_ctx->opaque);
            free(seg.data);

            set_fresh_connect_http_session(session, 0);

            time_t curRepTime = time(NULL);
            if ((curRepTime - repTime) >= 1) {
                MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%"PRId64"}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);
                repTime = curRepTime;
            }

            break;
        } while(true);

        media_segment_cleanup(ms);
    }

    pthread_join(thread, &ret);
    pthread_mutex_destroy(&media_playlist_mtx);

    pthread_cond_destroy(&media_playlist_refresh_cond);
    pthread_cond_destroy(&media_playlist_empty_cond);

    pthread_mutex_destroy(&cookie_file_mtx);
    hls_args.cookie_file_mutex = NULL;

    MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%"PRId64"}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);
    if (session)
    {
        clean_http_session(session);
    }

    return 0;
}

static int vod_download_segment(void **psession, hls_media_playlist_t *me, struct hls_media_segment *ms, struct ByteBuffer *seg)
{
    int retries = 0;
    int ret = 0;
    char range_buff[22];
    while (true) {
        MSG_PRINT("Downloading part %d\n", ms->sequence_number);
        char *range = NULL;
        if (ms->size > -1) {
            snprintf(range_buff, sizeof(range_buff), "%"PRId64"-%"PRId64, ms->offset, ms->offset + ms->size - 1);
            range = range_buff;
        }

        memset(seg, 0x00, sizeof(*seg));
        size_t size = 0;
        long http_code = get_data_from_url_with_session(psession, ms->url, (char **)&(seg->data), &size, BINARY, NULL, range);
        seg->len = (int)size;
        if (!(http_code == 200 || (http_code == 206 && (range != NULL || hls_args.accept_partial_content)))) {
            if (seg->data) {
                free(seg->data);
                seg->data = NULL;
            }
            if (http_code != 403 && http_code != 401 && http_code != 410 && retries <= hls_args.segment_download_retries) {
                clean_http_session(*psession);
                sleep(1);
                *psession = init_hls_session();
                set_timeout_session(*psession, 2L, 30L);
                if (*psession) {
                    set_fresh_connect_http_session(*psession, 1);
                    MSG_WARNING("VOD retry segment %d download, due to previous error. http_code[%d].\n", ms->sequence_number, (int)http_code);
                    retries += 1;
                    continue;
                }
            }
            ret = 1;
            MSG_API("{\"error_code\":%d, \"error_msg\":\"http\"}\n", (int)http_code);
            break;
        }
        break;
    }

    if (ret == 0) {
        if (me->encryption == true && me->encryptiontype == ENC_AES128) {
            decrypt_aes128(ms, seg);
        } else if (me->encryption == true && me->encryptiontype == ENC_AES_SAMPLE) {
            decrypt_sample_aes(ms, seg);
        }
    }

    /* normally we want to reuse sessions,
     * so restore it in case when fresh session
     * was requested do to re-try
     */
    if (retries) {
        set_fresh_connect_http_session(*psession, 0);
    }

    return ret;
}

int download_hls(write_ctx_t *out_ctx, hls_media_playlist_t *me, hls_media_playlist_t *me_audio)
{
    MSG_VERBOSE("Downloading segments.\n");
    MSG_API("{\"d_t\":\"vod\"}\n"); // d_t - download type
    MSG_API("{\"t_d\":%u,\"d_d\":0, \"d_s\":0}\n", (uint32_t)(me->total_duration_ms / 1000)); // t_d - total duration, d_d  - download duration, d_s - download size

    int ret = 0;
    void *session = init_hls_session();
    set_timeout_session(session, 2L, 3L);
    assert(session);
    time_t repTime = 0;

    uint64_t downloaded_duration_ms = 0;
    int64_t download_size = 0;
    struct ByteBuffer seg;
    struct ByteBuffer seg_audio;

    struct hls_media_segment *ms = me->first_media_segment;
    struct hls_media_segment *ms_audio = NULL;
    merge_context_t merge_context;

    if (me_audio) {
        ms_audio = me_audio->first_media_segment;
        memset(&merge_context, 0x00, sizeof(merge_context));
        merge_context.out = out_ctx;
    }

    while(ms) {
        if (0 != vod_download_segment(&session, me, ms, &seg)) {
            break;
        }

        // first segment should be TS for success merge
        if (ms_audio && seg.len > TS_PACKET_LENGTH && seg.data[0] == TS_SYNC_BYTE) {
            if ( 0 != vod_download_segment(&session, me_audio, ms_audio, &seg_audio)) {
                break;
            }

            download_size += merge_packets(&merge_context, seg.data, seg.len, seg_audio.data, seg_audio.len);

            free(seg_audio.data);
            ms_audio = ms_audio->next;
        } else {
            download_size += out_ctx->write(seg.data, seg.len, out_ctx->opaque);
        }

        free(seg.data);

        downloaded_duration_ms += ms->duration_ms;

        time_t curRepTime = time(NULL);
        if ((curRepTime - repTime) >= 1) {
            MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%"PRId64"}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);
            repTime = curRepTime;
        }

        ms = ms->next;
    }

    MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%"PRId64"}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);

    if (session) {
        clean_http_session(session);
    }

    return ret;
}

int print_enc_keys(hls_media_playlist_t *me)
{
    struct hls_media_segment *ms = me->first_media_segment;
    while(ms) {
        if (me->encryption == true) {
            fill_key_value(&(ms->enc_aes));
            MSG_PRINT("[AES-128]KEY: 0x");
            for(size_t count = 0; count < KEYLEN; count++) {
                MSG_PRINT("%02x", ms->enc_aes.key_value[count]);
            }
            MSG_PRINT(" IV: 0x");
            for(size_t count = 0; count < KEYLEN; count++) {
                MSG_PRINT("%02x", ms->enc_aes.iv_value[count]);
            }
            MSG_PRINT("\n");
        }
        ms = ms->next;
    }
    return 0;
}

void media_segment_cleanup(struct hls_media_segment *ms)
{
    if (ms)
    {
        free(ms->url);
        free(ms->enc_aes.key_url);
        free(ms);
    }
}

void media_playlist_cleanup(hls_media_playlist_t *me)
{
    struct hls_media_segment *ms = me->first_media_segment;
    free(me->source);
    free(me->orig_url);
    free(me->url);
    free(me->audio_grp);
    free(me->resolution);
    free(me->codecs);
    free(me->enc_aes.key_url);

    while(ms){
        me->first_media_segment = ms->next;
        media_segment_cleanup(ms);
        ms = me->first_media_segment;
    }
    assert(me->first_media_segment == NULL);
    me->last_media_segment = NULL;
}

static void audio_cleanup(hls_audio_t *audio)
{
    free(audio->url);
    free(audio->grp_id);
    free(audio->lang);
    free(audio->name);
}

void master_playlist_cleanup(struct hls_master_playlist *ma)
{
    hls_media_playlist_t *me = ma->media_playlist;
    while (me) {
        hls_media_playlist_t *ptr = me;
        me = me->next;
        media_playlist_cleanup(ptr);
        free(ptr);
    }

    hls_audio_t *audio = ma->audio;
    while (audio) {
        hls_audio_t *ptr = audio;
        audio = audio->next;
        audio_cleanup(ptr);
        free(ptr);
    }

    free(ma->source);
    free(ma->orig_url);
    free(ma->url);
}

int fill_key_value(struct enc_aes128 *es)
{
    /* temporary we will create cache with keys url here
     * this will make this function thread unsafe but at now
     * it is not problem because it is used only from one thread
     *
     * last allocation of cache_key_url will not be free
     * but this is not big problem since this code is run
     * as standalone process
     *(system will free all memory allocated by process at it exit).
     *
     * But this must be fixed for clear valgrind memory leak detection.
     */
    static char cache_key_value[KEYLEN] = "";
    static char *cache_key_url = NULL;

    if (es && es->key_url)
    {
        if (cache_key_url && 0 == strcmp(cache_key_url, es->key_url))
        {
            memcpy(es->key_value, cache_key_value, KEYLEN);
        }
        else
        {
            char *key_url = NULL;
            char *key_value = NULL;
            size_t size = 0;
            long http_code = 0;

            if (NULL != hls_args.key_uri_replace_old && \
                NULL != hls_args.key_uri_replace_new && \
                '\0' != hls_args.key_uri_replace_old[0]) {
                key_url = repl_str(es->key_url, hls_args.key_uri_replace_old, hls_args.key_uri_replace_new);
            } else {
                key_url = es->key_url;
            }

            http_code = get_hls_data_from_url(key_url, &key_value, &size, BINKEY, NULL);
            if (es->key_url != key_url) {
                free(key_url);
            }

            if (http_code != 200 || size == 0) {
                MSG_ERROR("Getting key-file [%s] failed http_code[%d].\n", es->key_url, http_code);
                return 1;
            }

            memcpy(es->key_value, key_value, KEYLEN);
            free(key_value);

            free(cache_key_url);
            cache_key_url = strdup(es->key_url);
            memcpy(cache_key_value, es->key_value, KEYLEN);
        }

        free(es->key_url);
        es->key_url = NULL;
    }

    return 0;
}
