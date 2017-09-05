#if defined(WITH_FFMPEG) && WITH_FFMPEG 
#include <libavformat/avformat.h>
#endif 

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include "curl.h"
#include "hls.h"
#include "msg.h"
#include "misc.h"

int main(int argc, const char * argv[])
{
    memset(&hls_args, 0x00, sizeof(hls_args));
    hls_args.loglevel = 1;
    hls_args.segment_download_retries = HLSDL_MAX_RETRIES;
    hls_args.live_start_offset_sec = HLSDL_LIVE_START_OFFSET_SEC;
    hls_args.open_max_retries = HLSDL_OPEN_MAX_RETRIES;
    hls_args.refresh_delay_sec = -1;
    

    if (parse_argv(argc, argv)) {
        MSG_WARNING("No files passed. Exiting.\n");
        return 0;
    }

    MSG_DBG("Loglevel: %d\n", hls_args.loglevel);

    curl_global_init(CURL_GLOBAL_ALL);
#if defined(WITH_FFMPEG) && WITH_FFMPEG 
    av_register_all();
#endif

    char *hlsfile_source;
    struct hls_media_playlist media_playlist;
    memset(&media_playlist, 0x00, sizeof(media_playlist));
    char *url = NULL;
    size_t size = 0;
    long http_code = 0;
    int tries = hls_args.open_max_retries;
    while (tries) {
        http_code = get_hls_data_from_url(hls_args.url, &hlsfile_source, &size, STRING, &url);
        if (200 != http_code || size == 0) {
            MSG_ERROR("%s %d tries[%d]\n", hls_args.url, (int)http_code, (int)tries);
            --tries;
            sleep(1);
            continue;
        }
        break;
    }
    
    if (http_code != 200) {
        MSG_API("{\"error_code\":%d, \"error_msg\":\"\"}\n", (int)http_code);
    } else if (size == 0) {
        MSG_API("{\"error_code\":-1, \"error_msg\":\"No result from server.\"}\n");
        return 1;
    }

    int playlist_type = get_playlist_type(hlsfile_source);

    if (playlist_type == MASTER_PLAYLIST) {
        struct hls_master_playlist master_playlist;
        master_playlist.source = hlsfile_source;
        master_playlist.url = url;
        url = NULL;
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
        media_playlist.orig_url = strdup(media_playlist.url);
        
    } else if (playlist_type == MEDIA_PLAYLIST) {
        media_playlist.source = hlsfile_source;
        media_playlist.bitrate = 0;
        media_playlist.orig_url = strdup(hls_args.url);
        media_playlist.url      = url;
        url = NULL;
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
        struct hls_media_segment *ms = media_playlist.first_media_segment;
        while(ms) {
            MSG_PRINT("%s\n", ms->url);
            ms = ms->next;
        }
    } else if (hls_args.dump_dec_cmd) {
        if (print_enc_keys(&media_playlist)) {
            return 1;
        }
    } else if (media_playlist.is_endlist) {
        if (download_hls(&media_playlist)) {
            return 1;
        }
    } else if (!media_playlist.is_endlist) {
        if (download_live_hls(&media_playlist)) {
            return 1;
        }
    }
    
    free(url);
    media_playlist_cleanup(&media_playlist);
    curl_global_cleanup();
    return 0;
}
