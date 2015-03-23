#ifndef __HLS_DownLoad__curl__
#define __HLS_DownLoad__curl__

#define USER_AGENT "Mozilla/5.0 (iPad; CPU OS 6_0 like Mac OS X) " \
                   "AppleWebKit/536.26 (KHTML, like Gecko) Version/6.0 " \
                   "Mobile/10A5355d Safari/8536.25"


int get_source_from_url(char *url, char **source);

int get_hex_from_url(char *url, char hex[]);

int dl_file(char *url, char *name);

#endif /* defined(__HLS_DownLoad__curl__) */