// SPDX-License-Identifier: CC0-1.0
//
// Data and kernels of the texture example, built for both CPUs.
//
// Animated 64x64 RGB555 plasma, computed from a table of sines.

#ifndef TEXTURE_H__
#define TEXTURE_H__

#include <stdint.h>

#include "proto.h"

#define TEX_W           64
#define TEX_H           64
#define TEX_SIZE        (TEX_W * TEX_H)

// sin(2 pi i / 256) in Q15, uploaded by the ARM9 (table TABLE_SIN)
#define TABLE_SIN       0
#define SIN_ENTRIES     256
extern int16_t sin_table[SIN_ENTRIES];

extern uint16_t texture[TEX_SIZE];

// Frame to compute (parameter PARAM_FRAME)
#define PARAM_FRAME     0
extern uint16_t plasma_frame;

typedef enum {
    JOB_PLASMA_C,
    JOB_PLASMA_ASM,     // teak/source/texture_asm.s
    JOB_COUNT
} texture_job;

// Output buffers that the DSP can send with PROTO_CMD_SEND
typedef enum {
    BUFFER_TEXTURE_VRAM,    // Into a 256-pixel wide 16-bit bitmap
} texture_buffer;

// Width of the bitmap that receives the texture, in pixels
#define VRAM_BITMAP_W   256

void k_plasma(uint16_t *dst, uint16_t frame);

// DSP assembly version
void k_plasma_asm(uint16_t *dst, uint16_t frame);

uint32_t texture_checksum(texture_job job);

#endif // TEXTURE_H__
