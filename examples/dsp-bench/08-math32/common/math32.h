// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the 32-bit math example, built for both CPUs.

#ifndef MATH32_H__
#define MATH32_H__

#include <stdint.h>

#include "proto.h"

#define N_VALUES        1024

extern int32_t val_a[N_VALUES];
extern int32_t val_b[N_VALUES];     // Never 0
extern int32_t val_out[N_VALUES];

typedef enum {
    JOB_ALU_C,          // Additions, subtractions, shifts, xor
    JOB_MUL_C,          // 32x32 -> 32 multiplication
    JOB_Q16MUL_C,       // Q16.16 multiplication
    JOB_DIV_C,          // Signed division (shift and subtract)
    JOB_SQRT_C,         // Integer square root
    JOB_INPUTS,         // Does nothing: its checksum covers the inputs
    JOB_ALU_ASM,        // DSP assembly versions (teak/source/math32_asm.s)
    JOB_MUL_ASM,
    JOB_Q16MUL_ASM,
    JOB_DIV_ASM,
    JOB_SQRT_ASM,
    JOB_COUNT
} math32_job;

void math32_generate(void);

void k_alu(const int32_t *a, const int32_t *b, int32_t *out, int n);
void k_mul(const int32_t *a, const int32_t *b, int32_t *out, int n);
void k_q16mul(const int32_t *a, const int32_t *b, int32_t *out, int n);
void k_div(const int32_t *a, const int32_t *b, int32_t *out, int n);
void k_sqrt(const int32_t *a, int32_t *out, int n);

// DSP assembly versions (n >= 1)
void k_alu_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n);
void k_mul_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n);
void k_q16mul_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n);
void k_div_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n);
void k_sqrt_asm(const int32_t *a, int32_t *out, uint16_t n);

uint32_t math32_checksum(math32_job job);

#endif // MATH32_H__
