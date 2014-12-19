#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include "misc.h"

static void print_help()
{
    printf("HLS Downloader\n");
    printf("--best    or -b ... Automaticly choose the best quality.\n");
    printf("--verbose or -v ... Verbose more information.\n");
    printf("--output  or -o ... Choose name of output file.\n");
    printf("--help    or -h ... Print help.\n");
    printf("--quiet   or -q ... Print less to the console.\n");
    exit(0);
}

int parse_argv(int argc, const char * argv[]) {
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
        else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if ((i + 1) < argc) {
                strncpy(hls_args.filename, argv[i + 1], 256);
                hls_args.custom_filename = 1;
            }
            i++;
        }
        else {
            strcpy(hls_args.url, argv[i]);
            hls_args.url_passed++;
        }
    }
    
    if (hls_args.url_passed) {
        return 0;
    }
    
    print_help();
    return 1;
}

char *get_rndstring(int length) //not used at the moment
{
    int max;
    const char *letters = "abcdefghijklmnopqrstuvwxyz123456789";
    int i, r;
    srand((int)time(NULL));
    max = length;
    
    char *generated = (char*)malloc(max + 1);
    for (i = 0; i < max; i++) {
        r = rand() % strlen(letters);
        generated[i] = letters[r];
    }
    generated[max] = '\0';
    return generated;
}

int system_va(int size_of_call, char *fmt, ...)
{
    int result = 0;
    va_list args;
    va_start(args, fmt);
    
    char *systemcall = (char*)malloc(size_of_call);
    result = vsnprintf(systemcall, size_of_call, fmt, args);
    system(systemcall);
    free(systemcall);
    return result;
}