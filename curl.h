#ifndef __HLS_DownLoad__curl__
#define __HLS_DownLoad__curl__

#define STRING 0x0001
#define HEXSTR 0x0002
#define BINARY 0x0003

#define USER_AGENT "Mozilla/5.0 (iPad; CPU OS 6_0 like Mac OS X) " \
                   "AppleWebKit/536.26 (KHTML, like Gecko) Version/6.0 " \
                   "Mobile/10A5355d Safari/8536.25"


int get_data_from_url(char *url, char **data, int type);
int dl_file(char *url, char *name);

#endif /* defined(__HLS_DownLoad__curl__) */