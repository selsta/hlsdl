#ifndef __HLS_DownLoad__misc__
#define __HLS_DownLoad__misc__

#include <stdint.h>

#define STRLEN_BTS(LEN) (((LEN) * 2) + 1)

#define MAX_FILENAME_LEN 256
#define MAX_URL_LEN 2048

struct ByteBuffer {
    uint8_t *data;
    int len;
    int pos;
};

struct hls_args {
    int loglevel;
    int use_best;
    int url_passed;
    int skip_encryption;
    int custom_filename;
    int force_overwrite;
    int dump_ts_urls;
    int dump_dec_cmd;
    char filename[MAX_FILENAME_LEN];
    char url[MAX_URL_LEN];
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
int parse_argv(int argc, const char *argv[]);

#endif /* defined(__HLS_DownLoad__misc__) */
