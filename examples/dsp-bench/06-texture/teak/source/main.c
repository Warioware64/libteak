// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the texture example.

#include <stddef.h>

#include <teak/teak.h>

#include "proto_teak.h"
#include "texture.h"

void app_init(void)
{
}

uint16_t *app_upload_buffer(uint16_t table, uint16_t words)
{
    if ((table == TABLE_SIN) && (words <= SIN_ENTRIES))
        return (uint16_t *)sin_table;
    return NULL;
}

uint16_t app_job(uint16_t job)
{
    if (job == JOB_PLASMA_C)
        k_plasma(texture, plasma_frame);
    else if (job == JOB_PLASMA_ASM)
        k_plasma_asm(texture, plasma_frame);
    else
        return PROTO_STATUS_BAD_ARG;

    return PROTO_STATUS_OK;
}

uint32_t app_checksum(uint16_t job)
{
    return texture_checksum(job);
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    if (buffer != BUFFER_TEXTURE_VRAM)
        return PROTO_STATUS_BAD_ARG;

    // One transfer per row: the rows of the bitmap are VRAM_BITMAP_W pixels
    // apart
    for (uint16_t y = 0; y < TEX_H; y++)
    {
        uint16_t status = proto_dma_out(&texture[y * TEX_W], address, TEX_W);
        if (status != PROTO_STATUS_OK)
            return status;
        address += VRAM_BITMAP_W * 2;
    }

    return PROTO_STATUS_OK;
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    if (id != PARAM_FRAME)
        return PROTO_STATUS_BAD_ARG;

    plasma_frame = (uint16_t)value;
    return PROTO_STATUS_OK;
}
