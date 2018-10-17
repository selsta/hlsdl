#ifndef __HLS_DownLoad__mpegts__
#define __HLS_DownLoad__mpegts__

#include <stdbool.h>
#include <stdint.h>

#define TID_PAT                       0x00
#define TID_CAT                       0x01
#define TID_PMT                       0x02
#define TID_DVB_NIT                   0x40
#define TID_DVB_ONIT                  0x41

#define PID_PAT                       0x0000
#define PID_CAT                       0x0001
#define PID_TSDT                      0x0002
#define PID_DVB_NIT                   0x0010
#define PID_DVB_SDT                   0x0011
#define PID_DVB_EIT                   0x0012
#define PID_DVB_RST                   0x0013
#define PID_DVB_TDT                   0x0014
#define PID_DVB_SYNC                  0x0015
#define PID_DVB_INBAND                0x001c
#define PID_DVB_MEASUREMENT           0x001d
#define PID_DVB_DIT                   0x001e
#define PID_DVB_SIT                   0x001f
#define PID_NULL                      0x1fff
#define PID_UNSPEC                    0xffff

typedef enum pidtype_e{
    PT_UNSPEC,
    PT_SECTIONS,
    PT_PES,
    PT_DATA,
    PT_NULL
} pidtype_t;

#define TS_PACKET_LENGTH              188
#define TS_SYNC_BYTE                  0x47 

typedef struct pmt_data_s {
    uint16_t pid;
    uint16_t program;
    uint16_t pmt_sectionlen;
    uint16_t componennt_idx;
    uint16_t pmt_idx;
    uint8_t data[TS_PACKET_LENGTH];
    uint16_t pcrpid;
    uint16_t epid;
} pmt_data_t;

typedef struct merge_context_s {
    pmt_data_t pmt;
    pmt_data_t pmt1;
    pmt_data_t pmt2;
    bool valid;
    FILE *out;
} merge_context_t;

size_t merge_packets(merge_context_t *context, const uint8_t *pdata1, uint32_t size1, const uint8_t *pdata2, uint32_t size2);

#endif /* defined(__HLS_DownLoad__mpegts__) */
