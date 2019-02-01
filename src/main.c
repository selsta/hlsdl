#ifndef _MSC_VER
#include <unistd.h>
#else
#include <Windows.h>
#define sleep Sleep
#endif

#if defined(__MINGW32__) || defined(_MSC_VER)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "curl.h"
#include "hls.h"
#include "msg.h"
#include "misc.h"

static size_t priv_write(const uint8_t *data, size_t len, void *opaque) {
    return fwrite(data, 1, len, opaque);
}

static bool is_file_exists(const char *filename)
{
#ifndef _MSC_VER
    return access(filename, F_OK) != -1;
#else
    struct stat info;
    int ret = -1;

    ret = stat(filename, &info);
    return 0 == ret;
#endif
}

static FILE* get_output_file(void)
{
    FILE *pFile = NULL;

    if (hls_args.filename && 0 == strncmp(hls_args.filename, "-", 2)) {
        // Set "stdout" to have binary mode:
        fflush(stdout);
#if !defined(_MSC_VER) && !defined(__MINGW32__)
        pFile = freopen(NULL, "wb", stdout);
#else
        if (-1 != setmode(_fileno(stdout), _O_BINARY)) {
            pFile = stdout;
        }
#endif
        fflush(stdout);
    } else {
        char filename[MAX_FILENAME_LEN];
        if (hls_args.filename) {
            strcpy(filename, hls_args.filename);
        }
        else {
            strcpy(filename, "000_hls_output.ts");
        }

        if (is_file_exists(filename)) {
            if (hls_args.force_overwrite) {
                if (remove(filename) != 0) {
                    MSG_ERROR("Error overwriting file");
                    exit(1);
                }
            }
            else {
                char userchoice = '\0';
                MSG_PRINT("File already exists. Overwrite? (y/n) ");
                if (scanf("\n%c", &userchoice) && userchoice == 'y') {
                    if (remove(filename) != 0) {
                        MSG_ERROR("Error overwriting file");
                        exit(1);
                    }
                }
                else {
                    MSG_WARNING("Choose a different filename. Exiting.\n");
                    exit(0);
                }
            }
        }

        pFile = fopen(filename, "wb");
    }

    if (pFile == NULL)
    {
        MSG_ERROR("Error can not open output file\n");
        exit(1);
    }
    return pFile;
}

static bool get_data_with_retry(char *url, char **hlsfile_source, char **finall_url, int tries)
{
    size_t size = 0;
    long http_code = 0;
    while (tries) {
        http_code = get_hls_data_from_url(url, hlsfile_source, &size, STRING, finall_url);
        if (200 != http_code || size == 0) {
            MSG_ERROR("%s %d tries[%d]\n", url, (int)http_code, (int)tries);
            --tries;
            sleep(1);
            continue;
        }
        break;
    }

    if (http_code != 200) {
        MSG_API("{\"error_code\":%d, \"error_msg\":\"\"}\n", (int)http_code);
    }

    if (size == 0) {
        MSG_API("{\"error_code\":-1, \"error_msg\":\"No result from server.\"}\n");
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    memset(&hls_args, 0x00, sizeof(hls_args));
    hls_args.loglevel = 0;
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

    char *hlsfile_source = NULL;
    hls_media_playlist_t media_playlist;
    hls_media_playlist_t audio_media_playlist;
    memset(&media_playlist, 0x00, sizeof(media_playlist));
    memset(&audio_media_playlist, 0x00, sizeof(audio_media_playlist));

    char *url = NULL;
    if ( !get_data_with_retry(hls_args.url, &hlsfile_source, &url, hls_args.open_max_retries))
    {
        return 1;
    }

    int playlist_type = get_playlist_type(hlsfile_source);
    if (playlist_type == MASTER_PLAYLIST && hls_args.audio_url)
    {
        MSG_ERROR("uri to audio media playlist was set but main playlist is not media playlist.\n");
        exit(1);
    }

    if (playlist_type == MASTER_PLAYLIST) {
        hls_master_playlist_t master_playlist;
        memset(&master_playlist, 0x00, sizeof(master_playlist));
        master_playlist.source = hlsfile_source;
        master_playlist.url = url;
        url = NULL;
        if (handle_hls_master_playlist(&master_playlist) || !master_playlist.media_playlist) {
            return 1;
        }

        hls_media_playlist_t *selected = NULL;
        if (hls_args.use_best) {
            selected = master_playlist.media_playlist;
            hls_media_playlist_t *me = selected->next;
            while (me) {
                if (me->bitrate > selected->bitrate) {
                    selected = me;
                }
                me = me->next;
            }
            MSG_VERBOSE("Choosing best quality. (Bitrate: %d), (Resolution: %s), (Codecs: %s)\n", selected->bitrate, selected->resolution, selected->codecs);
        } else {
            // print hls master playlist
            int i = 1;
            int quality_choice = 0;

            hls_media_playlist_t *me = master_playlist.media_playlist;
            while (me) {
                MSG_PRINT("%d: Bandwidth: %d, Resolution: %s, Codecs: %s\n", i, me->bitrate, me->resolution, me->codecs);
                i += 1;
                me = me->next;
            }

            MSG_PRINT("Which Quality should be downloaded? ");
            if (scanf("%d", &quality_choice) != 1 || quality_choice <= 0 || quality_choice >= i) {
                MSG_ERROR("Wrong input!\n");
                exit(1);
            }

            i = 1;
            me = master_playlist.media_playlist;
            while (i < quality_choice) {
                i += 1;
                me = me->next;
            }

            selected = me;
        }

        if (!selected) {
            MSG_ERROR("Wrong selection!\n");
            exit(1);
        }

        if (selected->audio_grp) {
            // check if have valid group
            hls_audio_t *selected_audio = NULL;
            hls_audio_t *audio = master_playlist.audio;
            bool has_audio_playlist = false;

            while (audio) {
                if (0 == strcmp(audio->grp_id, selected->audio_grp)) {
                    if (has_audio_playlist) {
                        selected_audio = NULL; // more then one audio playlist, so selection is needed
                        break;
                    } else {
                        has_audio_playlist = true;
                        selected_audio = audio;
                    }
                }
            }

            if (has_audio_playlist) {
                // print hls master playlist
                int audio_choice = 0;
                int i = 1;

                if (!selected_audio) {
                    if (!hls_args.use_best) {
                        audio = master_playlist.audio;
                        while (audio) {
                            if (0 == strcmp(audio->grp_id, selected->audio_grp)) {
                                MSG_PRINT("%d: Name: %s, Language: %s\n", i, audio->name, audio->lang ? audio->lang : "unknown");
                                i += 1;
                            }
                            audio = audio->next;
                        }

                        MSG_PRINT("Which Language should be downloaded? ");
                        if (scanf("%d", &audio_choice) != 1 || audio_choice <= 0 || audio_choice >= i) {
                            MSG_ERROR("Wrong input!\n");
                            exit(1);
                        }
                    } else {
                        audio_choice = 1;
                        i = 0;
                        audio = master_playlist.audio;
                        while (audio) {
                            if (0 == strcmp(audio->grp_id, selected->audio_grp) && audio->is_default) {
                                i += 1;
                                audio_choice = i;
                                break;
                            }
                            audio = audio->next;
                        }
                    }

                    i = 0;
                    audio = master_playlist.audio;
                    while (audio) {
                        if (0 == strcmp(audio->grp_id, selected->audio_grp)) {
                            i += 1;
                            if (i == audio_choice) {
                                selected_audio = audio;
                                break;
                            }
                        }
                        audio = audio->next;
                    }

                    if (!selected_audio) {
                        MSG_ERROR("Wrong selection!\n");
                        exit(1);
                    }
                }

                audio_media_playlist.orig_url = strdup(selected_audio->url);
            }
        }

        // make copy of structure
        memcpy(&media_playlist, selected, sizeof(media_playlist));
        /* we will take this attrib to selected playlist */
        selected->url = NULL;
        selected->audio_grp = NULL;
        selected->resolution = NULL;
        selected->codecs = NULL;

        media_playlist.orig_url = strdup(media_playlist.url);
        master_playlist_cleanup(&master_playlist);
    } else if (playlist_type == MEDIA_PLAYLIST) {
        media_playlist.source = hlsfile_source;
        media_playlist.bitrate = 0;
        media_playlist.orig_url = strdup(hls_args.url);
        media_playlist.url      = url;
        url = NULL;

        if (hls_args.audio_url) {
            audio_media_playlist.orig_url = strdup(hls_args.audio_url);
        }
    } else {
        return 1;
    }

    if (audio_media_playlist.orig_url) {

        if ( !get_data_with_retry(audio_media_playlist.orig_url, &audio_media_playlist.source, &audio_media_playlist.url, hls_args.open_max_retries)) {
            return 1;
        }

        if (get_playlist_type(audio_media_playlist.source) != MEDIA_PLAYLIST) {
            MSG_ERROR("uri to audio media playlist was set but it is not media playlist.\n");
            exit(1);
        }
    }

    if (handle_hls_media_playlist(&media_playlist)) {
        return 1;
    }

    if (audio_media_playlist.url && handle_hls_media_playlist(&audio_media_playlist)) {
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
    } else {
        int ret = -1;
        FILE *out_file = get_output_file();
        if (out_file) {
            write_ctx_t out_ctx = {priv_write, out_file};
            if (media_playlist.is_endlist) {
                ret = download_hls(&out_ctx, &media_playlist, &audio_media_playlist);
            } else {
                ret = download_live_hls(&out_ctx, &media_playlist);
            }
            fclose(out_file);
        }
        return ret ? 1 : 0;
    }

    free(url);
    media_playlist_cleanup(&media_playlist);
    curl_global_cleanup();
    return 0;
}
