#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "curl.h"
#include "hls.h"

int main(int argc, const char * argv[]) {
    //const char* const URL = "http://stream.gravlab.net/003119/20130506_out/Frequency_2011_21_HD.m3u8";
    //const char* const URL = "http://stream.gravlab.net/003119/20130506_out/Frequency_2011_21_HD-110k.m3u8";
    //const char* const URL = "http://184.72.239.149/vod/smil:bigbuckbunnyiphone.smil/index.m3u8";
    //const char* const URL = "http://demo.unified-streaming.com/video/oceans/oceans_aes.ism/oceans_aes.m3u8";
    //const char* const URL = "http://wpc.866f.edgecastcdn.net/03866F/greyback/yourtrinity/jw-online-giving_,2500,1500,580,265,.mp4.m3u8";
    //const char* const URL = "http://download.oracle.com/otndocs/products/javafx/JavaRap/prog_index.m3u8";
    //const char* const URL = "http://vas.sim-technik.de/video/playlist.m3u8?ClipID=3587262";
    //const char* const URL = "http://cdn.theoplayer.com/video/big_buck_bunny_encrypted/stream-800/index.m3u8";
    //const char* const URL = "http://tv4play-i.akamaihd.net/i/mp4root/2009-04-09/A1F4PXKK_WEBBMETALLICANY_737866_,T6MP43,T6MP48,_.mp4.csmil/index_1_av.m3u8?e=b471643725c47acd&";
    //const char* const URL = "http://demo.unified-streaming.com/video/caminandes/caminandes-sample-aes.ism/caminandes-sample-aes.m3u8";
    //const char* const URL = "http://demo.unified-streaming.com/video/caminandes/caminandes-pr-aes.ism/caminandes-pr-aes.m3u8";
    
    char URL[200];
    
    strcpy(URL, argv[1]);
    
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
        handle_hls_master_playlist(&master_playlist);
        print_hls_master_playlist(&master_playlist);
        int user_input;
        printf("Which Quality should be downloaded? ");
        scanf("%d", &user_input);
        media_playlist = master_playlist.media_playlist[user_input];
    } else if (playlist_type == MEDIA_PLAYLIST) {
        media_playlist.bitrate = 0;
        media_playlist.url = strdup(URL);
    } else {
        return 1;
    }
    handle_hls_media_playlist(&media_playlist);
    download_hls(&media_playlist);
    
    free(hlsfile_source);
    curl_global_cleanup();
    return 0;
}
