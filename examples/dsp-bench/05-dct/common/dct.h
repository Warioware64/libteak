// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the DCT example, built for both CPUs.
//
// 8x8 separable integer DCT and IDCT of the 64 blocks of a 64x64 image of
// 12-bit luminance, with a Q14 cosine matrix. The first pass keeps 2 extra
// fractional bits. Every intermediate sum fits in 32 bits.

#ifndef DCT_H__
#define DCT_H__

#include <stdint.h>

#include "proto.h"

#define IMG_W           64
#define IMG_H           64
#define IMG_SIZE        (IMG_W * IMG_H)
#define IMG_MAX         4095

#define DCT_SHIFT_1     12      // 14 - 2 extra bits
#define DCT_SHIFT_2     16      // 14 + 2 extra bits

extern uint16_t img_src[IMG_SIZE];
extern int16_t img_coef[IMG_SIZE];      // 64 blocks of 64 coefficients
extern uint16_t img_recon[IMG_SIZE];    // IDCT of img_coef

// DCT matrix (row u = frequency) and its transpose, Q14
extern int16_t dct_matrix[64];
extern int16_t idct_matrix[64];

typedef enum {
    JOB_DCT_C,
    JOB_DCT_ASM,
    JOB_IDCT_C,
    JOB_IDCT_ASM,
    JOB_INPUTS,     // Does nothing: its checksum covers the inputs
    JOB_DCT_IMAGE_ASM,  // Whole image in DSP assembly
    JOB_IDCT_IMAGE_ASM,
    JOB_COUNT
} dct_job;

typedef enum {
    BUFFER_RECON,
} dct_buffer;

void dct_generate(void);

// 8-point transform: out[8 * u] = (sum_x mat[8 * u + x] * vec[x] + round)
// >> shift, for u = 0..7 (the output is stored with a stride of 8 words)
void dct8(const int16_t *mat, const int16_t *vec, int16_t *out, uint16_t shift);

// The same, as a function type-compatible with the assembly version, used by
// the whole-image transforms below
typedef enum {
    DCT8_C,
    DCT8_ASM,
} dct8_impl;

void k_dct_image(dct8_impl impl);   // img_src -> img_coef
void k_idct_image(dct8_impl impl);  // img_coef -> img_recon

// Whole-image DSP assembly versions
void k_dct_image_asm(const uint16_t *src, int16_t *coef);
void k_idct_image_asm(const int16_t *coef, uint16_t *recon);

uint32_t dct_checksum(dct_job job);

#endif // DCT_H__
