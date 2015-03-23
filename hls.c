#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "curl.h"
#include "hls.h"
#include "msg.h"
#include "misc.h"

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
    size_t max_length = strlen(*url) + strlen(baseurl) + 10;
    
    if (!strncmp(*url, "http://", 7) || !strncmp(*url, "https://", 8)) {
        return 0; //absolute url, nothing to do
    }
    
    else if (**url == '/') {
        char *domain = (char*)malloc(max_length);
        strcpy(domain, baseurl);
        if (!sscanf(baseurl, "http://%[^/]", domain)) {
            sscanf(baseurl, "https://%[^/]", domain);
        }
        
        char *buffer = (char*)malloc(max_length);
        snprintf(buffer, max_length, "%s%s", domain, *url);
        *url = realloc(*url, strlen(buffer) + 1);
        if (!*url) {
            MSG_ERROR("out of memory");
            exit(1);
        }
        strcpy(*url, buffer);
        free(buffer);
        free(domain);
        return 0;
    }
    
    else {
        // remove ? from baseurl.. so that /../ works
        char *find_questionmark = strchr(baseurl, '?');
        if (find_questionmark) {
            *find_questionmark = '\0';
        }
        
        char *buffer = (char*)malloc(max_length);
        snprintf(buffer, max_length, "%s/../%s", baseurl, *url);
        *url = realloc(*url, strlen(buffer) + 1);
        if (!*url) {
            MSG_ERROR("out of memory");
            exit(1);
        }
        strcpy(*url, buffer);
        free(buffer);
        return 0;
    }
}

static int parse_playlist_tag(struct hls_media_playlist *me, char *tag)
{
    if (!strncmp(tag, "#EXT-X-KEY:METHOD=AES-128", 25)) {
        me->encryption = true;
        me->encryptiontype = ENC_AES128;
        me->enc_aes.iv_is_static = false;
        
        char *link_to_key = (char*)malloc(strlen(tag) + strlen(me->url) + 10);
        char iv_key[33];
        
        if (sscanf(tag, "#EXT-X-KEY:METHOD=AES-128,URI=\"%[^\"]\",IV=0x%s", link_to_key, iv_key) == 2) {
            strcpy(me->enc_aes.iv_value, iv_key);
            me->enc_aes.iv_is_static = true;
        }
        
        extend_url(&link_to_key, me->url);
        
        char *decrypt = (char*)malloc(33);
        if (get_data_from_url(link_to_key, &decrypt, HEXSTR)) {
            free(link_to_key);
            return 1;
        }
        
        strcpy(me->enc_aes.key_value, decrypt);
        free(link_to_key);
        free(decrypt);
    }
    
    if (!strncmp(tag, "#EXT-X-KEY:METHOD=SAMPLE-AES", 28)) {
        me->encryption = true;
        me->encryptiontype = ENC_AES_SAMPLE;
        me->enc_aes.iv_is_static = false;
        
        char *link_to_key = (char*)malloc(strlen(tag) + strlen(me->url) + 10);
        char iv_key[33];
        
        if (sscanf(tag, "#EXT-X-KEY:METHOD=SAMPLE-AES,URI=\"%[^\"]\",IV=0x%s", link_to_key, iv_key) == 2) {
            strcpy(me->enc_aes.iv_value, iv_key);
            me->enc_aes.iv_is_static = true;
        }
        
        extend_url(&link_to_key, me->url);
        
        char *decrypt = (char*)malloc(33);
        if (get_data_from_url(link_to_key, &decrypt, HEXSTR)) {
            free(link_to_key);
            return 1;
        }
        strcpy(me->enc_aes.key_value, decrypt);
        free(link_to_key);
        free(decrypt);
    }
    return 0;
}

static int get_link_count(char *src)
{
    int linkcount = 0;
    
    while ((src = (strchr(src, 10)))) {
        src++;
        if (*src == '#') {
            continue;
        }
        if (*src == '\0') {
            break;
        }
        linkcount++;
    }
    
    return linkcount;
}

static int media_playlist_get_media_sequence(char *source)
{
    int j = 0;
    char *p_media_sequence = strstr(source, "#EXT-X-MEDIA-SEQUENCE:");
    
    if (p_media_sequence) {
        if (sscanf(p_media_sequence, "#EXT-X-MEDIA-SEQUENCE:%d", &j) != 1) {
            MSG_ERROR("Could not read EXT-X-MEDIA-SEQUENCE\n");
            return 0;
        }
    }
    return j;
}

static int media_playlist_get_links(struct hls_media_playlist *me)
{
    int media_squence_start_val = media_playlist_get_media_sequence(me->source);
    struct hls_media_segment *ms = me->media_segment;
    
    /* Initialze the Strings */
    for (int i = 0; i < me->count; i++) {
        ms[i].url = (char*)malloc(strlen(me->source));
    }
    
    char *src = me->source;
    for (int i = 0; i < me->count; i++) {
        while ((src = (strchr(src, 10)))) {
            src++;
            if (*src == 10) {
                continue;
            }
            if (*src == '#') {
                parse_playlist_tag(me, src);
                continue;
            }
            if (*src == '\0') {
                goto finish;
            }
            if (sscanf(src, "%[^\n]", ms[i].url) == 1) {
                ms[i].sequence_number = i + media_squence_start_val;
                if (me->encryptiontype == ENC_AES128 || me->encryptiontype == ENC_AES_SAMPLE) {
                    strcpy(ms[i].enc_aes.key_value, me->enc_aes.key_value);
                    if (me->enc_aes.iv_is_static == false) {
                        snprintf(ms[i].enc_aes.iv_value, 33, "%032x\n", ms[i].sequence_number);
                    }
                }
                break;
            }
        }
    }
    
finish:
    /* Extend individual urls */
    for (int i = 0; i < me->count; i++) {
        extend_url(&ms[i].url, me->url);
    }
    return 0;
}

int handle_hls_media_playlist(struct hls_media_playlist *me)
{
    me->encryption = false;
    me->encryptiontype = ENC_NONE;
    
    get_data_from_url(me->url, &me->source, STRING);
    
    if (get_playlist_type(me->source) != MEDIA_PLAYLIST) {
        return 1;
    }
    me->count = get_link_count(me->source);
    me->media_segment = (struct hls_media_segment*)malloc(sizeof(struct hls_media_segment) * me->count);
    
    if (media_playlist_get_links(me)) {
        MSG_ERROR("Could not parse links. Exiting.\n");
        return 1;
    }
    return 0;
}

static int master_playlist_get_bitrate(struct hls_master_playlist *ma)
{
    struct hls_media_playlist *me = ma->media_playlist;
    
    char *src = ma->source;
    
    for (int i = 0; i < ma->count; i++) {
        if ((src = strstr(src, "BANDWIDTH="))) {
            if ((sscanf(src, "BANDWIDTH=%u", &me[i].bitrate)) == 1) {
                src++;
                continue;
            }
        }
        me[i].bitrate = 0;
    }
    return 0;
}

static int master_playlist_get_links(struct hls_master_playlist *ma)
{
    struct hls_media_playlist *me = ma->media_playlist;
    
    /* Initialze the Strings */
    for (int i = 0; i < ma->count; i++) {
        if ((me[i].url = (char*)malloc(strlen(ma->source))) == NULL) {
            MSG_ERROR("out of memory.\n");
            return 1;
        }
    }
    
    /* Get urls */
    char *src = ma->source;
    for (int i = 0; i < ma->count; i++) {
        while ((src = (strchr(src, 10)))) {
            src++;
            if (*src == '#' || *src == 10) {
                continue;
            }
            if (*src == '\0') {
                goto finish;
            }
            if (sscanf(src, "%[^\n]", me[i].url) == 1) {
                break;
            }
        }
    }
    
finish:
    /* Extend individual urls */
    for (int i = 0; i < ma->count; i++) {
        extend_url(&me[i].url, ma->url);
    }
    return 0;
}

int handle_hls_master_playlist(struct hls_master_playlist *ma)
{
    ma->count = get_link_count(ma->source);
    ma->media_playlist = (struct hls_media_playlist*)malloc(sizeof(struct hls_media_playlist) * ma->count);
    if (master_playlist_get_links(ma)) {
        MSG_ERROR("Could not parse links. Exiting.\n");
        return 1;
    }
    if (master_playlist_get_bitrate(ma)) {
        MSG_ERROR("Could not parse bitrate. Exiting.\n");
        return 1;
    }
    return 0;
}

void print_hls_master_playlist(struct hls_master_playlist *ma)
{
    int i;
    MSG_VERBOSE("Found %d Qualitys\n\n", ma->count);
    for (i = 0; i < ma->count; i++) {
        MSG_PRINT("%d: Bandwidth: %d\n", i, ma->media_playlist[i].bitrate);
    }
}

int download_hls(struct hls_media_playlist *me)
{
    MSG_VERBOSE("%d Segments found.\n", me->count);
    
    char filename[256];
    char *tmp_part = get_rndstring(10);
    char tmp_part_full[10 + 7 + 1];
    snprintf(tmp_part_full, 18, "%s.tmp.ts", tmp_part);
    
    if (hls_args.custom_filename) {
        strcpy(filename, hls_args.filename);
    } else {
        strcpy(filename, "000_hls_output.ts");
    }
    
    if (access(filename, F_OK ) != -1) {
        if (hls_args.force_overwrite) {
            if (remove(filename) != 0) {
                MSG_ERROR("Error overwriting file");
                exit(1);
            }
        } else {
            char userchoice;
            MSG_PRINT("File already exists. Overwrite? (y/n) ");
            scanf("\n%c", &userchoice);
            if (userchoice == 'y') {
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
    
    for (int i = 0; i < me->count; i++) {
        MSG_VERBOSE("\rDownloading Segment %d/%d", i + 1, me->count);
        fflush(stdout);
        dl_file(me->media_segment[i].url, tmp_part_full);
        if (me->encryption == true) {
            if (me->encryptiontype == ENC_AES128) {
                system_va("openssl aes-128-cbc -d -in %s -out %s.dec.ts -K %s -iv %s ; mv %s.dec.ts %s",
                          tmp_part_full, tmp_part, me->media_segment[i].enc_aes.key_value,
                          me->media_segment[i].enc_aes.iv_value, tmp_part, tmp_part_full);
            }
        }
        system_va("cat %s >> %s", tmp_part_full, filename);
        if (remove(tmp_part_full) != 0) {
            MSG_ERROR("Error deleting tmp file.");
            exit(1);
        }
    }
    
    MSG_VERBOSE("\n");
    MSG_VERBOSE("Downloaded %s to your current directory. Cleaning up.\n", filename);
    
    free(tmp_part);
    return 0;
}

int print_hls_dec_cmd(struct hls_media_playlist *me)
{
    char filename[256];
    char *tmp_part = get_rndstring(10);
    char tmp_part_full[10 + 7 + 1];
    snprintf(tmp_part_full, 18, "%s.tmp.ts", tmp_part);
    
    if (hls_args.custom_filename) {
        strcpy(filename, hls_args.filename);
    } else {
        strcpy(filename, "000_hls_output.ts");
    }
    
    for (int i = 0; i < me->count; i++) {
        if (me->encryption == true) {
            if (me->encryptiontype == ENC_AES128) {
                MSG_PRINT("openssl aes-128-cbc -d -in %s -out %s.dec.ts -K %s -iv %s ; mv %s.dec.ts %s\n",
                          tmp_part_full, tmp_part, me->media_segment[i].enc_aes.key_value,
                          me->media_segment[i].enc_aes.iv_value, tmp_part, tmp_part_full);
            }
            if (me->encryptiontype == ENC_AES_SAMPLE) {
                MSG_PRINT("openssl aes-128-cbc -d -in %s -out %s.dec.ts -K %s -iv %s ; mv %s.dec.ts %s\n",
                          tmp_part_full, tmp_part, me->media_segment[i].enc_aes.key_value,
                          me->media_segment[i].enc_aes.iv_value, tmp_part, tmp_part_full);
            }
        }
    }
    free(tmp_part);
    return 0;
}


void media_playlist_cleanup(struct hls_media_playlist *me)
{
    free(me->source);
    free(me->url);
    for (int i = 0; i < me->count; i++) {
        free(me->media_segment[i].url);
    }
    free(me->media_segment);
}

void master_playlist_cleanup(struct hls_master_playlist *ma)
{
    free(ma->source);
    free(ma->url);
    free(ma->media_playlist);
}
