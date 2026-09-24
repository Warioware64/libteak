// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include <stdint.h>

#include "texture.h"

void k_plasma_asm(uint16_t *dst, uint16_t frame)
{
    k_plasma(dst, frame);
}

uint16_t proto_dma_out(const void *src, uint32_t address, uint16_t words)
{
    (void)src;
    (void)address;
    (void)words;
    return 0;
}
