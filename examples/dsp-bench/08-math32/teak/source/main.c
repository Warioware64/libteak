// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the 32-bit math example.

#include <stddef.h>

#include <teak/teak.h>

#include "math32.h"
#include "proto_teak.h"

void app_init(void)
{
    math32_generate();
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
        case JOB_ALU_C:
            k_alu(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_MUL_C:
            k_mul(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_Q16MUL_C:
            k_q16mul(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_DIV_C:
            k_div(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_SQRT_C:
            k_sqrt(val_a, val_out, N_VALUES);
            break;
        case JOB_ALU_ASM:
            k_alu_asm(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_MUL_ASM:
            k_mul_asm(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_Q16MUL_ASM:
            k_q16mul_asm(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_DIV_ASM:
            k_div_asm(val_a, val_b, val_out, N_VALUES);
            break;
        case JOB_SQRT_ASM:
            k_sqrt_asm(val_a, val_out, N_VALUES);
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
    return math32_checksum(job);
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
