#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#if !defined(_MSC_VER) && !defined(__MINGW32__)
#include <pthread.h>
#endif
#include "msg.h"
#include "misc.h"

int msg_print_va(int lvl, char *fmt, ...)
{
    int result = 0;
    va_list args;
    va_start(args, fmt);

    /* -q keeps the status lines and the errors / warnings (a frontend logs why
     * a download failed), -qq silences everything */
    bool quiet_ok = (lvl == LVL_API || lvl == LVL_ERROR || lvl == LVL_WARNING) && hls_args.loglevel >= -1;
    if (hls_args.loglevel >= 0 || quiet_ok) {
        /* prefix + message as one line: the live refresh threads print too,
         * and a frontend parses the status lines (JSON) line by line. The
         * video refresh thread can be pthread_cancel()ed, and the write
         * underneath is a cancellation point - cancelled there it would keep
         * the stream lock and the next message would block for good, so
         * cancellation is held off while the lock is taken. */
#if !defined(_MSC_VER) && !defined(__MINGW32__)
        int cancel_state = 0;
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancel_state);
        flockfile(stderr);
#endif
        switch(lvl)
        {
            case LVL_API:
                result = vfprintf(stderr, fmt, args);
            break;
            case LVL_ERROR:
                fputs("Error: ", stderr);
                result = vfprintf(stderr, fmt, args);
            break;
            case LVL_WARNING:
                fputs("Warning: ", stderr);
                result = vfprintf(stderr, fmt, args);
            break;
            case LVL_VERBOSE:
                if (hls_args.loglevel > 0) {
                    result = vfprintf(stderr, fmt, args);
                }
            break;
            case LVL_DBG:
                if (hls_args.loglevel > 1) {
                    fputs("Debug: ", stderr);
                    result = vfprintf(stderr, fmt, args);
                }
            break;
            case LVL_PRINT:
                result = vfprintf(stderr, fmt, args);
            break;
            default:
                break;
        }
#if !defined(_MSC_VER) && !defined(__MINGW32__)
        funlockfile(stderr);
        pthread_setcancelstate(cancel_state, &cancel_state);
#endif
    }
    va_end(args);
    return result;
}
