#ifndef __HLS_DownLoad__curl__
#define __HLS_DownLoad__curl__

#define MASTER_PLAYLIST 0
#define MEDIA_PLAYLIST 1

/*******************************************************
 * USAGE: Pass the url and the adress of an charpointer
 *        Use to get the sourcecode of an url
 *
 * Return values:
 *     0 - success
 *     1 - error
 * Uses strdup() so free after usage
 ******************************************************/
int get_source_from_url(char *url, char **source);

/*******************************************************
 * USAGE: Pass the url and the adress of an pointer
 *        Use to get the hexvalue of an website.
 * Return values:
 *     0 - success
 *     1 - error
 * Uses malloc() so free after usage
 ******************************************************/
int get_hex_from_url(char *url, char hex[]);

int dl_file(char *url, char *name);

#endif /* defined(__HLS_DownLoad__curl__) */