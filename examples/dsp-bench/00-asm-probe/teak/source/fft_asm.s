// SPDX-License-Identifier: CC0-1.0
//
// Hand-written FFT butterflies (same results as fft_bfly()).
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved. The forms of "mpy"/"mac" with
// two memory operands aren't used (the second operand is read through the Y
// bus, which doesn't see the same memory on DSi hardware).

#include <teak/asminc.h>

    .section .bss.fft_bfly_asm, "aw", %nobits
    .align 2
fft_w:          .space 8        // c, s, c, -s
fft_stride:     .space 2        // 4 * half - 1 (words)

// void fft_bfly_asm(kcplx *x, const int16_t *w, uint16_t half, uint16_t groups)
//
// Requires groups >= 1. For each butterfly (a = x, b = x + half):
//
//   tr = (b.re * c + b.im * s) >> 15      ti = (b.im * c - b.re * s) >> 15
//   a = ((a.re + tr) >> 1, (a.im + ti) >> 1)
//   b = ((a.re - tr) >> 1, (a.im - ti) >> 1)

BEGIN_ASM_FUNC fft_bfly_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3

    mov     a0l, r0                 // a
    mov     a1l, r2                 // w

    mov     b0l, a0                 // half
    shfi    a0, a0, 1               // 2 * half words = distance from a to b
    mov     r0, a1
    add     a0, a1
    mov     a1l, r1                 // b
    shfi    a0, a0, 1               // 4 * half words = distance between groups
    sub     0x1, a0
    mov     a0l, [fft_stride]

    mov     fft_w, r3               // Table c, s, c, -s
    mov     [r2++], a0
    mov     a0l, [r3++]
    mov     [r2], a1
    mov     a1l, [r3++]
    mov     a0l, [r3++]
    xor     0xffff, a1
    add     0x1, a1
    mov     a1l, [r3]

    addv    0xffff, b1l             // "bkrep" runs its block count + 1 times
    bkrep   b1l, 9f

    mov     fft_w, r2

    clr     a0, always              // tr
    mov     [r2++], y0              // c
    mpy     y0, [r1++], a0          // p0 = c * b.re
    mov     [r2++], y0              // s
    mac     y0, [r1], a0            // a0 = c * b.re, p0 = s * b.im
    add     p*, a0

    clr     a1, always              // ti
    mov     [r2++], y0              // c
    mpy     y0, [r1--], a1          // p0 = c * b.im
    mov     [r2++], y0              // -s
    mac     y0, [r1], a1            // a1 = c * b.im, p0 = -s * b.re
    add     p*, a1

    shfi    a0, a0, -15
    shfi    a1, a1, -15

    mov     [r0], b0                // b.re = (a.re - tr) >> 1
    sub     a0, b0
    shfi    b0, b0, -1
    mov     b0l, [r1++]
    mov     [r0], b0                // a.re = (a.re + tr) >> 1
    add     a0, b0
    shfi    b0, b0, -1
    mov     b0l, [r0++]
    mov     [r0], b0                // b.im = (a.im - ti) >> 1
    sub     a1, b0
    shfi    b0, b0, -1
    mov     b0l, [r1]
    mov     [r0], b0                // a.im = (a.im + ti) >> 1
    add     a1, b0
    shfi    b0, b0, -1
    mov     b0l, [r0]

    mov     r0, a0                  // Next group: a += 4 * half, b += 4 * half
    add     [fft_stride], a0
    mov     a0l, r0
    mov     r1, a0
    add     [fft_stride], a0
    mov     a0l, r1
9:

    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always
