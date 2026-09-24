// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the collision example, built for both CPUs.

#ifndef COLLISION_H__
#define COLLISION_H__

#include <stdint.h>

#include "proto.h"

// Number of pairs of objects tested by each kernel
#define N_PAIRS         512
#define MASK_WORDS      (N_PAIRS / 16)

// Vector in Q3.12 (same layout as vec3_f16 of math_f16.h)
typedef struct {
    int16_t x, y, z;
} kvec3;

// Sphere in Q3.12: center and radius. It is also used as an axis-aligned box
// (center and half size).
typedef struct {
    kvec3 c;
    int16_t r;
} ksphere;

// Sphere in Q16.16
typedef struct {
    int32_t x, y, z;
    int32_t r;
} ksphere_q16;

// Inputs (the same values are generated on both CPUs)
extern kvec3 vec_a[N_PAIRS], vec_b[N_PAIRS];
extern ksphere sph_a[N_PAIRS], sph_b[N_PAIRS];      // Centers = vec_a/vec_b
extern ksphere_q16 sph_a_q16[N_PAIRS], sph_b_q16[N_PAIRS];

// Outputs
extern int32_t out_dot[N_PAIRS];
extern int32_t out_cross[N_PAIRS * 3];
extern uint16_t out_sphere[MASK_WORDS];
extern uint16_t out_aabb[MASK_WORDS];
extern int32_t out_dot_q16[N_PAIRS];
extern uint16_t out_sphere_q16[MASK_WORDS];

// Jobs run by the DSP (operand of PROTO_CMD_RUN)
typedef enum {
    JOB_DOT3_C,
    JOB_DOT3_ASM,
    JOB_CROSS3_C,
    JOB_SPHERE_C,
    JOB_AABB_C,
    JOB_DOT3_Q16_C,
    JOB_SPHERE_Q16_C,
    JOB_INPUTS,         // Does nothing: its checksum covers the inputs
    JOB_CROSS3_ASM,     // DSP assembly versions (teak/source/collision_asm.s)
    JOB_SPHERE_ASM,
    JOB_AABB_ASM,
    JOB_DOT3_Q16_ASM,
    JOB_SPHERE_Q16_ASM,
    JOB_COUNT
} collision_job;

void collision_generate(void);

// C kernels. The Teak compiler doesn't support functions with more than 4
// arguments yet, so no kernel has more.
void k_dot3(const kvec3 *a, const kvec3 *b, int32_t *out, int n);
void k_cross3(const kvec3 *a, const kvec3 *b, int32_t *out, int n);
void k_sphere(const ksphere *a, const ksphere *b, uint16_t *mask, int n);
void k_aabb(const ksphere *a, const ksphere *b, uint16_t *mask, int n);
void k_dot3_q16(const ksphere_q16 *a, const ksphere_q16 *b, int32_t *out,
                int n);
void k_sphere_q16(const ksphere_q16 *a, const ksphere_q16 *b, uint16_t *mask,
                  int n);

// DSP assembly versions. The mask kernels require n = 16 * k.
void k_dot3_asm(const kvec3 *a, const kvec3 *b, int32_t *out, uint16_t n);
void k_cross3_asm(const kvec3 *a, const kvec3 *b, int32_t *out, uint16_t n);
void k_sphere_asm(const ksphere *a, const ksphere *b, uint16_t *mask,
                  uint16_t n);
void k_aabb_asm(const ksphere *a, const ksphere *b, uint16_t *mask, uint16_t n);
void k_dot3_q16_asm(const ksphere_q16 *a, const ksphere_q16 *b, int32_t *out,
                    uint16_t n);
void k_sphere_q16_asm(const ksphere_q16 *a, const ksphere_q16 *b,
                      uint16_t *mask, uint16_t n);

// Checksum of the output of a job
uint32_t collision_checksum(collision_job job);

#endif // COLLISION_H__
