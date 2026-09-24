// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the DCT example.

#include <stddef.h>

#include <teak/teak.h>

#include "dct.h"
#include "proto_teak.h"

void app_init(void)
{
    dct_generate();
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
        case JOB_DCT_C:
            k_dct_image(DCT8_C);
            break;
        case JOB_DCT_ASM:
            k_dct_image(DCT8_ASM);
            break;
        case JOB_IDCT_C:
            k_idct_image(DCT8_C);
            break;
        case JOB_IDCT_ASM:
            k_idct_image(DCT8_ASM);
            break;
        case JOB_DCT_IMAGE_ASM:
            k_dct_image_asm(img_src, img_coef);
            break;
        case JOB_IDCT_IMAGE_ASM:
            k_idct_image_asm(img_coef, img_recon);
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
    return dct_checksum(job);
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    if (buffer == BUFFER_RECON)
        return proto_dma_out(img_recon, address, IMG_SIZE);
    return PROTO_STATUS_BAD_ARG;
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    (void)id;
    (void)value;
    return PROTO_STATUS_BAD_CMD;
}
