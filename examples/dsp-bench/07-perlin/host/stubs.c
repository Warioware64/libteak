// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include <stdint.h>

#include "perlin.h"

void k_noise_asm(uint16_t *dst)
{
    k_noise(dst, 1);
}

void k_fbm_asm(uint16_t *dst)
{
    k_noise(dst, 4);
}

void k_noise_q16_asm(uint16_t *dst)
{
    k_noise_q16(dst, 1);
}

void k_fbm_q16_asm(uint16_t *dst)
{
    k_noise_q16(dst, 4);
}

uint16_t proto_dma_out(const void *src, uint32_t address, uint16_t words)
{
    (void)src;
    (void)address;
    (void)words;
    return 0;
}
