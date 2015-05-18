#ifndef __HLS_DownLoad__hls__
#define __HLS_DownLoad__hls__

#include <stdbool.h>

#define MASTER_PLAYLIST 0
#define MEDIA_PLAYLIST 1

#define ENC_AES_SAMPLE 0x02
#define ENC_AES128 0x01
#define ENC_NONE 0x00

#define KEYLEN 16

struct enc_aes128 {
    bool iv_is_static;
    uint8_t iv_value[KEYLEN];
    uint8_t key_value[KEYLEN];
};

struct hls_media_segment {
    char *url;
    int sequence_number;
    struct enc_aes128 enc_aes;
};

struct hls_media_playlist {
    char *url;
    char *source;
    unsigned int bitrate;
    bool encryption;
    int encryptiontype;
    int count;
    struct hls_media_segment *media_segment;
    struct enc_aes128 enc_aes;
};

struct hls_master_playlist {
    char *url;
    char *source;
    int count;
    struct hls_media_playlist *media_playlist;
};

int get_playlist_type(char *source);
int handle_hls_master_playlist(struct hls_master_playlist *ma);
int handle_hls_media_playlist(struct hls_media_playlist *me);
int download_hls(struct hls_media_playlist *me);
int print_enc_keys(struct hls_media_playlist *me);
void print_hls_master_playlist(struct hls_master_playlist *ma);
void media_playlist_cleanup(struct hls_media_playlist *me);
void master_playlist_cleanup(struct hls_master_playlist *ma);
#endif /* defined(__HLS_DownLoad__hls__) */
