// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include <stdint.h>

#include "dct.h"

void k_dct_image_asm(const uint16_t *src, int16_t *coef)
{
    (void)src;
    (void)coef;
    k_dct_image(DCT8_C);
}

void k_idct_image_asm(const int16_t *coef, uint16_t *recon)
{
    (void)coef;
    (void)recon;
    k_idct_image(DCT8_C);
}

uint16_t proto_dma_out(const void *src, uint32_t address, uint16_t words)
{
    (void)src;
    (void)address;
    (void)words;
    return 0;
}
