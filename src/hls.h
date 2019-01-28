#ifndef __HLS_DownLoad__hls__
#define __HLS_DownLoad__hls__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "misc.h"

#define MASTER_PLAYLIST 0
#define MEDIA_PLAYLIST 1

#define ENC_AES_SAMPLE 0x02
#define ENC_AES128 0x01
#define ENC_NONE 0x00

#define KEYLEN 16

#define HLSDL_MIN_REFRESH_DELAY_SEC    0
#define HLSDL_MAX_REFRESH_DELAY_SEC    5
#define HLSDL_LIVE_START_OFFSET_SEC  120
#define HLSDL_MAX_RETRIES             30
#define HLSDL_OPEN_MAX_RETRIES         3

typedef struct enc_aes128 {
    bool iv_is_static;
    uint8_t iv_value[KEYLEN];
    uint8_t key_value[KEYLEN];
    char *key_url;
} enc_aes128_t;

typedef struct hls_media_segment {
    char *url;
    int64_t offset;
    int64_t size;
    int sequence_number;
    uint64_t duration_ms;
    struct enc_aes128 enc_aes;
    struct hls_media_segment *next;
    struct hls_media_segment *prev;
} hls_media_segment_t;

typedef struct hls_media_playlist {
    char *orig_url;
    char *url;
    char *source;
    char *audio_grp;
    char *resolution;
    char *codecs;
    unsigned int bitrate;
    uint64_t target_duration_ms;
    uint64_t total_duration_ms;
    bool is_endlist;
    bool encryption;
    int encryptiontype;
    int first_media_sequence;
    int last_media_sequence;
    struct hls_media_segment *first_media_segment;
    struct hls_media_segment *last_media_segment;
    struct enc_aes128 enc_aes;

    struct hls_media_playlist *next;
} hls_media_playlist_t;

typedef struct hls_audio {
    char *url;
    char *grp_id;
    char *lang;
    char *name;
    bool is_default;
    struct hls_audio *next;
} hls_audio_t;

typedef struct hls_master_playlist {
    char *orig_url;
    char *url;
    char *source;
    hls_media_playlist_t *media_playlist;
    hls_audio_t *audio;
} hls_master_playlist_t;

typedef struct hls_playlist_updater_params {
    hls_media_playlist_t *media_playlist;
    void *media_playlist_mtx;
    void *media_playlist_refresh_cond;
    void *media_playlist_empty_cond;
} hls_playlist_updater_params;


int get_playlist_type(char *source);
int handle_hls_master_playlist(struct hls_master_playlist *ma);
int handle_hls_media_playlist(hls_media_playlist_t *me);
int download_live_hls(write_ctx_t *ctx, hls_media_playlist_t *me);
int download_hls(write_ctx_t *ctx, hls_media_playlist_t *me, hls_media_playlist_t *me_audio);
int print_enc_keys(hls_media_playlist_t *me);
void print_hls_master_playlist(struct hls_master_playlist *ma);
void media_playlist_cleanup(hls_media_playlist_t *me);
void master_playlist_cleanup(struct hls_master_playlist *ma);
void media_segment_cleanup(struct hls_media_segment *ms);
void add_media_segment(hls_media_playlist_t *me);
int fill_key_value(struct enc_aes128 *es);

long get_hls_data_from_url(char *url, char **out, size_t *size, int type, char **new_url);

#ifdef __cplusplus
}
#endif

#endif /* defined(__HLS_DownLoad__hls__) */
