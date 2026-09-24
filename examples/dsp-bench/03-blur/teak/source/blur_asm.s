// SPDX-License-Identifier: CC0-1.0
//
// Hand-written DSP 3x3 convolution (same results as the interior of
// k_conv3x3()).
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved. The forms of "mpy"/"mac" with
// two memory operands aren't used (the second operand is read through the Y
// bus, which doesn't see the same memory on DSi hardware).

#include <teak/asminc.h>

#define IMG_W   64
#define IMG_H   64

// One row of the 3x3 window: r0 = coefficients, r1 = pixels
.macro TAP3
    mov     [r0++], y0
    mac     y0, [r1++], a0
    mov     [r0++], y0
    mac     y0, [r1++], a0
    mov     [r0++], y0
    mac     y0, [r1++], a0
.endm

// void k_conv3x3_interior_asm(const uint16_t *src, uint16_t *dst,
//                             const conv3x3_params *p)
//
// Only writes the interior of dst (the caller copies the border).

BEGIN_ASM_FUNC k_conv3x3_interior_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4

    mov     a0l, r3                 // Top-left of the window of pixel (1, 1)
    mov     a1l, r2
    addv    IMG_W + 1, r2           // Output pixel (1, 1)

    mov     b0l, r4                 // Coefficients k[0..8]
    mov     b0l, r0
    addv    9, r0
    mov     [r0], a1                // shift
    xor     0xffff, a1              // sv = -shift (shift right)
    add     0x1, a1
    mov     a1l, sv

    mov     IMG_H - 3, b1l          // IMG_H - 2 rows ("bkrep" runs count + 1)
    bkrep   b1l, 5f

    mov     IMG_W - 3, a1l          // IMG_W - 2 pixels per row
    bkrep   a1l, 4f

    mov     r4, r0
    mov     r3, r1
    clr     a0, always
    clr0                            // p0 = 0, so the first "mac" adds nothing
    TAP3
    addv    IMG_W - 3, r1
    TAP3
    addv    IMG_W - 3, r1
    TAP3
    add     p*, a0

    shfc    a0, a0, always          // acc >> shift

    cmp     0x0, a0                 // Clamp to [0, 4095]
    clr     a0, lt
    cmp     0xfff, a0
    brr     2f, le
    mov     0xfff, a0
2:
    mov     a0l, [r2++]
    modr    [r3++]                  // Next window
4:
    modr    r3, +2                  // Skip the border: next row
    modr    r2, +2
5:

    pop     r4
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always

// Separable Gaussian
// ------------------

// One output of the [1 4 6 4 1] / 16 filter, stored at [r5++]:
//
//   (a + e + 4 * (b + c + d) + 2 * c + 8) >> 4
//
// The arguments are memory operands. c is read twice: c1 must not modify the
// pointer, c2 may.
.macro GAUSS a, b, c1, c2, d, e
    mov     \a, a0
    add     \e, a0
    mov     \b, a1
    add     \c1, a1
    add     \d, a1
    shfi    a1, a1, 2
    add     a1, a0
    mov     \c2, a1
    shfi    a1, a1, 1
    add     a1, a0
    add     8, a0
    shfi    a0, a0, -4
    mov     a0l, [r5++]
.endm

// r0-r4 = r6 + 64 * (rows given), then "count" outputs of the vertical pass
.macro VSEG k0, k1, k2, k3, k4, count
    mov     r6, r0
    addv    64 * \k0, r0
    mov     r6, r1
    addv    64 * \k1, r1
    mov     r6, r2
    addv    64 * \k2, r2
    mov     r6, r3
    addv    64 * \k3, r3
    mov     r6, r4
    addv    64 * \k4, r4
    mov     \count - 1, b1
    bkrep   b1l, 3f
    GAUSS   [r0++], [r1++], [r2], [r2++], [r3++], [r4++]
3:
.endm

// void k_gauss5_asm(const uint16_t *src, uint16_t *tmp, uint16_t *dst)
//
// Same result as k_gauss5() for a 64x64 image.
//
// Horizontal pass: in each row, the pixels 2 to 61 use 5 pointers that
// advance together. The 2 pixels at each edge read the row with r7 + offset,
// with the clamped coordinates.
//
// On DSi hardware, an [r7 + offset] operand just after an instruction that
// computes r7 (here "addv") reads the old value of r7, hence the "nop" after
// "addv 64, r7" (teakra doesn't model this; see 00-asm-probe).
//
// Vertical pass: the rows 2 to 61 are a single loop of 60 * 64 pixels, as the
// 5 pointers move to the next row by themselves. The 2 rows at each edge use
// clamped row pointers.

BEGIN_ASM_FUNC k_gauss5_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6
    push    r7

    mov     a0l, r7                 // Row of src
    mov     a1l, r5                 // Output (tmp)
    mov     a1l, y0                 // Keep tmp for the vertical pass

    bkrep   63, 1f
    GAUSS   [r7], [r7], [r7], [r7], [r7+0x1], [r7+0x2]
    GAUSS   [r7], [r7], [r7+0x1], [r7+0x1], [r7+0x2], [r7+0x3]
    mov     r7, r0
    mov     r7, r1
    modr    [r1++]
    mov     r7, r2
    addv    2, r2
    mov     r7, r3
    addv    3, r3
    mov     r7, r4
    addv    4, r4
    bkrep   59, 2f
    GAUSS   [r0++], [r1++], [r2], [r2++], [r3++], [r4++]
2:
    GAUSS   [r7+0x3c], [r7+0x3d], [r7+0x3e], [r7+0x3e], [r7+0x3f], [r7+0x3f]
    addv    64, r7                  // Next row (row + 61 is now r7 - 3)
    nop                             // See below
    GAUSS   [r7-0x3], [r7-0x2], [r7-0x1], [r7-0x1], [r7-0x1], [r7-0x1]
1:

    mov     y0, r6                  // tmp
    mov     b0l, r5                 // dst
    VSEG    0, 0, 0, 1, 2, 64
    VSEG    0, 0, 1, 2, 3, 64
    VSEG    0, 1, 2, 3, 4, 60 * 64
    VSEG    60, 61, 62, 63, 63, 64
    VSEG    61, 62, 63, 63, 63, 64

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

// Gaussian, variant B
// -------------------
//
// Same result as k_gauss5_asm(), written only with forms that the other
// kernels had already used on DSi hardware when k_gauss5_asm() was found to
// give wrong results there (the cause was the r7 + offset operand after
// "addv"). It doesn't use r7 + offset operands, loop counts loaded with
// "mov imm16" or loops longer than 256 iterations.
//
// Horizontal pass: each row is copied into gb_pad with 2 clamped pixels at
// each end, then one loop of 64 outputs. Vertical pass: the 5 row pointers are
// computed for each row with clamped row numbers.

    .section .bss.k_gauss5b_asm, "aw", %nobits
    .align 2
gb_pad:     .space 2 * 68

// rK = r6 + 64 * clamp(y + k, 0, 63), with y in y0 and b0 = 63
.macro ROW_PTR k, reg
    mov     y0, a0
    add     \k, a0
    clr     a0, lt
    sub     b0, a0
    clr     a0, gt
    add     b0, a0
    shfi    a0, a0, 6
    add     r6, a0
    mov     a0l, \reg
.endm

// void k_gauss5b_asm(const uint16_t *src, uint16_t *tmp, uint16_t *dst)

BEGIN_ASM_FUNC k_gauss5b_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6
    push    r7

    mov     a0l, r6                 // Row of src
    mov     a1l, r5                 // Output (tmp)
    mov     a1l, y0                 // Keep tmp for the vertical pass
    mov     b0l, r7                 // dst

    bkrep   63, 1f
    // Padded copy of the row
    mov     r6, r0
    mov     gb_pad, r1
    mov     [r0], a0
    mov     a0l, [r1++]
    mov     a0l, [r1++]
    bkrep   63, 2f
    mov     [r0++], a0
    mov     a0l, [r1++]
2:
    mov     a0l, [r1++]             // a0 = last pixel of the row
    mov     a0l, [r1++]
    addv    64, r6                  // Next row

    mov     gb_pad, r0
    mov     gb_pad + 1, r1
    mov     gb_pad + 2, r2
    mov     gb_pad + 3, r3
    mov     gb_pad + 4, r4
    nop
    bkrep   63, 3f
    GAUSS   [r0++], [r1++], [r2], [r2++], [r3++], [r4++]
3:
    nop
1:

    // Vertical pass
    mov     r7, r5                  // dst
    mov     y0, r6                  // tmp
    mov     0xfffe, a0              // y - 2 (y0 holds y - 2 below)
    mov     a0l, y0
    mov     63, b0

    bkrep   63, 1f
    ROW_PTR 0, r0
    ROW_PTR 1, r1
    ROW_PTR 2, r2
    ROW_PTR 3, r3
    ROW_PTR 4, r4
    mov     y0, a0                  // Next row
    add     1, a0
    mov     a0l, y0
    bkrep   63, 2f
    GAUSS   [r0++], [r1++], [r2], [r2++], [r3++], [r4++]
2:
    nop
1:

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
