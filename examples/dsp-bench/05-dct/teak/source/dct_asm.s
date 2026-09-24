// SPDX-License-Identifier: CC0-1.0
//
// Hand-written 8-point transform (same results as dct8()).
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved. The forms of "mpy"/"mac" with
// two memory operands aren't used (the second operand is read through the Y
// bus, which doesn't see the same memory on DSi hardware).

#include <teak/asminc.h>

.macro MAC1
    mov     [r1++], y0
    mac     y0, [r0++], a0
.endm

// void dct8_asm(const int16_t *mat, const int16_t *vec, int16_t *out,
//               uint16_t shift)
//
// out[8 * u] = (sum_x mat[8 * u + x] * vec[x] + (1 << (shift - 1))) >> shift

BEGIN_ASM_FUNC dct8_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3

    mov     a0l, r0                 // Matrix, read row after row
    mov     a1l, r3                 // Vector
    mov     b0l, r2                 // Output

    mov     b1l, a0                 // b1 = 1 << (shift - 1) (rounding)
    sub     0x1, a0
    mov     a0l, sv
    mov     0x1, a1
    shfc    a1, a1, always
    mov     b1l, a0                 // sv = -shift
    xor     0xffff, a0
    add     0x1, a0
    mov     a0l, sv
    mov     a1, b1

    bkrep   7, 9f
    mov     r3, r1
    clr     a0, always
    mov     [r1++], y0
    mpy     y0, [r0++], a0          // p0 = mat[0] * vec[0]
    MAC1
    MAC1
    MAC1
    MAC1
    MAC1
    MAC1
    MAC1
    add     p*, a0
    add     b1, a0                  // Rounding
    shfc    a0, a0, always          // >> shift
    mov     a0l, [r2]
    addv    8, r2
9:

    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always

// Whole-image transforms
// ----------------------
//
// Same results as k_dct_image() and k_idct_image(), without any C code per
// block or per row.
//
// Each 8-point transform computes 2 outputs (u0 and u0 + 1) at a time: each
// input value is loaded once into y0 and multiplied by both matrix rows. A
// "mac" adds the previous product to its accumulator, so the accumulators
// alternate: a0 for row u0 (read with r0), a1 for row u0 + 1 (read with r4).
//
// The shifts are DCT_SHIFT_1 (12) and DCT_SHIFT_2 (16) of dct.h.
//
// Each output starts from a 32-bit constant of the pass (one per row of the
// matrix, in dct_offsets_1 and dct_offsets_2):
//
// - DCT pass 1: the level shift of the input (v - 2048) is moved into the
//   constant: sum(m * (v - 2048)) = sum(m * v) - 2048 * sum(m). The constant
//   is (1 << 11) - 2048 * (sum of the row of the matrix).
// - IDCT pass 2: 2048 is added before the clamp: (1 << 15) + (2048 << 16).
//   It doesn't change the result, as 2048 << 16 has no bits below bit 16.
// - Other passes: rounding only.

    .section .bss.dct_image_asm, "aw", %nobits
    .align 2
dct_tmp:        .space 128      // 8x8 block after the first pass
dct_offsets_1:  .space 32       // 8 constants of 32 bits for each pass
dct_offsets_2:  .space 32

// Two outputs of an 8-point transform. The input vector is at r3, the rows of
// the matrix at r0 and r4, the constants at r5. The results are stored at
// [r2] and [r2 + stride]; r2 advances by 2 * stride.
.macro PAIR shift, stride, clamp
    mov     r3, r1
    mov     [r5++], a0h
    or      [r5++], a0
    mov     [r5++], a1h
    or      [r5++], a1
    mov     [r1++], y0
    mpy     y0, [r0++], a0          // p = m[u0][0] * v[0]
    mac     y0, [r4++], a0          // p = m[u1][0] * v[0]
.rept 7
    mov     [r1++], y0
    mac     y0, [r0++], a1          // p = m[u0][x] * v[x]
    mac     y0, [r4++], a0          // p = m[u1][x] * v[x]
.endr
    add     p*, a1
    shfi    a0, a0, -\shift
.if \clamp
    CLAMP   a0
.endif
    shfi    a1, a1, -\shift
.if \clamp
    CLAMP   a1
.endif
    mov     a0l, [r2]
    addv    \stride, r2
    mov     a1l, [r2]
    addv    \stride, r2
    mov     r4, r0                  // Next 2 rows of the matrix
    addv    8, r4
.endm

// Clamps to [0, b0] (b0 = 4095). The flags come from the shift before it.
.macro CLAMP acc
    clr     \acc, lt
    sub     b0, \acc
    clr     \acc, gt
    add     b0, \acc
.endm

// 8 transforms: rows of the input at r3 (advancing by "in_stride"), outputs at
// r2, transposed (the outputs of a row have a stride of "out_stride").
.macro PASS matrix, offsets, shift, in_stride, out_stride, clamp
    bkrep   7, 4f
    mov     \matrix, r0
    mov     \matrix + 8, r4
    mov     \offsets, r5
    PAIR    \shift, \out_stride, \clamp
    PAIR    \shift, \out_stride, \clamp
    PAIR    \shift, \out_stride, \clamp
    PAIR    \shift, \out_stride, \clamp
    addv    1 - 8 * \out_stride, r2 // Next column of the output
    addv    \in_stride, r3
4:
.endm

// Sets the 8 constants at r5 to a0
.macro SET_OFFSETS
    bkrep   7, 5f
    mov     a0h, [r5++]
    mov     a0l, [r5++]
5:
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

// Transforms the 64 blocks: r6 = first input block, r7 = first output block.
// A block of the image is 8 rows of 64 pixels (next block: + 8, next row of
// blocks: + 7 * 64 more), a block of coefficients is 64 consecutive values.
.macro IMAGE matrix, in_block_stride, in_row_stride, in_extra, out_block_stride, out_row_stride, out_extra, clamp
    bkrep   7, 2f                   // Rows of blocks
    bkrep   7, 3f                   // Blocks

    mov     r6, r3
    mov     dct_tmp, r2
    PASS    \matrix, dct_offsets_1, 12, \in_row_stride, 8, 0

    mov     dct_tmp, r3
    mov     r7, r2
    PASS    \matrix, dct_offsets_2, 16, 8, \out_row_stride, \clamp

    addv    \in_block_stride, r6
    addv    \out_block_stride, r7
3:
    addv    \in_extra, r6
    addv    \out_extra, r7
2:
.endm

// void k_dct_image_asm(const uint16_t *src, int16_t *coef)

BEGIN_ASM_FUNC k_dct_image_asm

    PUSH_ALL

    mov     a0l, r6
    mov     a1l, r7

    // Constants of the first pass
    mov     dct_matrix, r0
    mov     dct_offsets_1, r5
    bkrep   7, 1f
    clr     a0, always
.rept 8
    add     [r0++], a0              // Sum of the row
.endr
    shfi    a0, a0, 11
    neg     a0, always
    add     0x800, a0
    mov     a0h, [r5++]
    mov     a0l, [r5++]
1:
    mov     0x4000, a0              // Constants of the second pass
    add     0x4000, a0
    mov     dct_offsets_2, r5
    SET_OFFSETS

    // src: blocks of 8 pixels, rows of 64 (next row of blocks: + 7 * 64)
    // coef: blocks of 64 values
    IMAGE   dct_matrix, 8, 64, 7 * 64, 64, 8, 0, 0

    POP_ALL
    ret     always

// void k_idct_image_asm(const int16_t *coef, uint16_t *recon)

BEGIN_ASM_FUNC k_idct_image_asm

    PUSH_ALL

    mov     a0l, r6
    mov     a1l, r7

    mov     0x800, a0
    mov     dct_offsets_1, r5
    SET_OFFSETS
    mov     0x800, a0               // (1 << 15) + (2048 << 16)
    shfi    a0, a0, 16
    add     0x4000, a0
    add     0x4000, a0
    mov     dct_offsets_2, r5
    SET_OFFSETS

    mov     0xfff, b0               // Clamp
    IMAGE   idct_matrix, 64, 8, 0, 8, 64, 7 * 64, 1

    POP_ALL
    ret     always
