// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include "fft.h"

void fft_bfly_asm(kcplx *x, const int16_t *w, uint16_t half, uint16_t groups)
{
    fft_bfly(x, w, half, groups);
}

void k_fft2_asm(const kcplx *in, kcplx *out)
{
    k_fft(in, out);
}

uint16_t proto_dma_out(const void *src, uint32_t address, uint16_t words)
{
    (void)src;
    (void)address;
    (void)words;
    return 0;
}
