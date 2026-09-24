// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the skinning example.

#include <stddef.h>

#include <teak/teak.h>

#include "proto_teak.h"
#include "skinning.h"

void app_init(void)
{
    skinning_generate();
}

uint16_t *app_upload_buffer(uint16_t table, uint16_t words)
{
    (void)table;
    (void)words;
    return NULL;
}

uint16_t app_job(uint16_t job)
{
    switch (job)
    {
        case JOB_SKIN_C:
            k_skin(verts, bones, out_verts, N_VERTS);
            break;
        case JOB_SKIN_ASM:
            k_skin_asm(verts, bones, out_verts, N_VERTS);
            break;
        case JOB_SKIN_Q16_C:
            k_skin_q16(verts_q16, bones_q16, out_verts_q16, N_VERTS);
            break;
        case JOB_SKIN2_ASM:
            k_skin2_asm(verts, bones, out_verts, N_VERTS);
            break;
        case JOB_SKIN_Q16_ASM:
            k_skin_q16_asm(verts_q16, bones_q16, out_verts_q16, N_VERTS);
            break;
        case JOB_INPUTS:
            break;
        default:
            return PROTO_STATUS_BAD_ARG;
    }

    return PROTO_STATUS_OK;
}

uint32_t app_checksum(uint16_t job)
{
    return skinning_checksum(job);
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    (void)buffer;
    (void)address;
    return PROTO_STATUS_BAD_CMD;
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    (void)id;
    (void)value;
    return PROTO_STATUS_BAD_CMD;
}
