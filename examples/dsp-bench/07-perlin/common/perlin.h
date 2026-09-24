// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the Perlin noise example, built for both CPUs.
//
// 2D gradient noise on a 64x64 image, in Q8 and in Q16.16: one octave, and
// fractal Brownian motion (fBm) of 4 octaves.

#ifndef PERLIN_H__
#define PERLIN_H__

#include <stdint.h>

#include "proto.h"

#define IMG_W           64
#define IMG_H           64
#define IMG_SIZE        (IMG_W * IMG_H)
#define IMG_MAX         4095

// Tables uploaded by the ARM9
#define TABLE_PERM      0       // Permutation of 0..255
#define TABLE_GRAD      1       // 16 gradients (x, y), unit length in Q8
#define PERM_ENTRIES    256
#define GRAD_ENTRIES    16

extern uint16_t perm[PERM_ENTRIES];
extern int16_t grad[GRAD_ENTRIES * 2];

// Outputs: 12-bit gray images
extern uint16_t img_noise[IMG_SIZE];
extern uint16_t img_fbm[IMG_SIZE];
extern uint16_t img_noise_q16[IMG_SIZE];
extern uint16_t img_fbm_q16[IMG_SIZE];

typedef enum {
    JOB_NOISE_C,
    JOB_FBM_C,
    JOB_NOISE_Q16_C,
    JOB_FBM_Q16_C,
    JOB_INPUTS,     // Does nothing: its checksum covers the tables
    JOB_NOISE_ASM,  // DSP assembly versions (teak/source/perlin_asm.s)
    JOB_FBM_ASM,
    JOB_NOISE_Q16_ASM,
    JOB_FBM_Q16_ASM,
    JOB_COUNT
} perlin_job;

typedef enum {
    BUFFER_FBM,
} perlin_buffer;

void k_noise(uint16_t *dst, int octaves);
void k_noise_q16(uint16_t *dst, int octaves);

// DSP assembly versions: 1 octave (noise) or 4 octaves (fbm)
void k_noise_asm(uint16_t *dst);
void k_fbm_asm(uint16_t *dst);
void k_noise_q16_asm(uint16_t *dst);
void k_fbm_q16_asm(uint16_t *dst);

uint32_t perlin_checksum(perlin_job job);

#endif // PERLIN_H__
