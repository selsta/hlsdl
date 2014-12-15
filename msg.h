#ifndef __HLS_DownLoad__msg__
#define __HLS_DownLoad__msg__

#include <stdio.h>

#define LVL_ERROR 0x01
#define LVL_WARNING 0x02
#define LVL_VERBOSE 0x03
#define LVL_DBG 0x04
#define LVL_PRINT 0x05


#define MSG_ERROR(...) msg_print_va(LVL_ERROR, __VA_ARGS__)
#define MSG_WARNING(...) msg_print_va(LVL_WARNING, __VA_ARGS__)
#define MSG_VERBOSE(...) msg_print_va(LVL_VERBOSE, __VA_ARGS__)
#define MSG_DBG(...) msg_print_va(LVL_DBG, __VA_ARGS__)
#define MSG_PRINT(...) msg_print_va(LVL_PRINT, __VA_ARGS__)

int loglevel;

int msg_print_va(int lvl, char *fmt, ...);


#endif /* defined(__HLS_DownLoad__msg__) */