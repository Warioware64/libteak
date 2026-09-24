// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the skinning example, built for both CPUs.
//
// Each vertex is transformed by two of the 16 bones and the results are
// blended with the weights of the vertex:
//
//   out = (w0 * (M[b0] * v) + w1 * (M[b1] * v)) >> 12     w1 = 4096 - w0
//
// Bones are 4x3 matrices (3x3 rotation/scale + translation) in Q3.12.

#ifndef SKINNING_H__
#define SKINNING_H__

#include <stdint.h>

#include "proto.h"

#define N_VERTS         256
#define N_BONES         16

// Vertex: position in Q3.12, bone indices (b0 | b1 << 8), weight of b0 in Q0.12
typedef struct {
    int16_t x, y, z;
    uint16_t bones;
    int16_t w0;
} kvert;

// Rows of 4 values: m[4 * row + 0..2] = rotation, m[4 * row + 3] = translation
typedef struct {
    int16_t m[12];
} kbone;

typedef struct {
    int16_t x, y, z;
} kvec3;

// Q16.16 versions (weight in Q16.16 too)
typedef struct {
    int32_t x, y, z;
    int32_t w0;
    uint16_t bones;
} kvert_q16;

typedef struct {
    int32_t m[12];
} kbone_q16;

typedef struct {
    int32_t x, y, z;
} kvec3_q16;

extern kvert verts[N_VERTS];
extern kbone bones[N_BONES];
extern kvec3 out_verts[N_VERTS];

extern kvert_q16 verts_q16[N_VERTS];
extern kbone_q16 bones_q16[N_BONES];
extern kvec3_q16 out_verts_q16[N_VERTS];

typedef enum {
    JOB_SKIN_C,
    JOB_SKIN_ASM,
    JOB_SKIN_Q16_C,
    JOB_INPUTS,     // Does nothing: its checksum covers the inputs
    JOB_SKIN2_ASM,  // Faster DSP assembly version
    JOB_SKIN_Q16_ASM,
    JOB_COUNT
} skinning_job;

void skinning_generate(void);

void k_skin(const kvert *v, const kbone *b, kvec3 *out, int n);
void k_skin_q16(const kvert_q16 *v, const kbone_q16 *b, kvec3_q16 *out, int n);

// DSP assembly versions (n >= 1)
void k_skin_asm(const kvert *v, const kbone *b, kvec3 *out, uint16_t n);
void k_skin2_asm(const kvert *v, const kbone *b, kvec3 *out, uint16_t n);
void k_skin_q16_asm(const kvert_q16 *v, const kbone_q16 *b, kvec3_q16 *out,
                    uint16_t n);

uint32_t skinning_checksum(skinning_job job);

#endif // SKINNING_H__
