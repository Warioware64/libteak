// SPDX-License-Identifier: CC0-1.0
//
// Hand-written Perlin noise (same results as k_noise() and k_noise_q16()).
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved.
//
// Most of the work of noise8() doesn't depend on both coordinates:
//
// - For octave o, x = px << (4 + o) (Q8), so the cell (ix = px >> (4 - o)),
//   the fraction fx and fade(fx) only depend on px. The same values are used
//   for y. They are computed once per call in tables (pn_fx and pn_fade, 64
//   entries per octave).
// - In a row, all pixels of a cell use the same 4 gradients, and the y terms
//   of the 4 gradient dot products only depend on the row. They are computed
//   once per cell (pn_cell): gx and a constant for each corner.
// - mul8(g, fx - 256) is mul8(g, fx) - g, as 256 * g has no bits below bit 8.
//   The constants include these -g terms, so each pixel only needs the 4
//   products gx * fx.
//
// Then each pixel of each octave is 4 products for the gradient dots and 3
// for the interpolations. The octave sums of a row are kept in pn_sum, and
// converted to gray at the end of the row.

#include <teak/asminc.h>

    .section .bss.perlin_asm, "aw", %nobits
    .align 2
pn_fx:      .space 2 * 256      // fx for 4 octaves of 64 coordinates
pn_fade:    .space 2 * 256      // fade(fx), must follow pn_fx
pn_cell:    .space 2 * 8        // gx and constant of the 4 corners
pn_sum:     .space 2 * 64       // Sum of the octaves of a row
pn_py:      .space 2
pn_iy:      .space 2
pn_dst:     .space 2

// a0 = (p * 1) >> 8, the product being the last one started
.macro P_SHR8
    clr     a0, always
    add     p*, a0
    shfi    a0, a0, -8
.endm

// fade8(t) in a0, with t in r3 (0 to 255). Uses b0 and y0.
//
//   f = mul8(t, 6t - 15 * 256) + 10 * 256
//   fade = mul8(mul8(mul8(t, t), t), f)
.macro FADE8
    mov     r3, y0
    mov     r3, a0
    shfi    a0, a0, 1               // 2t
    shfi    a0, b0, 1               // 4t
    add     b0, a0                  // 6t
    sub     0xf00, a0
    mpy     y0, a0l, a0
    P_SHR8
    add     0xa00, a0
    mov     a0, b0                  // f
    mpy     y0, r3, a0
    P_SHR8                          // t2 = mul8(t, t)
    mov     a0l, y0
    mpy     y0, r3, a0
    P_SHR8                          // t3 = mul8(t2, t)
    mov     a0l, y0
    mpy     y0, b0l, a0
    P_SHR8
.endm

// Tables of one octave: fx = (p << (4 + o)) & 0xFF and fade8(fx), for p =
// 0..63. r0 and r1 point to the entries of the octave.
.macro OCTAVE_TABLES8 o
    clr     a1, always
    bkrep   63, 1f
    mov     a1, a0
    and     0xff, a0
    mov     a0l, [r0++]
    mov     a0l, r3
    FADE8
    mov     a0l, [r1++]
    add     1 << (4 + \o), a1
1:
.endm

// r1 = &grad[2 * (perm[a0 & 0xFF] & 15)], with r7 = perm, b1 = grad
.macro GRAD_PTR
    and     0xff, a0
    add     r7, a0
    mov     a0l, r1
    mov     [r1], a0
    and     0xf, a0
    shfi    a0, a0, 1
    add     b1, a0
    mov     a0l, r1
.endm

// One corner of the cell: gx and (mul8(gy, fy) - sub_gx * gx - sub_gy * gy)
// at pn_cell + 2 * k. a0 = perm[ix] + iy (+ 1). fy is in r5.
.macro CORNER8 k, sub_gx, sub_gy
    GRAD_PTR
    mov     [r1++], a1              // gx
    mov     a1l, [pn_cell + 2 * \k]
    mov     [r1], y0                // gy
    mpy     y0, r5, a0
    P_SHR8
.if \sub_gx
    sub     a1, a0
.endif
.if \sub_gy
    sub     y0, a0
.endif
    mov     a0l, [pn_cell + 2 * \k + 1]
.endm

// Gradients and constants of the cell ix = r6 - perm (r6 isn't modified)
.macro CELL8
    mov     grad, r1
    mov     r1, b1
    mov     [r6++], a0
    add     [pn_iy], a0
    CORNER8 0, 0, 0                 // (ix, iy)
    mov     [r6--], a0
    add     [pn_iy], a0
    CORNER8 1, 1, 0                 // (ix + 1, iy)
    mov     [r6++], a0
    add     [pn_iy], a0
    add     1, a0
    CORNER8 2, 0, 1                 // (ix, iy + 1)
    mov     [r6--], a0
    add     [pn_iy], a0
    add     1, a0
    CORNER8 3, 1, 1                 // (ix + 1, iy + 1)
.endm

// Gradient dot product of the next corner at r1: mul8(gx, fx) + constant
.macro DOT8
    mov     [r1++], y0
    mpy     y0, [r0], a0
    P_SHR8
    add     [r1++], a0
.endm

// a0 = a + mul8(a0 - a, w), with a in \a and the weight given by the
// operand of "mpy"
.macro LERP8 a, w
    sub     \a, a0
    mov     a0l, y0
    mpy     y0, \w, a0
    P_SHR8
    add     \a, a0
.endm

// One pixel of an octave: r0 = fx, r2 = fade(fx), r4 = fade(fy), r3 = sum
.macro PIXEL8
    mov     pn_cell, r1
    DOT8                            // n00
    mov     a0, b0
    DOT8                            // n10
    LERP8   b0, [r2]
    copy    a1, always              // a1 = lerp(n00, n10, u)
    DOT8                            // n01
    mov     a0, b0
    DOT8                            // n11
    LERP8   b0, [r2++]              // lerp(n01, n11, u)
    LERP8   a1, r4
    modr    [r0++]
    shfc    a0, a0, always          // >> octave (sv = -o)
    add     [r3], a0
    mov     a0l, [r3++]
.endm

// Octave o of the row pn_py
.macro OCTAVE8 o
    mov     -\o, a0
    mov     a0l, sv

    mov     [pn_py], a0
    shfi    a0, a0, -(4 - \o)
    mov     a0l, [pn_iy]
    mov     [pn_py], a0
    mov     pn_fx + 64 * \o, r1
    add     r1, a0
    mov     a0l, r1
    mov     [r1], r5                // fy
    addv    256, r1
    mov     [r1], r4                // fade(fy)

    mov     pn_fx + 64 * \o, r0
    mov     pn_fade + 64 * \o, r2
    mov     pn_sum, r3
    mov     perm, r6
    bkrep   (4 << \o) - 1, 2f       // Cells
    CELL8
    bkrep   (16 >> \o) - 1, 3f      // Pixels of the cell
    PIXEL8
3:
    modr    [r6++]
2:
.endm

.macro PUSH_ALL
    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6
    push    r7
.endm

.macro POP_ALL
    pop     r7
    pop     r6
    pop     r5
    pop     r4
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
.endm

// Whole image with "octaves" octaves (1 or 4), dst in a0l
.macro NOISE8 octaves
    PUSH_ALL
    mov     a0l, [pn_dst]

    mov     pn_fx, r0
    mov     pn_fade, r1
    OCTAVE_TABLES8 0
    OCTAVE_TABLES8 1
    OCTAVE_TABLES8 2
    OCTAVE_TABLES8 3

    mov     perm, r7
    clr     a0, always
    mov     a0l, [pn_py]

    bkrep   63, 1f                  // Rows
    mov     pn_sum, r3
    clr     a0, always
    rep     63
    mov     a0l, [r3++]

    OCTAVE8 0
.if \octaves > 1
    OCTAVE8 1
    OCTAVE8 2
    OCTAVE8 3
.endif

    // to_gray8(): clamp(2048 + (sum << 3))
    mov     0xfff, b0
    mov     pn_sum, r3
    mov     [pn_dst], a0
    mov     a0l, r5
    bkrep   63, 2f
    mov     [r3++], a0
    shfi    a0, a0, 3
    add     0x800, a0
    clr     a0, lt
    sub     b0, a0
    clr     a0, gt
    add     b0, a0
    mov     a0l, [r5++]
2:
    mov     r5, a0
    mov     a0l, [pn_dst]
    mov     [pn_py], a0
    add     1, a0
    mov     a0l, [pn_py]
1:

    POP_ALL
    ret     always
.endm

// void k_noise_asm(uint16_t *dst): same as k_noise(dst, 1)
BEGIN_ASM_FUNC k_noise_asm
    NOISE8  1

// void k_fbm_asm(uint16_t *dst): same as k_noise(dst, 4)
BEGIN_ASM_FUNC k_fbm_asm
    NOISE8  4

// Q16.16 version
// --------------
//
// For octave o, x = px << (12 + o), so fx16 = fx << 8 with the fx of the Q8
// version (pn_fx), and:
//
//   q16_mul(g << 8, fx16) = (g * fx * 65536) >> 16 = g * fx
//   q16_mul(g << 8, fx16 - 65536) = g * fx - 256 * g
//
// The gradient dot products are exact 32-bit values: gx * fx + constant of
// the corner (3 words per corner in pn_cell16). Only fade16() (in a table,
// pn_fade16, as fx16 only has 256 values) and the interpolations need
// q16_mul(). fade16() is always below 65536: it is an unsigned 16-bit value.

    .section .bss.perlin_q16_asm, "aw", %nobits
    .align 2
pn_fade16:  .space 2 * 256      // fade16(pn_fx << 8)
pn_cell16:  .space 2 * 12       // gx and 32-bit constant of the 4 corners
pn_sum16:   .space 4 * 64       // Sum of the octaves of a row (32-bit)
pn_f:       .space 4

    .text

// a0 = q16_mul(a0, a1) (32-bit values, sign extended in the accumulators).
// Uses b0, b1, y0. Same chain as k_q16mul_asm (08-math32).
pn_q16m:
    mov     a0, b0
    mov     a1, b1
    mov     b0h, y0                 // ah
    mpy     y0, b1h, a0             // p = ah * bh
    mov     b0l, y0                 // al
    clr     a0, always
    macuu   y0, b1l, a0             // a0 = ah * bh, p = al * bl
    shfi    a0, a0, 16
    mov     b1h, y0                 // bh
    maasu   y0, b0l, a0             // a0 += (al * bl) >> 16, p = bh * al
    mov     b1l, y0                 // bl
    macus   y0, b0h, a0             // a0 += bh * al, p = bl * ah
    add     p*, a0
    shfi    a0, a0, 8               // Sign extension of bit 31
    shfi    a0, a0, -8
    ret     always

// fade16(t) in a0l, with t in r3 (0 to 65535). Uses a1, b0, b1, y0, r1.
//
//   f = q16_mul(t, 6t - 15 * 65536) + 10 * 65536
//   fade = q16_mul(q16_mul(q16_mul(t, t), t), f)
.macro FADE16
    mov     r3, a0l                 // t (zero extended)
    shfi    a0, a1, 1
    shfi    a0, a0, 2
    add     a0, a1                  // 6t
    mov     15, b0
    shfi    b0, b0, 16
    sub     b0, a1
    mov     r3, a0l
    call    pn_q16m, always
    mov     10, b0
    shfi    b0, b0, 16
    add     b0, a0                  // f
    mov     pn_f, r1
    mov     a0h, [r1++]
    mov     a0l, [r1]
    mov     r3, a0l
    mov     r3, a1l
    call    pn_q16m, always         // t2
    mov     r3, a1l
    call    pn_q16m, always         // t3
    mov     pn_f, r1
    mov     [r1++], a1h
    or      [r1], a1
    call    pn_q16m, always
.endm

// One corner of the cell: gx and gy * fy - sub_gx * 256 * gx
// - sub_gy * 256 * gy at pn_cell16 + 3 * k. a0 = perm[ix] + iy (+ 1), fy is
// in r5.
.macro CORNER16 k, sub_gx, sub_gy
    GRAD_PTR
    mov     [r1++], a1              // gx
    mov     a1l, [pn_cell16 + 3 * \k]
    mov     [r1], y0                // gy
    mpy     y0, r5, a0
    clr     a0, always
    add     p*, a0
.if \sub_gx
    shfi    a1, a1, 8
    sub     a1, a0
.endif
.if \sub_gy
    mov     y0, a1
    shfi    a1, a1, 8
    sub     a1, a0
.endif
    mov     a0l, [pn_cell16 + 3 * \k + 2]
    shfi    a0, a0, -16
    mov     a0l, [pn_cell16 + 3 * \k + 1]
.endm

.macro CELL16
    mov     grad, r1
    mov     r1, b1
    mov     [r6++], a0
    add     [pn_iy], a0
    CORNER16 0, 0, 0
    mov     [r6--], a0
    add     [pn_iy], a0
    CORNER16 1, 1, 0
    mov     [r6++], a0
    add     [pn_iy], a0
    add     1, a0
    CORNER16 2, 0, 1
    mov     [r6--], a0
    add     [pn_iy], a0
    add     1, a0
    CORNER16 3, 1, 1
.endm

// Gradient dot product of the next corner at r1: gx * fx + constant
.macro DOT16
    mov     [r1++], y0
    mpy     y0, [r0], a0
    clr     a0, always
    add     p*, a0
    addh    [r1++], a0
    addl    [r1++], a0
.endm

// a0 = a + q16_mul(a0 - a, w), with a in b0 or b1 and w (unsigned 16-bit)
// given by 3 operands of the multiplications (the last one may advance a
// pointer). With d = dh * 65536 + dl:
//
//   q16_mul(d, w) = dh * w + ((dl * w) >> 16)
.macro LERP16 a, w1, w2, w3
    sub     \a, a0                  // d
    clr     a1, always
    mov     a0h, y0
    mpysu   y0, \w1, a1             // p = dh * w
    mov     a0l, y0
    macuu   y0, \w2, a1             // a1 = dh * w, p = dl * w
    mov     0, y0
    maa     y0, \w3, a1             // a1 += (dl * w) >> 16
    add     \a, a1
    copy    a0, always              // a0 = a1
.endm

// One pixel of an octave: r0 = fx, r2 = fade16(fx), r4 = fade16(fy),
// r3 = sum (32-bit)
.macro PIXEL16
    mov     pn_cell16, r1
    DOT16                           // n00
    mov     a0, b0
    DOT16                           // n10
    LERP16  b0, [r2], [r2], [r2]
    mov     a0, b1                  // lerp(n00, n10, u)
    DOT16                           // n01
    mov     a0, b0
    DOT16                           // n11
    LERP16  b0, [r2], [r2], [r2++]  // lerp(n01, n11, u)
    LERP16  b1, r4, r4, r4
    modr    [r0++]
    shfc    a0, a0, always          // >> octave (sv = -o)
    addh    [r3++], a0
    addl    [r3--], a0
    mov     a0h, [r3++]
    mov     a0l, [r3++]
.endm

.macro OCTAVE16 o
    mov     -\o, a0
    mov     a0l, sv

    mov     [pn_py], a0
    shfi    a0, a0, -(4 - \o)
    mov     a0l, [pn_iy]
    mov     [pn_py], a0
    mov     pn_fx + 64 * \o, r1
    add     r1, a0
    mov     a0l, r1
    mov     [r1], r5                // fy
    mov     [pn_py], a0
    mov     pn_fade16 + 64 * \o, r1
    add     r1, a0
    mov     a0l, r1
    mov     [r1], r4                // fade16(fy)

    mov     pn_fx + 64 * \o, r0
    mov     pn_fade16 + 64 * \o, r2
    mov     pn_sum16, r3
    mov     perm, r6
    bkrep   (4 << \o) - 1, 2f       // Cells
    CELL16
    bkrep   (16 >> \o) - 1, 3f      // Pixels of the cell
    PIXEL16
3:
    modr    [r6++]
2:
.endm

// Whole image with "octaves" octaves (1 or 4), dst in a0l
.macro NOISE16 octaves
    PUSH_ALL
    mov     a0l, [pn_dst]

    // pn_fx (as in the Q8 version) and pn_fade16
    mov     pn_fx, r0
    mov     pn_fade, r1
    OCTAVE_TABLES8 0
    OCTAVE_TABLES8 1
    OCTAVE_TABLES8 2
    OCTAVE_TABLES8 3
    mov     pn_fx, r0
    mov     pn_fade16, r2
    bkrep   255, 1f
    mov     [r0++], a0
    shfi    a0, a0, 8
    mov     a0l, r3                 // t = fx << 8
    FADE16
    mov     a0l, [r2++]
1:

    mov     perm, r7
    clr     a0, always
    mov     a0l, [pn_py]

    bkrep   63, 1f                  // Rows
    mov     pn_sum16, r3
    clr     a0, always
    rep     127
    mov     a0l, [r3++]

    OCTAVE16 0
.if \octaves > 1
    OCTAVE16 1
    OCTAVE16 2
    OCTAVE16 3
.endif

    // to_gray16(): clamp(2048 + (sum >> 5))
    mov     0xfff, b0
    mov     pn_sum16, r3
    mov     [pn_dst], a0
    mov     a0l, r5
    bkrep   63, 2f
    mov     [r3++], a0h
    or      [r3++], a0
    shfi    a0, a0, -5
    add     0x800, a0
    clr     a0, lt
    sub     b0, a0
    clr     a0, gt
    add     b0, a0
    mov     a0l, [r5++]
2:
    mov     r5, a0
    mov     a0l, [pn_dst]
    mov     [pn_py], a0
    add     1, a0
    mov     a0l, [pn_py]
1:

    POP_ALL
    ret     always
.endm

// void k_noise_q16_asm(uint16_t *dst): same as k_noise_q16(dst, 1)
BEGIN_ASM_FUNC k_noise_q16_asm
    NOISE16 1

// void k_fbm_q16_asm(uint16_t *dst): same as k_noise_q16(dst, 4)
BEGIN_ASM_FUNC k_fbm_q16_asm
    NOISE16 4
