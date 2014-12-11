#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "curl.h"
#include "hls.h"

int main(int argc, const char * argv[]) {
    
    char URL[200];
    
    if (argc >= 2) {
        strcpy(URL, argv[1]);
    } else {
        fprintf(stdout, "No file passed");
        return 0;
    }
    
    curl_global_init(CURL_GLOBAL_ALL);
    char *hlsfile_source;
    struct hls_media_playlist media_playlist;
    
    if (get_source_from_url(URL, &hlsfile_source)) {
        fprintf(stderr, "Error connecting to server");
        return 1;
    }
    
    int playlist_type = get_playlist_type(hlsfile_source);

    if (playlist_type == MASTER_PLAYLIST) {
        struct hls_master_playlist master_playlist;
        master_playlist.source = hlsfile_source;
        master_playlist.url = strdup(URL);
        if(handle_hls_master_playlist(&master_playlist)) {
            return 1;
        }
        print_hls_master_playlist(&master_playlist);
        int user_input;
        printf("Which Quality should be downloaded? ");
        scanf("%d", &user_input);
        media_playlist = master_playlist.media_playlist[user_input];
        master_playlist_cleanup(&master_playlist);
    } else if (playlist_type == MEDIA_PLAYLIST) {
        media_playlist.bitrate = 0;
        media_playlist.url = strdup(URL);
    } else {
        return 1;
    }
    
    if (handle_hls_media_playlist(&media_playlist)) {
        return 1;
    }
    
    printf("Media Playlist parsed, downloading now!\n");
    
    if (download_hls(&media_playlist)) {
        return 1;
    }
    
    printf("Downloaded out.ts to your current directory. Cleaning up.\n");
    media_playlist_cleanup(&media_playlist);
    curl_global_cleanup();
    return 0;
}
