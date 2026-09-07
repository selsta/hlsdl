#ifndef __hlsdl__misc__
#define __hlsdl__misc__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define STRLEN_BTS(LEN) (((LEN) * 2) + 2)
#define MAX(a,b) ((a) > (b) ? (a) : (b))

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
    int maxwidth;
    int maxheight;
    char *audiolang;
    int skip_encryption;
    bool force_overwrite;
    bool force_ignoredrm;
    bool dump_ts_urls;
    bool dump_dec_cmd;
    int live_start_offset_sec;
    int live_duration_sec;
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
    uint8_t *key_value;
    char *cookie_file;
    void *cookie_file_mutex;
    bool accept_partial_content;
    bool resume;
};

extern struct hls_args hls_args;

/* VOD resume: a small sidecar file <output>.hlsdl.resume records how far a
 * download got so an interrupted transfer can carry on instead of starting
 * over. */
typedef struct hls_resume_state {
    char     out_filename[MAX_FILENAME_LEN];
    int      total;   /* media segments in the playlist (EXT-X-MAP excluded) */
    int      done;    /* media segments already written */
    int64_t  bytes;   /* bytes already in the output file */
    int      map;     /* 1 if the playlist carries an EXT-X-MAP init segment */
    uint64_t fingerprint;   /* hash of the segment list (url + byte range per
                               segment, video and audio): a different quality
                               variant usually has the same segment count, so
                               this is what actually detects a playlist swap */
} hls_resume_state_t;

/* Called only when -R is set. done/bytes are 0 for a fresh start (no sidecar,
 * or it does not match this playlist), non-zero to resume. Returns NULL only
 * on allocation failure; callers treat that as "do not resume". */
hls_resume_state_t *resume_load(const char *out_filename, int total, int has_map, uint64_t fingerprint);
/* Atomically rewrite the sidecar with the given progress. */
void resume_save(hls_resume_state_t *rs, int done, int64_t bytes);
/* Remove the sidecar (call on a completed download). */
void resume_clear(const char *out_filename);

int str_to_bin(uint8_t *data, char *hexstring, int len);
int parse_argv(int argc, char * const argv[]);

char *repl_str(const char *str, const char *from, const char *to);

#ifdef __cplusplus
}
#endif

#endif /* defined(__hlsdl__misc__) */
