// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the collision example.

#include <stddef.h>

#include <teak/teak.h>

#include "collision.h"
#include "proto_teak.h"

void app_init(void)
{
    collision_generate();
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
        case JOB_DOT3_C:
            k_dot3(vec_a, vec_b, out_dot, N_PAIRS);
            break;
        case JOB_DOT3_ASM:
            k_dot3_asm(vec_a, vec_b, out_dot, N_PAIRS);
            break;
        case JOB_CROSS3_C:
            k_cross3(vec_a, vec_b, out_cross, N_PAIRS);
            break;
        case JOB_SPHERE_C:
            k_sphere(sph_a, sph_b, out_sphere, N_PAIRS);
            break;
        case JOB_AABB_C:
            k_aabb(sph_a, sph_b, out_aabb, N_PAIRS);
            break;
        case JOB_DOT3_Q16_C:
            k_dot3_q16(sph_a_q16, sph_b_q16, out_dot_q16, N_PAIRS);
            break;
        case JOB_SPHERE_Q16_C:
            k_sphere_q16(sph_a_q16, sph_b_q16, out_sphere_q16, N_PAIRS);
            break;
        case JOB_CROSS3_ASM:
            k_cross3_asm(vec_a, vec_b, out_cross, N_PAIRS);
            break;
        case JOB_SPHERE_ASM:
            k_sphere_asm(sph_a, sph_b, out_sphere, N_PAIRS);
            break;
        case JOB_AABB_ASM:
            k_aabb_asm(sph_a, sph_b, out_aabb, N_PAIRS);
            break;
        case JOB_DOT3_Q16_ASM:
            k_dot3_q16_asm(sph_a_q16, sph_b_q16, out_dot_q16, N_PAIRS);
            break;
        case JOB_SPHERE_Q16_ASM:
            k_sphere_q16_asm(sph_a_q16, sph_b_q16, out_sphere_q16, N_PAIRS);
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
    return collision_checksum(job);
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
