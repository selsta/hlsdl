#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "msg.h"
#include "misc.h"

int msg_print_va(int lvl, char *fmt, ...) {
    int result = 0;
    va_list args;
    va_start(args, fmt);
    
    if (lvl == LVL_ERROR) {
        fputs("Error: ", stderr);
        result = vfprintf(stderr, fmt, args);
    }
    
    if (lvl == LVL_WARNING) {
        fputs("Warning: ", stderr);
        result = vfprintf(stdout, fmt, args);
    }
    
    if (lvl == LVL_VERBOSE) {
        if (hls_args.loglevel > 0) {
            result = vfprintf(stdout, fmt, args);
        }
    }
    
    if (lvl == LVL_DBG) {
        if (hls_args.loglevel > 1) {
        fputs("Debug: ", stdout);
        result = vfprintf(stdout, fmt, args);
        }
    }
    
    if (lvl == LVL_PRINT) {
        result = vfprintf(stdout, fmt, args);
    }
    
    va_end(args);
    return result;
}