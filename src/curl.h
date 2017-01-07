#ifndef __HLS_DownLoad__curl__
#define __HLS_DownLoad__curl__

#include <stdbool.h>

#define STRING 0x0001
#define BINKEY 0x0002
#define BINARY 0x0003

#define USER_AGENT "Mozilla/5.0 (iPad; CPU OS 6_0 like Mac OS X) " \
                   "AppleWebKit/536.26 (KHTML, like Gecko) Version/6.0 " \
                   "Mobile/10A5355d Safari/8536.25"

void * init_http_session(void);
void * set_user_agent_http_session(void *ptr_session, const char *user_agent);
void * add_custom_header_http_session(void *ptr_session, const char *header);
long get_data_from_url_with_session(void **session, char **url, char **out, size_t *size, int type, bool update_url);
void clean_http_session(void *session);

#endif /* defined(__HLS_DownLoad__curl__) */
