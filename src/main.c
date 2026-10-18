#ifndef _MSC_VER
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "curl.h"
#include "hls.h"
#include "msg.h"
#include "misc.h"

char* str_ecryption_type[] ={
    "NONE",
    "AES-128",
    "SAMPLE-AES",
    "SAMPLE-AES-CTR",
    "UNSUPPORTED",
};

typedef struct output_file {
    FILE *file;
    char filename[MAX_FILENAME_LEN + 22];
    bool failed;
} output_file_t;

/* LANGUAGE of a rendition against -A, case-insensitive (BCP 47 tags):
 * "de" also takes "de-DE" / "DE" */
static bool lang_matches(const char *lang, const char *wanted)
{
    if (!lang || !wanted) {
        return false;
    }
    while (*lang && *wanted) {
        char a = *lang++, b = *wanted++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) {
            return false;
        }
    }
    return *wanted == '\0' && (*lang == '\0' || *lang == '-');
}

/* The AUDIO rendition of group grp_id to use. Without a terminal to ask
 * (-b, -A, -W or -H given) it is never asked for: the -A language, else the
 * DEFAULT=YES one, else the first one. Asking used to block on stdin when the
 * group had no DEFAULT rendition. A rendition with url == NULL is the audio
 * inside the variant itself. NULL when the group is unknown. */
static hls_audio_t *select_audio_rendition(hls_audio_t *list, const char *grp_id)
{
    hls_audio_t *audio;
    hls_audio_t *first = NULL;
    hls_audio_t *def = NULL;
    hls_audio_t *by_lang = NULL;
    int count = 0;

    for (audio = list; audio; audio = audio->next) {
        if (strcmp(audio->grp_id, grp_id)) {
            continue;
        }
        count++;
        if (!first) {
            first = audio;
        }
        if (!def && audio->is_default) {
            def = audio;
        }
        if (!by_lang && hls_args.audiolang && lang_matches(audio->lang, hls_args.audiolang)) {
            by_lang = audio;
        }
    }
    if (count <= 1) {
        return first;
    }
    if (by_lang) {
        return by_lang;
    }
    if (hls_args.use_best || hls_args.audiolang || hls_args.maxwidth > -1 || hls_args.maxheight > -1) {
        return def ? def : first;
    }

    int i = 0;
    int audio_choice = -1;
    for (audio = list; audio; audio = audio->next) {
        if (!strcmp(audio->grp_id, grp_id)) {
            MSG_PRINT("%d: Name: %s, Language: %s%s\n", i, audio->name, audio->lang ? audio->lang : "unknown",
                      audio->url ? "" : " (in the video stream)");
            i += 1;
        }
    }
    MSG_PRINT("Which Language should be downloaded? ");
    if (scanf("%d", &audio_choice) != 1 || audio_choice < 0 || audio_choice >= i) {
        MSG_ERROR("Wrong input!\n");
        exit(1);
    }
    i = 0;
    for (audio = list; audio; audio = audio->next) {
        if (!strcmp(audio->grp_id, grp_id) && i++ == audio_choice) {
            return audio;
        }
    }
    return NULL;
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

/* FNV-1a over what identifies the playlist being downloaded: for the video and
 * then the audio playlist the selected variant's bandwidth / resolution /
 * codecs and, per segment, the URL path, byte range and duration. Two quality
 * variants of the same VOD almost always have the same segment count, so this
 * is what actually tells a resume run that the playlist it was handed is a
 * different one. The variant and duration terms keep that true when the URLs
 * alone cannot tell (they are stripped of scheme, host and query, see below). */
static uint64_t fnv1a(uint64_t h, const void *data, size_t len)
{
    const unsigned char *p = data;
    while (len--) {
        h ^= *p++;
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t fnv1a_str(uint64_t h, const char *s)
{
    if (s) {
        h = fnv1a(h, s, strlen(s));
    }
    return fnv1a(h, "\xff", 1);   /* keeps ("ab","c") apart from ("a","bc") */
}

/* The part of a segment URL that identifies the media: its path. Scheme, host
 * and everything from '?' / '#' on are left out - CDNs routinely put a
 * per-request signature or session token in the query string (and may hand out
 * a different edge hostname each time), so hashing them would make every
 * resume of the very same stream look like a different playlist. Quality
 * variants still differ in their path (and, if not, in the variant terms of the
 * fingerprint). A relative segment URL is stored joined to its playlist's URL,
 * so the playlist's own path counts too. A token that lives in the path itself
 * only costs a resume (starting fresh), never a wrong splice. */
static void url_identity(const char *url, const char **start, size_t *len)
{
    const char *p = url;
    const char *q = url;

    if ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z')) {
        while ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z')
               || (*q >= '0' && *q <= '9') || *q == '+' || *q == '-' || *q == '.') {
            q++;
        }
        if (0 == strncmp(q, "://", 3)) {
            p = strchr(q + 3, '/');
            if (!p) {
                p = url + strlen(url);
            }
        }
    }
    *start = p;
    *len = strcspn(p, "?#");
}

static uint64_t playlist_fingerprint(const hls_media_playlist_t *me, const hls_media_playlist_t *audio)
{
    uint64_t h = 14695981039346656037ULL;   /* FNV-1a 64-bit offset basis */
    for (int pass = 0; pass < 2; pass++) {
        const hls_media_playlist_t *pl = pass ? audio : me;
        if (!pl) {
            continue;
        }
        h = fnv1a(h, &pl->bitrate, sizeof(pl->bitrate));
        h = fnv1a_str(h, pl->resolution);
        h = fnv1a_str(h, pl->codecs);
        for (const struct hls_media_segment *s = pl->first_media_segment; s; s = s->next) {
            if (s->url) {
                const char *id = NULL;
                size_t idlen = 0;
                url_identity(s->url, &id, &idlen);
                h = fnv1a(h, id, idlen);
            }
            h = fnv1a(h, &s->offset, sizeof(s->offset));
            h = fnv1a(h, &s->size, sizeof(s->size));
            h = fnv1a(h, &s->duration_ms, sizeof(s->duration_ms));
            h = fnv1a(h, &s->is_map, sizeof(s->is_map));
        }
        h ^= 0x9e3779b97f4a7c15ULL;   /* separate the video and audio runs */
    }
    return h;
}

static FILE* get_output_file(const char *filename)
{
    FILE *pFile = NULL;

    if (strcmp(filename, "-") == 0) {
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
        if (is_file_exists(filename)) {
            if (hls_args.force_overwrite) {
                if (remove(filename) != 0) {
                    MSG_ERROR("Error overwriting file");
                    return NULL;
                }
            }
            else {
                char userchoice = '\0';
                MSG_PRINT("File already exists. Overwrite? (y/n) ");
                if (scanf("\n%c", &userchoice) && userchoice == 'y') {
                    if (remove(filename) != 0) {
                        MSG_ERROR("Error overwriting file");
                        return NULL;
                    }
                }
                else {
                    MSG_WARNING("Choose a different filename. Exiting.\n");
                    return NULL;
                }
            }
        }

        pFile = fopen(filename, "wb");
    }

    if (pFile == NULL)
    {
        MSG_ERROR("Error can not open output file\n");
        return NULL;
    }
    return pFile;
}

/* -R: the output of the interrupted run, without truncation or overwrite
 * prompt, positioned at the resume offset (a partly written tail is cut
 * off). NULL when that does not work - the caller then starts afresh. */
static FILE *reopen_output_file(const char *filename, int64_t offset)
{
    FILE *pFile = fopen(filename, "r+b");
    if (!pFile) {
        MSG_WARNING("resume: cannot reopen %s - starting fresh\n", filename);
        return NULL;
    }
    fflush(pFile);
#ifndef _MSC_VER
    int trunc_err = ftruncate(fileno(pFile), (off_t)offset);
#else
    int trunc_err = _chsize_s(_fileno(pFile), (long long)offset);
#endif
    /* EOF is the resume offset after the truncate; fseek(SEEK_END) needs no
     * 64-bit absolute offset argument */
    if (0 != trunc_err || 0 != fseek(pFile, 0, SEEK_END)) {
        MSG_WARNING("resume: cannot position the output - starting fresh\n");
        fclose(pFile);
        return NULL;
    }
    return pFile;
}

static int begin_output_segment(uint64_t index, void *opaque)
{
    output_file_t *out = opaque;
    const char *path = hls_args.filename ? hls_args.filename : "hls_output.ts";
    const char *base = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if (backslash && (!base || backslash > base)) {
        base = backslash;
    }
#endif
    base = base ? base + 1 : path;
#ifdef _WIN32
    if (base == path && path[0] && path[1] == ':') {
        base += 2;
    }
#endif
    if (!*base) {
        MSG_ERROR("Output filename must include a basename.\n");
        return 1;
    }
    int len = snprintf(out->filename, sizeof(out->filename), "%.*s%03"PRIu64"_%s",
        (int)(base - path), path, index, base);
    if (len < 0 || (size_t)len >= sizeof(out->filename)) {
        MSG_ERROR("Output filename is too long.\n");
        return 1;
    }
    return 0;
}

static size_t priv_write(const uint8_t *data, size_t len, void *opaque)
{
    output_file_t *out = opaque;
    if (out->failed) {
        return 0;
    }
    if (!out->file) {
        out->file = get_output_file(out->filename);
        if (!out->file) {
            out->failed = true;
            return 0;
        }
    }
    size_t written = fwrite(data, 1, len, out->file);
    if (written != len) {
        MSG_ERROR("Could not write output file.\n");
        out->failed = true;
    }
    return written;
}

static int finish_output_segment(bool success, void *opaque)
{
    output_file_t *out = opaque;
    if (hls_args.separate_segments && out->file) {
        if (fclose(out->file)) {
            MSG_ERROR("Could not finish writing output file.\n");
            out->failed = true;
        }
        out->file = NULL;
        if ((!success || out->failed) && remove(out->filename)) {
            MSG_WARNING("Could not remove incomplete output file: %s\n", out->filename);
        }
    }
    return out->failed ? 1 : 0;
}

/* -R: the bytes really in the output file, after a flush */
static int64_t output_position(void *opaque)
{
    output_file_t *out = opaque;
    if (!out->file || fflush(out->file)) {
        return -1;
    }
#ifdef _MSC_VER
    return (int64_t)_ftelli64(out->file);
#else
    return (int64_t)ftello(out->file);
#endif
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
        return false;
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
    hls_args.live_duration_sec = HLSDL_LIVE_DURATION;
    hls_args.open_max_retries = HLSDL_OPEN_MAX_RETRIES;
    hls_args.refresh_delay_sec = -1;
    hls_args.maxwidth = -1;
    hls_args.maxheight = -1;
    hls_args.audiolang = NULL;

    if (parse_argv(argc, argv)) {
        MSG_WARNING("No files passed. Exiting.\n");
        return 0;
    }

    if (hls_args.separate_segments && hls_args.filename &&
        strcmp(hls_args.filename, "-") == 0) {
        MSG_ERROR("Separate segments cannot be written to stdout.\n");
        return 1;
    }

    /* The resume sidecar counts written segments and bytes; after a skipped
     * segment a later -R run could not tell the gap from the finished part. */
    if (hls_args.resume && hls_args.ignore_download_errors) {
        MSG_ERROR("-R and -I cannot be combined.\n");
        return 1;
    }
    /* ... and it describes one output file */
    if (hls_args.resume && hls_args.separate_segments) {
        MSG_ERROR("-R and -m cannot be combined.\n");
        return 1;
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
        } else if (hls_args.maxwidth > -1 || hls_args.maxheight > -1) {
            int width, maxwidth = 0;
            int height, maxheight = 0;
            hls_media_playlist_t *me;
            for (me = master_playlist.media_playlist; me; me = me->next) {
                /* a variant without RESOLUTION must not end the search */
                if (sscanf(me->resolution, "%dx%d", &width, &height) < 2)
                    continue;
                if (width > hls_args.maxwidth && hls_args.maxwidth != -1)
                    continue;
                if (height > hls_args.maxheight && hls_args.maxheight != -1)
                    continue;
                if (selected == NULL ||
                    ((hls_args.maxwidth == -1 || width > maxwidth) &&
                     (hls_args.maxheight == -1 || height > maxheight))) {
                    selected = me;
                    maxwidth = width;
                    maxheight = height;
                }
            }
            if (selected == NULL) {
                MSG_ERROR("No resolution match found\n");
                exit(1);
            }
            MSG_VERBOSE("Choosing by resolution. (Bitrate: %d), (Resolution: %s), (Codecs: %s)\n", selected->bitrate, selected->resolution, selected->codecs);
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
            hls_audio_t *selected_audio = select_audio_rendition(master_playlist.audio, selected->audio_grp);
            if (selected_audio && selected_audio->url) {
                MSG_VERBOSE("Audio rendition \"%s\" (%s).\n", selected_audio->name, selected_audio->lang ? selected_audio->lang : "unknown");
                audio_media_playlist.orig_url = strdup(selected_audio->url);
            } else if (selected_audio) {
                MSG_VERBOSE("Audio rendition \"%s\" is part of the variant - no separate audio playlist.\n", selected_audio->name);
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

    MSG_PRINT("HLS Stream is %s encrypted.\n",
                  str_ecryption_type[media_playlist.encryptiontype]);

    MSG_VERBOSE("Media Playlist parsed successfully.\n");

    if (hls_args.dump_ts_urls) {
        struct hls_media_segment *ms = media_playlist.first_media_segment;
        const struct hls_media_segment *last_init = NULL;
        while(ms) {
            /* a repeated EXT-X-MAP of the same init is listed once */
            if (!ms->is_map || !last_init || strcmp(ms->url, last_init->url) ||
                ms->offset != last_init->offset || ms->size != last_init->size) {
                MSG_PRINT("%s\n", ms->url);
            }
            if (ms->is_map) {
                last_init = ms;
            }
            ms = ms->next;
        }
    } else if (hls_args.dump_dec_cmd) {
        if (print_enc_keys(&media_playlist)) {
            return 1;
        }
    } else {
        int ret = -1;
        hls_resume_state_t *resume = NULL;
        bool resuming = false;

        if (hls_args.resume && media_playlist.is_endlist
            && hls_args.filename && 0 != strncmp(hls_args.filename, "-", 2)) {
            int total = 0, has_map = 0;
            for (struct hls_media_segment *s = media_playlist.first_media_segment; s; s = s->next) {
                if (s->is_map) {
                    has_map = 1;
                } else {
                    total++;
                }
            }
            uint64_t fp = playlist_fingerprint(&media_playlist,
                              audio_media_playlist.first_media_segment ? &audio_media_playlist : NULL);
            resume = resume_load(hls_args.filename, total, has_map, fp);
            resuming = (resume && resume->done > 0);

            if (resuming) {
                /* The output file must be at least as large as the recorded
                 * byte count, otherwise a truncate would zero-extend it. */
                struct stat st;
                if (0 != stat(hls_args.filename, &st) || (int64_t)st.st_size < resume->bytes) {
                    MSG_WARNING("resume: %s is missing or smaller than expected - starting fresh\n", hls_args.filename);
                    resume->done = 0;
                    resume->bytes = 0;
                    resuming = false;
                }
            }
        }

        output_file_t output = {0};
        if (resuming) {
            output.file = reopen_output_file(hls_args.filename, resume->bytes);
            if (!output.file) {
                /* the partial file stays; get_output_file() overwrites it
                 * under -f, or asks, rather than deleting it here */
                resume->done = 0;
                resume->bytes = 0;
            }
        }
        if (!output.file && !hls_args.separate_segments) {
            output.file = get_output_file(hls_args.filename ? hls_args.filename : "000_hls_output.ts");
        }
        if (output.file || hls_args.separate_segments) {
            write_ctx_t out_ctx = {
                .write = priv_write,
                .opaque = &output,
                .begin_segment = hls_args.separate_segments ? begin_output_segment : NULL,
                .end_segment = finish_output_segment,
                .position = output_position,
            };
            if (media_playlist.is_endlist) {
                ret = download_hls(&out_ctx, &media_playlist, &audio_media_playlist, resume);
            } else {
                ret = download_live_hls(&out_ctx, &media_playlist,
                          audio_media_playlist.first_media_segment ? &audio_media_playlist : NULL);
            }
            /* the last buffered bytes are written here - a full disk shows up
             * now, not in the segment writes */
            if (output.file && fclose(output.file)) {
                MSG_ERROR("Could not finish writing output file.\n");
                ret = 1;
            }
            if (output.failed) {
                ret = 1;
            }
        }
        free(resume);
        fill_key_value_cleanup();
        return ret ? 1 : 0;
    }

    free(url);
    media_playlist_cleanup(&media_playlist);
    curl_global_cleanup();
    return 0;
}
