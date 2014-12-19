#include "msg.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

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
        if (loglevel > 0) {
            result = vfprintf(stdout, fmt, args);
        }
    }
    
    if (lvl == LVL_DBG) {
        if (loglevel > 1) {
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

int system_va(int size_of_call, char *fmt, ...) {
    int result = 0;
    va_list args;
    va_start(args, fmt);
    
    char *systemcall = (char*)malloc(size_of_call);
    result = vsnprintf(systemcall, size_of_call, fmt, args);
    system(systemcall);
    free(systemcall);
    return result;
}