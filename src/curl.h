#ifndef __HLS_DownLoad__curl__
#define __HLS_DownLoad__curl__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define STRING 0x0001
#define BINKEY 0x0002
#define BINARY 0x0003

#define USER_AGENT "Mozilla/5.0 (iPad; CPU OS 6_0 like Mac OS X) " \
                   "AppleWebKit/536.26 (KHTML, like Gecko) Version/6.0 " \
                   "Mobile/10A5355d Safari/8536.25"

void * init_http_session(void);
void * set_user_agent_http_session(void *ptr_session, const char *user_agent);
void * set_proxy_uri_http_session(void *ptr_session, const char *proxy_uri);
void * set_cookie_file_session(void *ptr_session, const char *cookie_file, void *cookie_file_mutex);
void * set_timeout_session(void *ptr_session, const long speed_limit, const long speed_time);
void add_custom_header_http_session(void *ptr_session, const char *header);
long get_data_from_url_with_session(void **session, char *url, char **out, size_t *size, int type, char **new_url, const char *range);
void clean_http_session(void *session);
size_t get_data_from_url(char *url, char **str, uint8_t **bin, int type, char **new_url);
void set_fresh_connect_http_session(void *ptr_session, long val);

#ifdef __cplusplus
}
#endif

#endif /* defined(__HLS_DownLoad__curl__) */
