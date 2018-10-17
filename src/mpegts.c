
#include <stdio.h>
#include <string.h>

#include "mpegts.h"
#include "msg.h"

static const unsigned int crc32_table[] =
{
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
  0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
  0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
  0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
  0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
  0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
  0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
  0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
  0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
  0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
  0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
  0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
  0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
  0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
  0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
  0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
  0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
  0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
  0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
  0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
  0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
  0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
  0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
  0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
  0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
  0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
  0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
  0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
  0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
  0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
  0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
  0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
  0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
  0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
  0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
  0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
  0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
  0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
  0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
  0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
  0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
  0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
  0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static uint32_t crc32 (const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t crc = 0xffffffff;

    for (i=0; i<len; i++)
            crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *data++) & 0xff];

    return crc;
}

static pidtype_t get_pid_type(uint16_t pid)
{
    pidtype_t pidtype = PT_UNSPEC;
    switch(pid)
    {
        case PID_PAT:
        case PID_CAT:
        case PID_TSDT:
        case PID_DVB_NIT:
            pidtype = PT_SECTIONS;
            break;
        case PID_NULL:
            pidtype = PT_NULL;
            break;
    }
    return pidtype;
}

static bool parse_pmt(const uint8_t *data, pmt_data_t *pmt)
{
    const uint8_t *bufp = data;

    // packed parsing

    bufp++; /* Skip sync byte */
    uint8_t unitstart = (bufp[0] & 0x40) >> 6; // payload_unit_start_indicator - 1b
    // transport_priority - 1b
    pmt->pid = ((bufp[0] & 0x1f) << 8) | bufp[1]; // PID - 13b
    // transport_scrambling_control - 2b
    uint16_t afc = (bufp[2] & 0x30) >> 4; // adaptation_field_control - 2b
    uint16_t continuity = bufp[2] & 0x0f; // continuity_counter - 4b
    if (continuity != 0 || (afc & 2))
    {
        MSG_WARNING("Not supported format afc: 0x%hhx, continuity: 0x%hhx\n", afc, continuity);
        //return false;
    }
    bufp += 3;
    if (unitstart)
    {
        bufp += bufp[0] + 1;
    }

    // section parssing
    uint8_t tableid = bufp[0]; // table_id - 8b
    if(TID_PMT != tableid)
    {
        return false;
    }
    uint8_t syntax = (bufp[1] & 0x80) >> 7; // section_syntax_indicator - 1b
    if (!syntax)
    {
        MSG_ERROR("Error PMT section should syntax set to 1\n");
        return false;
    }
    // '0' - 1b
    // reserved - 2b
    uint16_t seclen = ((bufp[1] & 0x0f) << 8) | bufp[2]; // section_length - 12b
    pmt->pmt_sectionlen = seclen + 3; // whole size of data
    if (pmt->pmt_sectionlen + (bufp - data) > TS_PACKET_LENGTH)
    {
        MSG_ERROR("PMT section to long seclen: 0x%hhx\n", seclen);
        return false;
    }

    pmt->pmt_idx = bufp - data;
    const uint8_t *ppmt = bufp;
    memcpy(pmt->data, data, TS_PACKET_LENGTH);

    bufp += 3;
    pmt->program = (bufp[0] << 8) | bufp[1]; // program_number - 16b
    // reserved - 2b
    // version_number - 5b
    // uint8_t curnext = (bufp[2] & 0x01); // current_next_indicator - 1b
    uint8_t section = bufp[3]; // section_number - 8b
    uint8_t last = bufp[4];    // last_section_number - 8b
    if (section != 0 && last != 0)
    {
        MSG_ERROR("PMT in more then one section, section_number: 0x%hhx, last_section_number: 0x%hhx\n", section, last);
        return false;
    }
    // reserved - 3b
    bufp += 5;

    pmt->pcrpid = ((bufp[0] & 0x1f) << 8) | bufp[1]; // PCR_PID - 13b
    // reserved - 4b
    uint16_t desclen = ((bufp[2] & 0x0f) << 8) | bufp[3]; // program_info_length - 12b
    bufp += 4;
    if (desclen + (bufp - ppmt) > pmt->pmt_sectionlen)
    {
        MSG_ERROR("PMT section to long desclen: 0x%hhx\n", desclen);
        return false;
    }
    // descriptor
    bufp += desclen;
    pmt->componennt_idx = bufp - data; //ppmt;

    while (pmt->pmt_sectionlen - (bufp - ppmt) - 4)
    {
        // uint8_t stype = bufp[0]; // stream_type - 8b
        bufp+=1;
        // reserved - 3b
        pmt->epid = ((bufp[0] & 0x1f) << 8) | bufp[1]; // elementary_PID - 13b
        bufp += 2;
        // reserved - 4b
        uint16_t infolen = ((bufp[0] & 0x0f) << 8) | bufp[1];// ES_info_length - 12b
        bufp += 2;
        bufp += infolen;
        if (pmt->pmt_sectionlen - 4 != (bufp - ppmt))
        {
            MSG_ERROR("PMT section contain more then one component, rest data: 0x%x\n", ((int32_t)pmt->pmt_sectionlen) - (bufp - ppmt) - 4);
            return false;
        }
    }
    return true;
}

static bool find_pmt(const uint8_t *bufp, uint32_t size, pmt_data_t *pmt)
{
    while (size > TS_PACKET_LENGTH)
    {
        if (TS_SYNC_BYTE == bufp[0])
        {
            uint16_t pid = ((bufp[1] & 0x1f) << 8) | bufp[2]; // PID - 13b
            if (PT_UNSPEC == get_pid_type(pid))
            {
                if ( parse_pmt(bufp, pmt) )
                {
                    return true;
                }
            }
            bufp += TS_PACKET_LENGTH;
            size -= TS_PACKET_LENGTH;
        }
        else
        {
            MSG_WARNING("Missing sync byte!!!\n");
            bufp += 1;
        } 
    }
    return false;
}

static bool merge_pmt(const pmt_data_t *pmt1, const pmt_data_t *pmt2, pmt_data_t *pmt)
{
    if (pmt1->epid == pmt2->epid)
    {
        MSG_ERROR("Same elementary pids %04x == %04x!!!\n", (uint32_t)pmt1->epid, (uint32_t)pmt2->epid);
        return false;
    }

    if (pmt1->program != pmt2->program)
    {
        MSG_WARNING("Diffrent program ids %04x != %04x!!!\n", (uint32_t)pmt1->program, (uint32_t)pmt2->program);
        //return false;
    }

    uint32_t len1 = pmt1->pmt_idx + pmt1->pmt_sectionlen;
    uint32_t len2 = pmt2->pmt_idx + pmt2->pmt_sectionlen;

    uint32_t component_len1 = len1 - pmt1->componennt_idx - 4; 
    uint32_t component_len2 = len2 - pmt2->componennt_idx - 4; 
    if (len1 + component_len2 > TS_PACKET_LENGTH)
    {
        MSG_ERROR("Merged PMT to long for one TS packet!");
        return false;
    }

    memcpy(pmt, pmt1, sizeof(*pmt1));
    memcpy(pmt->data + pmt->componennt_idx + component_len1, pmt2->data + pmt2->componennt_idx , component_len2);

    // update section len
    pmt->pmt_sectionlen = pmt1->pmt_sectionlen + component_len2;
    if (pmt->pmt_sectionlen > 1023)
    {
        MSG_ERROR("Merged PMT section to long: %d!", pmt->pmt_sectionlen);
        return false;
    }
    uint16_t sectionlen = pmt->pmt_sectionlen - 3;
    pmt->data[pmt->pmt_idx + 1] = (pmt->data[pmt->pmt_idx + 1] & 0xf0) | (sectionlen & 0x0F00) >> 8;
    pmt->data[pmt->pmt_idx + 2] = (sectionlen & 0x00FF);

    // update crc
    uint32_t crc = crc32(pmt->data + pmt->pmt_idx, pmt->pmt_sectionlen-4);

    pmt->data[pmt->pmt_idx + pmt->pmt_sectionlen - 4] = (crc >> 24) & 0xff;
    pmt->data[pmt->pmt_idx + pmt->pmt_sectionlen - 3] = (crc >> 16) & 0xff;
    pmt->data[pmt->pmt_idx + pmt->pmt_sectionlen - 2] = (crc >> 8) & 0xff;
    pmt->data[pmt->pmt_idx + pmt->pmt_sectionlen - 1] = crc & 0xff;
    return true;
}

size_t merge_packets(merge_context_t *context, const uint8_t *pdata1, uint32_t size1, const uint8_t *pdata2, uint32_t size2)
{
    size_t ret = 0;
    if ( !context->valid)
    {
        if (find_pmt(pdata1, size1, &context->pmt1) && 
            find_pmt(pdata2, size2, &context->pmt2))
        {
            if (merge_pmt(&context->pmt1, &context->pmt2, &context->pmt))
            {
                context->valid = true;
            }
        }
    }

    if (context->valid)
    {
        uint32_t count1 = size1 / TS_PACKET_LENGTH;
        uint32_t count2 = size2 / TS_PACKET_LENGTH;

        if (count1 && count2)
        {
            uint32_t max1 = 0;
            uint32_t max2 = 0;
            if (count1 > count2)
            {
                max1 = count1 / count2;
                max2 = 1;
            }
            else
            {
                max1 = 1;
                max2 = count2 / count1;
            }

            uint32_t i = 0;
            uint32_t j = 0;
            while (i < count1 || j < count2)
            {
                for(uint32_t k=0; k<max1 && i < count1; ++k, ++i, pdata1 += TS_PACKET_LENGTH)
                {
                    if (TS_SYNC_BYTE == pdata1[0])
                    {
                        uint16_t pid = ((pdata1[1] & 0x1f) << 8) | pdata1[2]; // PID - 13b
                        if (pid != context->pmt1.pid)
                        {
                            if (fwrite(pdata1, TS_PACKET_LENGTH, 1, context->out))
                            {
                                ret += TS_PACKET_LENGTH;
                            }

                            if (TID_PAT == pid)
                            {
                                if (fwrite(context->pmt.data, TS_PACKET_LENGTH, 1, context->out))
                                {
                                    ret += TS_PACKET_LENGTH;
                                }
                            }
                        }
                    }
                }

                for(uint32_t k=0; k<max2 && j < count2; ++k, ++j, pdata2 += TS_PACKET_LENGTH)
                {
                    if (TS_SYNC_BYTE == pdata2[0])
                    {
                        uint16_t pid = ((pdata2[1] & 0x1f) << 8) | pdata2[2]; // PID - 13b
                        if (TID_PAT != pid && pid != context->pmt2.pid)
                        {
                            if (fwrite(pdata2, TS_PACKET_LENGTH, 1, context->out))
                            {
                                ret += TS_PACKET_LENGTH;
                            }
                        }
                    }
                }
            }
        }
    }
    return ret;
}
