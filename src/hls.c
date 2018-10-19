
#if defined(WITH_FFMPEG) && WITH_FFMPEG 
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

#include <sys/prctl.h>

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
    
    if ((sscanf(tag, "#EXT-X-KEY:METHOD=AES-128,URI=\"%[^\"]\",IV=0%c%s", link_to_key, &sep, iv_str) > 0 ||
         sscanf(tag, "#EXT-X-KEY:METHOD=SAMPLE-AES,URI=\"%[^\"]\",IV=0%c%s", link_to_key, &sep, iv_str) > 0))
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
                
                ms->sequence_number = i + me->first_media_sequence;
                
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
    while (*ptr != '=' && *ptr != '\0') ++ptr;
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
        while (*ptr != end_val_marker && *ptr != '\0') ++ptr;
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


#if defined(WITH_FFMPEG) && WITH_FFMPEG 
static int decrypt_sample_aes(struct hls_media_segment *s, struct ByteBuffer *buf)
{
    // SAMPLE AES works by encrypting small segments (blocks).
    // Blocks have a size of 16 bytes.
    // Only 1 in 10 blocks of the video stream are encrypted,
    // while every single block of the audio stream is encrypted.
    int audio_index = -1, video_index = -1;
    static int audio_sample_rate = 0, audio_frame_size = 0;

    struct ByteBuffer input_buffer = {buf->data, buf->len, 0};

    AVInputFormat *ifmt = av_find_input_format("mpegts");
    uint8_t *input_avbuff = av_malloc(4096);
    AVIOContext *input_io_ctx = avio_alloc_context(input_avbuff, 4096, 0, &input_buffer,
                                                   read_packet, NULL, seek);
    AVFormatContext *ifmt_ctx = avformat_alloc_context();
    ifmt_ctx->pb = input_io_ctx;

    AVOutputFormat *ofmt = av_guess_format("mpegts", NULL, NULL);

    AVFormatContext *ofmt_ctx = avformat_alloc_context();
    ofmt_ctx->oformat = ofmt;
    
    if (avformat_open_input(&ifmt_ctx, "file.h264", ifmt, NULL) != 0) {
        MSG_ERROR("Opening input file failed\n");
    }

    // avformat_find_stream_info() throws useless warnings because the data is encrypted.
    av_log_set_level(AV_LOG_QUIET);
    avformat_find_stream_info(ifmt_ctx, NULL);
    av_log_set_level(AV_LOG_WARNING);

    for (int i = 0; i < (int)ifmt_ctx->nb_streams; i++) {
        AVCodecContext *in_c = ifmt_ctx->streams[i]->codec;
        if (in_c->codec_type == AVMEDIA_TYPE_AUDIO) {
            avformat_new_stream(ofmt_ctx, in_c->codec);
            avcodec_copy_context(ofmt_ctx->streams[i]->codec, in_c);
            audio_index = i;
            if (s->sequence_number == 1) {
                audio_frame_size = in_c->frame_size;
                audio_sample_rate = in_c->sample_rate;
            }
        } else if (in_c->codec_id == AV_CODEC_ID_H264) {
            avformat_new_stream(ofmt_ctx, in_c->codec);
            avcodec_copy_context(ofmt_ctx->streams[i]->codec, in_c);
            video_index = i;
        }
    }

    if (video_index < 0 || audio_index < 0) {
        MSG_ERROR("Video or Audio missing.");
    }


    // It can happen that only the first segment contains
    // useful sample_rate/frame_size data.
    if (ofmt_ctx->streams[audio_index]->codec->sample_rate == 0) {
        ofmt_ctx->streams[audio_index]->codec->sample_rate = audio_sample_rate;
    }
    if (ofmt_ctx->streams[audio_index]->codec->frame_size == 0) {
        ofmt_ctx->streams[audio_index]->codec->frame_size = audio_frame_size;
    }

    if (avio_open_dyn_buf(&ofmt_ctx->pb) != 0) {
        MSG_ERROR("Could not open output memory stream.\n");
    }

    AVPacket pkt;
    uint8_t packet_iv[16];

    if (avformat_write_header(ofmt_ctx, NULL) != 0) {
        MSG_ERROR("Writing header failed.\n");
    }

    while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == audio_index) {
            // The IV must be reset at the beginning of every packet.
            memcpy(packet_iv, s->enc_aes.iv_value, 16);

            uint8_t *audio_frame = pkt.data;
            uint8_t *p_frame = audio_frame;

            enum AVCodecID cid = ifmt_ctx->streams[audio_index]->codec->codec_id;
            if (cid == AV_CODEC_ID_AAC) {
                // ADTS headers can contain CRC checks.
                // If the CRC check bit is 0, CRC exists.
                //
                // Header (7 or 9 byte) + unencrypted leader (16 bytes)
                p_frame += (p_frame[1] & 0x01) ? 23 : 25;
            } else if (cid == AV_CODEC_ID_AC3) {
                // AC3 Audio is untested. Sample streams welcome.
                //
                // unencrypted leader
                p_frame += 16;
            } else {
                MSG_ERROR("This audio codec is unsupported.\n");
                exit(1);
            }

            while (bytes_remaining(p_frame, (audio_frame + pkt.size)) >= 16 ) {
                uint8_t *dec_tmp = malloc(16);
                fill_key_value(&(s->enc_aes));
                AES128_CBC_decrypt_buffer(dec_tmp, p_frame, 16, s->enc_aes.key_value, packet_iv);

                // CBC requires the unencrypted data from the previous
                // decryption as IV.
                memcpy(packet_iv, p_frame, 16);

                memcpy(p_frame, dec_tmp, 16);
                free(dec_tmp);
                p_frame += 16;
            }
            if (av_interleaved_write_frame(ofmt_ctx, &pkt)) {
                MSG_WARNING("Writing audio frame failed.\n");
            }
        } else if (pkt.stream_index == video_index) {
            // av_return_frame returns whole h264 frames. SAMPLE-AES
            // encrypts NAL units instead of frames. Fortunatly, a frame
            // contains multiple NAL units, so av_return_frame can be used.
            uint8_t *h264_frame = pkt.data;
            uint8_t *p = h264_frame;
            uint8_t *end = p + pkt.size;
            uint8_t *nal_end;

            int block;

            while (p < end) {
                block = 0;
                // Reset the IV at the beginning of every NAL unit.
                memcpy(packet_iv, s->enc_aes.iv_value, 16);
                p = memmem(p, (end - p), h264_nal_init, 3);
                if (!p) {
                    break;
                }
                nal_end = memmem(p + 3, (end - (p + 3)), h264_nal_init, 3);
                if (!nal_end) {
                    nal_end = end;
                }

                if (bytes_remaining(p, nal_end) <= (16 + 3 + 32)) {
                    p = nal_end;
                    continue;
                } else {
                    // Remove the start code emulation prevention.
                    for (int i = 0; i < 4; i++) {
                        uint8_t *tmp = p;
                        while ((tmp = memmem(tmp, nal_end - tmp, h264_scep_search[i], 4))) {
                            memcpy(tmp, h264_scep_replace[i], 3);
                            tmp += 3;
                            end -= 1;
                            memcpy(tmp, tmp + 1, (end - tmp));
                        }
                    }

                    // NALinit bytes + NAL_unit_type_byte + unencrypted_leader
                    p += 3 + 32;

                    nal_end = memmem(p, (end - p), h264_nal_init, 3);
                    if (!nal_end) {
                        nal_end = end;
                    }
                }

                while (bytes_remaining(p, nal_end) > 16) {
                    block++;
                    if (block == 1) {
                        uint8_t *output = malloc(16);
                        fill_key_value(&(s->enc_aes));
                        AES128_CBC_decrypt_buffer(output, p, 16, s->enc_aes.key_value, packet_iv);

                        // CBC requires the unencrypted data from the previous
                        // decryption as IV.
                        memcpy(packet_iv, p, 16);

                        memcpy(p, output, 16);
                        free (output);
                    } else if (block == 10) {
                        block = 0;
                    }
                    p += 16; //blocksize
                }
                p += bytes_remaining(p, nal_end); //unencryted trailer
            }
            pkt.size = bytes_remaining(pkt.data, end);
            if (av_interleaved_write_frame(ofmt_ctx, &pkt)) {
                MSG_WARNING("Writing video frame failed.\n");
            }
        }
#if LIBAVCODEC_VERSION_MAJOR >= 56
        av_packet_unref(&pkt);
#else
        av_free_packet(&pkt);
#endif
    }
    if (av_write_trailer(ofmt_ctx) != 0) {
        MSG_ERROR("Writing trailer failed.\n");
    }

    uint8_t *outbuf;
    buf->len = avio_close_dyn_buf(ofmt_ctx->pb, &outbuf);
    buf->data = realloc(buf->data, buf->len);
    memcpy(buf->data, outbuf, buf->len);
    av_free(outbuf);

    av_free(input_io_ctx->buffer);
    av_free(input_io_ctx);
    input_avbuff = NULL;
    avformat_free_context(ifmt_ctx);
    avformat_free_context(ofmt_ctx);
    return 0;
}

#endif

static int decrypt_aes128(struct hls_media_segment *s, struct ByteBuffer *buf)
{
    // The AES128 method encrypts whole segments.
    // Simply decrypting them is enough.
    uint8_t *db = malloc(buf->len);
    
    fill_key_value(&(s->enc_aes));
    AES128_CBC_decrypt_buffer(db, buf->data, (uint32_t)buf->len,
                              s->enc_aes.key_value, s->enc_aes.iv_value);
    memcpy(buf->data, db, buf->len);
    
    free(db);
    return 0;
}

static void *hls_playlist_update_thread(void *arg)
{
    char threadname[50];
    strncpy(threadname, __func__, sizeof(threadname));
    threadname[49] = '\0';
    prctl(PR_SET_NAME, (unsigned long)&threadname);
    
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
        refresh_delay_s = me->target_duration_ms / 1000;
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
}

int download_live_hls(hls_media_playlist_t *me)
{
    MSG_API("{\"d_t\":\"live\"}\n");
    
    char filename[MAX_FILENAME_LEN];
    if (hls_args.filename) {
        strcpy(filename, hls_args.filename);
    } else {
        strcpy(filename, "000_hls_output.ts");
    }

    if (access(filename, F_OK) != -1) {
        if (hls_args.force_overwrite) {
            if (remove(filename) != 0) {
                MSG_ERROR("Error overwriting file");
                exit(1);
            }
        } else {
            char userchoice = '\0';
            MSG_PRINT("File already exists. Overwrite? (y/n) ");
            if (scanf("\n%c", &userchoice) && userchoice == 'y') {
                if (remove(filename) != 0) {
                    MSG_ERROR("Error overwriting file");
                    exit(1);
                }
            } else {
                MSG_WARNING("Choose a different filename. Exiting.\n");
                exit(0);
            }
        }
    }

    FILE *pFile = fopen(filename, "wb");
    if (pFile == NULL)
    {
        MSG_ERROR("Error can not open output file\n");
        exit(1);
    }

    hls_playlist_updater_params updater_params;
    
    /* declaration synchronization prymitives */
    pthread_mutex_t media_playlist_mtx;
        
    pthread_cond_t  media_playlist_refresh_cond;
    pthread_cond_t  media_playlist_empty_cond;
    
    /* init synchronization prymitives */
    pthread_mutex_init(&media_playlist_mtx, NULL);
    
    pthread_cond_init(&media_playlist_refresh_cond, NULL);
    pthread_cond_init(&media_playlist_empty_cond, NULL);
    
    memset(&updater_params, 0x00, sizeof(updater_params));
    updater_params.media_playlist = me;
    updater_params.media_playlist_mtx = (void *)&media_playlist_mtx;  
    updater_params.media_playlist_refresh_cond = (void *)&media_playlist_refresh_cond;    
    updater_params.media_playlist_empty_cond   = (void *)&media_playlist_empty_cond;    
    
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
            snprintf(range_buff, sizeof(range_buff), "%lld-%lld", ms->offset, ms->offset + ms->size - 1);
            range = range_buff;
        }
        do {
            struct ByteBuffer seg;
            memset(&seg, 0x00, sizeof(seg));
            size_t size = 0;
            long http_code = get_data_from_url_with_session(&session, ms->url, (char **)&(seg.data), &size, BINARY, NULL, range);
            seg.len = (int)size;
            if (http_code != 200 && (range == NULL || http_code != 206)) {
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
#if defined(WITH_FFMPEG) && WITH_FFMPEG 
                decrypt_sample_aes(ms, &seg);
#else
                MSG_API("{\"error_code\":-12, \"error_msg\":\"no_ffmpeg\"}\n");
#endif
            }
            download_size += fwrite(seg.data, 1, seg.len, pFile);
            free(seg.data);
            
            set_fresh_connect_http_session(session, 0);
            
            time_t curRepTime = time(NULL);
            if ((curRepTime - repTime) >= 1) {
                MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%lld}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);
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
    
    MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%lld}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);
    if (session)
    {
        clean_http_session(session);
    }
    fclose(pFile);
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
            snprintf(range_buff, sizeof(range_buff), "%lld-%lld", ms->offset, ms->offset + ms->size - 1);
            range = range_buff;
        }

        memset(seg, 0x00, sizeof(*seg));
        size_t size = 0;
        long http_code = get_data_from_url_with_session(psession, ms->url, (char **)&(seg->data), &size, BINARY, NULL, range);
        seg->len = (int)size;
        if (http_code != 200 && (range == NULL || http_code != 206)) {
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
#if defined(WITH_FFMPEG) && WITH_FFMPEG 
            decrypt_sample_aes(ms, seg);
#else
            MSG_API("{\"error_code\":-12, \"error_msg\":\"no_ffmpeg\"}\n");
#endif
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

int download_hls(hls_media_playlist_t *me, hls_media_playlist_t *me_audio)
{
    MSG_VERBOSE("Downloading segments.\n");
    MSG_API("{\"d_t\":\"vod\"}\n"); // d_t - download type
    MSG_API("{\"t_d\":%u,\"d_d\":0, \"d_s\":0}\n", (uint32_t)(me->total_duration_ms / 1000)); // t_d - total duration, d_d  - download duration, d_s - download size

    char filename[MAX_FILENAME_LEN];
    if (hls_args.filename) {
        strcpy(filename, hls_args.filename);
    } else {
        strcpy(filename, "000_hls_output.ts");
    }

    if (access(filename, F_OK) != -1) {
        if (hls_args.force_overwrite) {
            if (remove(filename) != 0) {
                MSG_ERROR("Error overwriting file");
                exit(1);
            }
        } else {
            char userchoice = '\0';
            MSG_PRINT("File already exists. Overwrite? (y/n) ");
            if (scanf("\n%c", &userchoice) && userchoice == 'y') {
                if (remove(filename) != 0) {
                    MSG_ERROR("Error overwriting file");
                    exit(1);
                }
            } else {
                MSG_WARNING("Choose a different filename. Exiting.\n");
                exit(0);
            }
        }
    }

    FILE *pFile = fopen(filename, "wb");
    if (pFile == NULL)
    {
        MSG_ERROR("Error can not open output file\n");
        exit(1);
    }
    
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
        merge_context.out = pFile;
    }

    while(ms) {

        if (0 != vod_download_segment(&session, me, ms, &seg)) {
            break;
        }

        if (ms_audio) {
            if ( 0 != vod_download_segment(&session, me_audio, ms_audio, &seg_audio)) {
                break;
            }

            download_size += merge_packets(&merge_context, seg.data, seg.len, seg_audio.data, seg_audio.len);

            free(seg_audio.data);
            ms_audio = ms_audio->next;
        } else {
            download_size += fwrite(seg.data, 1, seg.len, pFile);
        }

        free(seg.data);

        time_t curRepTime = time(NULL);
        if ((curRepTime - repTime) >= 1) {
            MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%lld}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);
            repTime = curRepTime;
        }

        downloaded_duration_ms += ms->duration_ms;

        ms = ms->next;
    }
    
    MSG_API("{\"t_d\":%u,\"d_d\":%u,\"d_s\":%lld}\n", (uint32_t)(me->total_duration_ms / 1000), (uint32_t)(downloaded_duration_ms / 1000), download_size);
    
    if (session) {
        clean_http_session(session);
    }
    fclose(pFile);
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
