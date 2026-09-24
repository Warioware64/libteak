// SPDX-License-Identifier: CC0-1.0
//
// Hand-written plasma (same results as k_plasma()).
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved.
//
// Most of the work of k_plasma() doesn't depend on both x and y, so it is done
// once per call in tables:
//
// - pl_sin4[i] = sin[i] >> 2 (sin[i] >> 10 is pl_sin4[i] >> 8)
// - pl_col[p]: color of phase p (the 3 channels only depend on p & 0xFF)
// - pl_sxs[x] = sx >> 9, pl_sx4[x] = sx >> 2, with sx = sin[(4 * x + t) & 0xFF]
// - pl_sd4[k] = sin[(2 * k + 3 * t) & 0xFF] >> 2, for k = x + y
//
// Then each pixel is 2 table lookups and a few additions.

#include <teak/asminc.h>

    .section .bss.k_plasma_asm, "aw", %nobits
    .align 2
pl_sin4:    .space 2 * 256
pl_col:     .space 2 * 256
pl_sxs:     .space 2 * 64
pl_sx4:     .space 2 * 64
pl_sd4:     .space 2 * 128
pl_t:       .space 2

// a0 = [base + (a0 & 0xFF)], with the base in a register
.macro LOOKUP base
    and     0xff, a0
    add     \base, a0
    mov     a0l, r3
    mov     [r3], a0
.endm

// a0 = channel(r7 + offset) = 16 + (sin[(r7 + offset) & 0xFF] >> 11)
.macro CHANNEL offset
    mov     r7, a0
    add     \offset, a0
    LOOKUP  r6
    shfi    a0, a0, -11
    add     16, a0
.endm

// void k_plasma_asm(uint16_t *dst, uint16_t t)

BEGIN_ASM_FUNC k_plasma_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6
    push    r7

    mov     a0l, r5                 // dst
    mov     a1l, [pl_t]

    // pl_sin4
    mov     sin_table, r0
    mov     pl_sin4, r1
    bkrep   255, 1f
    mov     [r0++], a0
    shfi    a0, a0, -2
    mov     a0l, [r1++]
1:

    // pl_col
    mov     sin_table, r6
    mov     pl_col, r1
    mov     0, r7                   // Phase
    bkrep   255, 1f
    CHANNEL 170
    shfi    a0, a1, 10
    CHANNEL 85
    shfi    a0, a0, 5
    or      a0, a1
    CHANNEL 0
    or      a0, a1
    or      0x8000, a1
    mov     a1l, [r1++]
    modr    [r7++]
1:

    // pl_sxs and pl_sx4
    mov     pl_sin4, r6
    mov     pl_sxs, r0
    mov     pl_sx4, r1
    mov     [pl_t], a1              // Index 4 * x + t
    bkrep   63, 1f
    mov     a1, a0
    LOOKUP  r6                      // sin >> 2
    mov     a0l, [r1++]
    shfi    a0, a0, -7              // sin >> 9
    mov     a0l, [r0++]
    add     4, a1
1:

    // pl_sd4
    mov     pl_sd4, r1
    mov     [pl_t], a1              // Index 2 * k + 3 * t
    shfi    a1, a0, 1
    add     a0, a1
    bkrep   126, 1f
    mov     a1, a0
    LOOKUP  r6
    mov     a0l, [r1++]
    add     2, a1
1:

    // Pixels
    mov     pl_col, r4
    mov     pl_sd4, r7              // pl_sd4 + y
    mov     [pl_t], a0              // Index of sy: 3 * y - 2 * t
    shfi    a0, a0, 1
    neg     a0, always
    mov     a0l, y0

    bkrep   63, 2f
    mov     y0, a0
    LOOKUP  r6
    mov     a0, b1                  // b1 = sy >> 2
    shfi    a0, a0, -8              // sy >> 10
    add     [pl_t], a0
    mov     a0, b0                  // b0 = (sy >> 10) + t
    mov     y0, a0
    add     3, a0
    mov     a0l, y0
    mov     pl_sxs, r0
    mov     pl_sx4, r1
    mov     r7, r2
    modr    [r7++]

    bkrep   63, 1f
    mov     [r0++], a0              // sx >> 9
    add     b0, a0
    LOOKUP  r6                      // sm >> 2
    add     [r1++], a0              // + sx >> 2
    add     b1, a0                  // + sy >> 2
    add     [r2++], a0              // + sd >> 2
    shfi    a0, a0, -8              // Phase
    LOOKUP  r4                      // Color
    mov     a0l, [r5++]
1:
    nop
2:

    pop     r7
    pop     r6
    pop     r5
    pop     r4
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always
