#ifndef __HLS_DownLoad__misc__
#define __HLS_DownLoad__misc__

#include <stdint.h>

#define STRLEN_BTS(LEN) (((LEN) * 2) + 2)

#define MAX_FILENAME_LEN 256
#define MAX_URL_LEN 2048
#define HLSDL_MAX_NUM_OF_CUSTOM_HEADERS 256

struct ByteBuffer {
    uint8_t *data;
    int len;
    int pos;
};

struct hls_args {
    int loglevel;
    int use_best;
    int skip_encryption;
    int force_overwrite;
    int dump_ts_urls;
    int dump_dec_cmd;
    int live_start_offset_sec;
    int refresh_delay_sec;
    int segment_download_retries;
    int open_max_retries;
    char *filename;
    char *url;
    char *audio_url;
    char *user_agent;
    char *proxy_uri;
    char *(custom_headers[HLSDL_MAX_NUM_OF_CUSTOM_HEADERS]);
    char *key_uri_replace_old;
    char *key_uri_replace_new;
};

static const uint8_t h264_nal_init[3] = {0x00, 0x00, 0x01};

// start code emulation prevention table
static const uint8_t h264_scep_search[4][4] =
                  {{0x00, 0x00, 0x03, 0x00},
                   {0x00, 0x00, 0x03, 0x01},
                   {0x00, 0x00, 0x03, 0x02},
                   {0x00, 0x00, 0x03, 0x03}};

static const uint8_t h264_scep_replace[4][3] =
                  {{0x00, 0x00, 0x00},
                   {0x00, 0x00, 0x01},
                   {0x00, 0x00, 0x02},
                   {0x00, 0x00, 0x03}};

struct hls_args hls_args;

int read_packet(void *opaque, uint8_t *buf, int buf_size);
int64_t seek(void *opaque, int64_t offset, int whence);
int bytes_remaining(uint8_t *pos, uint8_t *end);
int str_to_bin(uint8_t *data, char *hexstring, int len);
int parse_argv(int argc, char * const argv[]);

char *repl_str(const char *str, const char *from, const char *to);

#endif /* defined(__HLS_DownLoad__misc__) */
