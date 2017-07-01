#if defined(WITH_FFMPEG) && WITH_FFMPEG 
#include <libavformat/avformat.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include "misc.h"
#include "msg.h"

static void print_help(const char *filename)
{
    printf("hlsdl v0.09\n");
    printf("(c) 2017 samsamsam@o2.pl based on @selsta code\n");
    printf("Usage: %s url [options]\n\n"
           "-b ... Automaticly choose the best quality.\n"
           "-v ... Verbose more information.\n"
           "-o ... Choose name of output file.\n"
           "-u ... Set custom HTTP User-Agent header\n"
           "-h ... Set custom HTTP header.\n"
           "-p ... Set proxy uri.\n"
           "-f ... Force overwriting the output file.\n"
           "-q ... Print less to the console.\n"
           "-d ... Print the openssl decryption command.\n"
           "-t ... Print the links to the .ts files.\n", filename);
    exit(0);
}

int parse_argv(int argc, const char *argv[])
{
    int ret = 0;
    int c = 0;
    int custom_header_idx = 0;
    while ( (c = getopt(argc, argv, "bvqbftdo:u:h:")) != -1) 
    {
        switch (c) 
        {
        case 'v':
            hls_args.loglevel++;
            break;
        case 'q':
            hls_args.loglevel = -1;
            break;
        case 'b':
            hls_args.use_best = 1;
            break;
        case 'h':
            if (custom_header_idx < HLSDL_MAX_NUM_OF_CUSTOM_HEADERS) {
                hls_args.custom_headers[custom_header_idx] = optarg;
                custom_header_idx += 1;
            }
            break;
        case 'f':
            hls_args.force_overwrite = 1;
            break;
        case 'o':
            hls_args.filename = optarg;
            break;
        case 't':
            hls_args.dump_ts_urls = 1;
            break;
        case 'd':
            hls_args.dump_dec_cmd = 1;
            break;
        case 'u':
            hls_args.user_agent = optarg;
            break;
        case 'p':
            hls_args.proxy_uri = optarg;
            break;
        default:
            MSG_ERROR("?? getopt returned character code 0%o ??\n", c);
            ret = -1;
        }
    }
    
    if (0 == ret && (optind+1) == argc) 
    {
        ret = 0;
        hls_args.url = argv[optind];
        return 0;
    }
    
    print_help(argv[0]);
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

int64_t seek(void *opaque, int64_t offset, int whence)
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
#if defined(WITH_FFMPEG) && WITH_FFMPEG 
        case AVSEEK_SIZE:
            return bb->len;
            break;
#endif
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

    for (int count = 0; count < len; count++) {
        char buf[3] = {pos[0], pos[1], 0};
        data[count] = strtol(buf, NULL, 16);
        pos += 2;
    }
    return 0;
}
