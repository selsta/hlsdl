#ifndef __HLS_DownLoad__curl__
#define __HLS_DownLoad__curl__

#define MASTER_PLAYLIST 0
#define MEDIA_PLAYLIST 1

int get_source_from_url(char *url, char **source);

int get_hex_from_url(char *url, char hex[]);

int dl_file(char *url, char *name);

#endif /* defined(__HLS_DownLoad__curl__) */