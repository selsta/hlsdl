#ifndef __HLS_DownLoad__curl__
#define __HLS_DownLoad__curl__

#define MASTER_PLAYLIST 0
#define MEDIA_PLAYLIST 1

/*******************************************************
 * USAGE: Pass the url and the adress of an pointer
 * Return values:
 *     0 - success
 *     1 - error
 * Uses malloc() so free after usage
 ******************************************************/
int get_source_from_url(const char *url, char **source);
int dl_file(char *url, char *name);

#endif /* defined(__HLS_DownLoad__curl__) */
