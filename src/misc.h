#ifndef __HLS_DownLoad__misc__
#define __HLS_DownLoad__misc__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define STRLEN_BTS(LEN) (((LEN) * 2) + 2)

#define MAX_FILENAME_LEN 256
#define MAX_URL_LEN 2048
#define HLSDL_MAX_NUM_OF_CUSTOM_HEADERS 256

typedef struct write_ctx {
    size_t (*write) ( const uint8_t *data, size_t len, void *opaque);
    void *opaque;
} write_ctx_t;

typedef struct ByteBuffer {
    uint8_t *data;
    int len;
    int pos;
} ByteBuffer_t;

struct hls_args {
    int loglevel;
    bool use_best;
    int skip_encryption;
    bool force_overwrite;
    bool dump_ts_urls;
    bool dump_dec_cmd;
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
    char *cookie_file;
    void *cookie_file_mutex;
    bool accept_partial_content;
};

struct hls_args hls_args;

int str_to_bin(uint8_t *data, char *hexstring, int len);
int parse_argv(int argc, char * const argv[]);

char *repl_str(const char *str, const char *from, const char *to);

#ifdef __cplusplus
}
#endif

#endif /* defined(__HLS_DownLoad__misc__) */
