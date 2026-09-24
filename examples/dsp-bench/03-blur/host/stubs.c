// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include "blur.h"

void k_conv3x3_interior_asm(const uint16_t *src, uint16_t *dst,
                            const conv3x3_params *p)
{
    k_conv3x3(src, dst, p);
}

void k_gauss5_asm(const uint16_t *src, uint16_t *tmp, uint16_t *dst)
{
    k_gauss5(src, tmp, dst);
}

void k_gauss5b_asm(const uint16_t *src, uint16_t *tmp, uint16_t *dst)
{
    k_gauss5(src, tmp, dst);
}

uint16_t proto_dma_out(const void *src, uint32_t address, uint16_t words)
{
    (void)src;
    (void)address;
    (void)words;
    return 0;
}
