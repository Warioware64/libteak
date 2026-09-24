// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the FFT example, built for both CPUs.
//
// 256-point complex radix-2 decimation-in-time FFT in Q15. Every stage
// divides the values by 2, so the output is the DFT divided by 256.

#ifndef FFT_H__
#define FFT_H__

#include <stdint.h>

#include "proto.h"

#define FFT_N           256

typedef struct {
    int16_t re, im;
} kcplx;

// Twiddle factors, uploaded by the ARM9 (table TABLE_TWIDDLE):
// twiddle[2 * k] = cos(2 pi k / N), twiddle[2 * k + 1] = sin(2 pi k / N) in
// Q15, for k < N / 2.
#define TABLE_TWIDDLE   0
#define TWIDDLE_WORDS   FFT_N
extern int16_t twiddle[TWIDDLE_WORDS];

// Bit reversal of the indices (built by fft_setup())
extern uint16_t fft_bitrev[FFT_N];

extern kcplx fft_in[FFT_N];
extern kcplx fft_out[FFT_N];

// Number of stages run by JOB_FFT_ASM (parameter PARAM_STAGES, 0 to 8,
// default 8), to find the first stage that gives a different result
#define PARAM_STAGES    0

typedef enum {
    JOB_SETUP,      // Builds the input from the twiddle factors
    JOB_FFT_C,
    JOB_FFT_ASM,
    JOB_FFT2_ASM,   // Whole FFT in assembly
    JOB_COUNT
} fft_job;

// Builds the bit reversal table and the input signal (needs the twiddles)
void fft_setup(void);

// FFT of "in" into "out"
void k_fft(const kcplx *in, kcplx *out);

// Same, but only the bit reversed copy and the first "stages" stages
void k_fft_stages(const kcplx *in, kcplx *out, uint16_t stages);

// Copies "in" into "out" in bit-reversed order (first step of k_fft())
void fft_bitrev_copy(const kcplx *in, kcplx *out);

// All butterflies with twiddle factor (c, s) = w[0..1] of one stage: "x"
// points to the first element, "half" is the distance between the two
// elements of a butterfly, and there are "groups" butterflies, 2 * half
// elements apart.
void fft_bfly(kcplx *x, const int16_t *w, uint16_t half, uint16_t groups);

// Whole FFT in DSP assembly
void k_fft2_asm(const kcplx *in, kcplx *out);

uint32_t fft_checksum(fft_job job);

#endif // FFT_H__
