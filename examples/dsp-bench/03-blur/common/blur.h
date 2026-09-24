// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the blur example, built for both CPUs.

#ifndef BLUR_H__
#define BLUR_H__

#include <stdint.h>

#include "proto.h"

#define IMG_W           64
#define IMG_H           64
#define IMG_SIZE        (IMG_W * IMG_H)
#define IMG_MAX         4095        // 12-bit luminance

// Parameters of the 3x3 convolution: out = clamp((sum k * p) >> shift)
typedef struct {
    int16_t k[9];
    int16_t shift;
} conv3x3_params;

extern uint16_t img_src[IMG_SIZE];
extern uint16_t img_tmp[IMG_SIZE];      // Horizontal pass of the Gaussian
extern uint16_t img_gauss[IMG_SIZE];
extern uint16_t img_conv[IMG_SIZE];
extern conv3x3_params conv_params;

typedef enum {
    JOB_GAUSS_C,
    JOB_CONV_C,
    JOB_CONV_ASM,
    JOB_INPUTS,     // Does nothing: its checksum covers the inputs
    JOB_GAUSS_ASM,
    JOB_GAUSS_B_ASM,    // Variant that avoids the forms of JOB_GAUSS_ASM
    JOB_COUNT
} blur_job;

// Output buffers that the DSP can send with PROTO_CMD_SEND
typedef enum {
    BUFFER_GAUSS,
    BUFFER_CONV,
    BUFFER_TMP,     // Horizontal pass of the Gaussian
} blur_buffer;

void blur_generate(void);

// Separable Gaussian [1 4 6 4 1] / 16, edges clamped
void k_gauss5(const uint16_t *src, uint16_t *tmp, uint16_t *dst);

// 3x3 convolution of the interior of the image. The border pixels of dst
// are a copy of src.
void k_conv3x3(const uint16_t *src, uint16_t *dst, const conv3x3_params *p);
void conv3x3_copy_border(const uint16_t *src, uint16_t *dst);

// DSP assembly versions
void k_conv3x3_interior_asm(const uint16_t *src, uint16_t *dst,
                            const conv3x3_params *p);
void k_gauss5_asm(const uint16_t *src, uint16_t *tmp, uint16_t *dst);
void k_gauss5b_asm(const uint16_t *src, uint16_t *tmp, uint16_t *dst);

uint32_t blur_checksum(blur_job job);

#endif // BLUR_H__
