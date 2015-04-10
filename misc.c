#include <libavformat/avformat.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include "misc.h"
#include "msg.h"

static void print_help()
{
    printf("Usage: ./hls-download url [options]\n\n"
           "--best    or -b ... Automaticly choose the best quality.\n"
           "--verbose or -v ... Verbose more information.\n"
           "--output  or -o ... Choose name of output file.\n"
           "--help    or -h ... Print help.\n"
           "--force   or -f ... Force overwriting the output file.\n"
           "--quiet   or -q ... Print less to the console.\n"
           "--dump-dec-cmd  ... Print the openssl decryption command.\n"
           "--dump-ts-urls  ... Print the links to the .ts files.\n");
    exit(0);
}

int parse_argv(int argc, const char * argv[])
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            hls_args.loglevel++;
        }
        else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
            hls_args.loglevel--;
        }
        else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--best")) {
            hls_args.use_best = 1;
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_help();
        }
        else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--force")) {
            hls_args.force_overwrite = 1;
        }
        else if (!strcmp(argv[i], "--dump-ts-urls")) {
            hls_args.dump_ts_urls = 1;
        }
        else if (!strcmp(argv[i], "--dump-dec-cmd")) {
            hls_args.dump_dec_cmd = 1;
        }
        else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if ((i + 1) < argc && *argv[i + 1] != '-') {
                strncpy(hls_args.filename, argv[i + 1], MAX_FILENAME_LEN);
                hls_args.custom_filename = 1;
                i++;
            }
        }
        else {
            strcpy(hls_args.url, argv[i]);
            hls_args.url_passed++;
        }
    }
    
    if (hls_args.url_passed == 1) {
        return 0;
    }
    
    print_help();
    return 1;
}

int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct ByteBuffer *bb = opaque;
    int size = buf_size;
    
    if (bb->len - bb->pos < buf_size) {
        size = bb->len - bb->pos;
    }
    
    if (size > 0) {
        memcpy(buf, bb->data + bb->pos, size);
        bb->pos += size;
    }
    return size;
}

int64_t seek(void* opaque, int64_t offset, int whence)
{
    struct ByteBuffer *bb = opaque;
    
    switch (whence) {
        case SEEK_SET:
            bb->pos = (int)offset;
            break;
        case SEEK_CUR:
            bb->pos += offset;
            break;
        case SEEK_END:
            bb->pos = (int)(bb->len - offset);
            break;
        case AVSEEK_SIZE:
            return bb->len;
            break;
    }
    return bb->pos;
}

int bytes_remaining(uint8_t *pos, uint8_t *end)
{
    return (int)(end - pos);
}

int str_to_bin(uint8_t *data, char *hexstring, int len)
{
    char *pos = hexstring;
    
    for(size_t count = 0; count < len; count++) {
        char buf[3] = {pos[0], pos[1], 0};
        data[count] = strtol(buf, NULL, 16);
        pos += 2;
    }
    return 0;
}