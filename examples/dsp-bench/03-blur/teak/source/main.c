// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the blur example.

#include <stddef.h>

#include <teak/teak.h>

#include "blur.h"
#include "proto_teak.h"

void app_init(void)
{
    blur_generate();
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
        case JOB_GAUSS_C:
            k_gauss5(img_src, img_tmp, img_gauss);
            break;
        case JOB_CONV_C:
            k_conv3x3(img_src, img_conv, &conv_params);
            break;
        case JOB_CONV_ASM:
            conv3x3_copy_border(img_src, img_conv);
            k_conv3x3_interior_asm(img_src, img_conv, &conv_params);
            break;
        case JOB_GAUSS_ASM:
            k_gauss5_asm(img_src, img_tmp, img_gauss);
            break;
        case JOB_GAUSS_B_ASM:
            k_gauss5b_asm(img_src, img_tmp, img_gauss);
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
    return blur_checksum(job);
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    switch (buffer)
    {
        case BUFFER_GAUSS:
            return proto_dma_out(img_gauss, address, IMG_SIZE);
        case BUFFER_CONV:
            return proto_dma_out(img_conv, address, IMG_SIZE);
        case BUFFER_TMP:
            return proto_dma_out(img_tmp, address, IMG_SIZE);
        default:
            return PROTO_STATUS_BAD_ARG;
    }
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    (void)id;
    (void)value;
    return PROTO_STATUS_BAD_CMD;
}
