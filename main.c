#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "curl.h"
#include "hls.h"
#include "msg.h"
#include "misc.h"

int main(int argc, const char * argv[])
{    
    hls_args = (struct hls_args){0};
    hls_args.loglevel = 1;

    if (parse_argv(argc, argv)) {
        MSG_WARNING("No files passed. Exiting.\n");
        return 0;
    }
    
    MSG_DBG("Loglevel: %d\n", hls_args.loglevel);
    
    curl_global_init(CURL_GLOBAL_ALL);
    av_register_all();
    
    char *hlsfile_source;
    struct hls_media_playlist media_playlist;
    
    if (get_data_from_url(hls_args.url, &hlsfile_source, NULL, STRING) == 0) {
        MSG_ERROR("No result from server.\n");
        return 1;
    }
    
    int playlist_type = get_playlist_type(hlsfile_source);
    
    if (playlist_type == MASTER_PLAYLIST) {
        struct hls_master_playlist master_playlist;
        master_playlist.source = hlsfile_source;
        master_playlist.url = strdup(hls_args.url);
        if (handle_hls_master_playlist(&master_playlist)) {
            return 1;
        }
        
        int quality_choice;
        if (hls_args.use_best) {
            int max = 0;
            for (int i = 0; i < master_playlist.count; i++) {
                if (master_playlist.media_playlist[i].bitrate > master_playlist.media_playlist[max].bitrate) {
                    max = i;
                }
            }
            MSG_VERBOSE("Choosing best quality. (Bitrate: %d)\n", master_playlist.media_playlist[max].bitrate);
            quality_choice = max;
        } else {
            print_hls_master_playlist(&master_playlist);
            MSG_PRINT("Which Quality should be downloaded? ");
            if (scanf("%d", &quality_choice) != 1) {
                MSG_ERROR("Input is not a number.\n");
                exit(1);
            }
        }
        media_playlist = master_playlist.media_playlist[quality_choice];
        master_playlist_cleanup(&master_playlist);
    } else if (playlist_type == MEDIA_PLAYLIST) {
        media_playlist.bitrate = 0;
        media_playlist.url = strdup(hls_args.url);
    } else {
        return 1;
    }
    
    if (handle_hls_media_playlist(&media_playlist)) {
        return 1;
    }

    if (media_playlist.encryption) {
        MSG_PRINT("HLS Stream is %s encrypted.\n",
                  media_playlist.encryptiontype == ENC_AES128 ? "AES-128" : "SAMPLE-AES");
    }
    
    MSG_VERBOSE("Media Playlist parsed successfully.\n");
    
    if (hls_args.dump_ts_urls) {
        for (int i = 0; i < media_playlist.count; i++) {
            printf("%s\n", media_playlist.media_segment[i].url);
        }
    } else if (hls_args.dump_dec_cmd) {
        if (print_enc_keys(&media_playlist)) {
            return 1;
        }
    } else if (download_hls(&media_playlist)) {
        return 1;
    }
    
    media_playlist_cleanup(&media_playlist);
    curl_global_cleanup();
    return 0;
}
