// SPDX-License-Identifier: CC0-1.0
//
// Hand-written DSP skinning kernel (same results as k_skin()).
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved. The forms of "mpy"/"mac" with
// two memory operands aren't used (the second operand is read through the Y
// bus, which doesn't see the same memory on DSi hardware).

#include <teak/asminc.h>

    .section .bss.k_skin_asm, "aw", %nobits
    .align 2
skin_bones: .space 2        // Address of bones[]
skin_m0:    .space 2        // Address of the matrix of bone 0 of the vertex
skin_m1:    .space 2        // Address of the matrix of bone 1 of the vertex
skin_w0:    .space 2        // Weights of the vertex
skin_w1:    .space 2
skin_t:     .space 12       // Transformed rows: t0.x t1.x t0.y t1.y t0.z t1.z

// Row "r0" of a bone matrix applied to the vertex at r3, in Q3.12. The result
// is stored at [r4] and r4 advances by 2 (results of both bones interleaved).
.macro ROW
    mov     r3, r1
    clr     a0, always
    mov     [r1++], y0
    mpy     y0, [r0++], a0          // p0 = m[0] * x
    mov     [r1++], y0
    mac     y0, [r0++], a0          // a0 += p0, p0 = m[1] * y
    mov     [r1++], y0
    mac     y0, [r0++], a0          // a0 += p0, p0 = m[2] * z
    add     p*, a0
    mov     [r0++], b0              // Translation
    shfi    b0, b0, 12
    add     b0, a0
    shfi    a0, a0, -12
    mov     a0l, [r4++]
    modr    [r4++]
.endm

// Blends the results of both bones for one coordinate: [r4] and [r4 + 1]
.macro BLEND
    mov     [skin_w0], a1
    mov     a1l, y0
    clr     a0, always
    mpy     y0, [r4++], a0          // p0 = w0 * t0
    mov     [skin_w1], a1
    mov     a1l, y0
    mac     y0, [r4++], a0          // a0 = p0, p0 = w1 * t1
    add     p*, a0
    shfi    a0, a0, -12
    mov     a0l, [r2++]
.endm

// Address of the matrix of bone a0 (0 to 15) in a0
.macro BONE_ADDRESS
    shfi    a0, a1, 3               // a1 = index * 8
    shfi    a0, a0, 2               // a0 = index * 4
    add     a1, a0                  // a0 = index * 12
    add     [skin_bones], a0
.endm

// void k_skin_asm(const kvert *v, const kbone *b, kvec3 *out, uint16_t n)
//
// Requires n >= 1.

BEGIN_ASM_FUNC k_skin_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4

    mov     a0l, r3                 // Current vertex
    mov     a1l, [skin_bones]
    mov     b0l, r2                 // Output
    addv    0xffff, b1l             // "bkrep" runs its block count + 1 times

    bkrep   b1l, 9f

    // Bone indices and weights: the vertex is x, y, z, bones, w0
    mov     r3, r1
    modr    r1, +2
    modr    [r1++]
    mov     [r1++], b1              // b1 = bones (b0 | b1 << 8)
    mov     [r1], a1                // a1 = w0
    mov     a1l, [skin_w0]
    xor     0xffff, a1              // w1 = 4096 - w0 (low 16 bits)
    add     0x1001, a1
    mov     a1l, [skin_w1]

    mov     b1, a0
    and     0xff, a0
    BONE_ADDRESS
    mov     a0l, [skin_m0]

    mov     b1, a0
    shfi    a0, a0, -8
    BONE_ADDRESS
    mov     a0l, [skin_m1]

    // Rows of bone 0 at t[0, 2, 4], rows of bone 1 at t[1, 3, 5]
    mov     skin_t, r4
    mov     [skin_m0], a0
    mov     a0l, r0
    ROW
    ROW
    ROW
    mov     skin_t+1, r4
    mov     [skin_m1], a0
    mov     a0l, r0
    ROW
    ROW
    ROW

    mov     skin_t, r4
    BLEND
    BLEND
    BLEND

    addv    5, r3                   // Next vertex
9:

    pop     r4
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always

// Faster Q3.12 version
// --------------------
//
// The vertex stays in registers (x, y, z in r4-r6, w0 and w1 in b0l and b1l),
// the matrix values go through y0, and both transformed rows of a coordinate
// are blended as soon as they are ready, without storing them.
//
// The translation is added after the shift: (sum + (t << 12)) >> 12 is
// sum >> 12 + t, because the low 12 bits of t << 12 are 0.

// Row of the matrix at "ptr" applied to the vertex, result (low 16 bits) in
// "acc". "ptr" advances to the next row.
.macro ROW2 ptr, acc
    clr     \acc, always
    mov     [\ptr++], y0
    mpy     y0, r4, \acc            // p = m[0] * x
    mov     [\ptr++], y0
    mac     y0, r5, \acc            // p = m[1] * y
    mov     [\ptr++], y0
    mac     y0, r6, \acc            // p = m[2] * z
    add     p*, \acc
    shfi    \acc, \acc, -12
    add     [\ptr++], \acc          // Translation
.endm

// Blends a0l (bone 0) and a1l (bone 1), stores the result at [r2++]
.macro BLEND2
    mov     b0l, y0
    mpy     y0, a0l, a0             // p = w0 * t0 (a0 isn't modified)
    mov     b1l, y0
    clr     a0, always
    mac     y0, a1l, a0             // a0 = w0 * t0, p = w1 * t1
    add     p*, a0
    shfi    a0, a0, -12
    mov     a0l, [r2++]
.endm

// Address of the matrix of bone a0 (0 to 15) in a0, with r7 = bones[]
.macro BONE_ADDRESS2
    shfi    a0, a1, 3               // a1 = index * 8
    shfi    a0, a0, 2               // a0 = index * 4
    add     a1, a0
    add     r7, a0                  // a0 = bones + index * 12
.endm

// void k_skin2_asm(const kvert *v, const kbone *b, kvec3 *out, uint16_t n)
//
// Requires n >= 1.

BEGIN_ASM_FUNC k_skin2_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6
    push    r7

    mov     a0l, r3                 // Current vertex
    mov     a1l, r7                 // bones[]
    mov     b0l, r2                 // Output
    addv    0xffff, b1l

    bkrep   b1l, 1f
    mov     [r3++], r4              // x
    mov     [r3++], r5              // y
    mov     [r3++], r6              // z
    mov     [r3++], a0              // Bone indices
    mov     [r3++], b0              // w0
    mov     0x1000, b1
    sub     b0, b1                  // w1 = 4096 - w0

    mov     a0l, y0                 // Keep the bone indices
    and     0xff, a0
    BONE_ADDRESS2
    mov     a0l, r0                 // Matrix of bone 0
    mov     y0, a0
    shfi    a0, a0, -8
    and     0xff, a0
    BONE_ADDRESS2
    mov     a0l, r1                 // Matrix of bone 1

    ROW2    r0, a0
    ROW2    r1, a1
    BLEND2
    ROW2    r0, a0
    ROW2    r1, a1
    BLEND2
    ROW2    r0, a0
    ROW2    r1, a1
    BLEND2
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

// Q16.16 version
// --------------
//
// q16_mul(a, b) = ((ah * bh) << 16) + ((al * bl) >> 16) + ah * bl + al * bh,
// with ah, bh signed and al, bl unsigned. The 4 products of each q16_mul()
// are chained: each multiplication instruction adds the previous product to
// an accumulator while it starts the next one.
//
// - a1: sum of the ah * bh products, shifted left by 16 at the end
// - a0: the other products. "maa" adds the previous product shifted right by
//   16 (al * bl) while it starts the ah * bh product of the next q16_mul().

    .section .bss.k_skin_q16_asm, "aw", %nobits
    .align 2
skin_q16_w: .space 8        // w0 and w1 of the vertex (high half first)

// One q16_mul() term: a (at [ptr], x operand) * b (bh and bl given by the
// y0 loads). The first term of a sum uses "mpy" instead of "maa".
.macro Q16_TERM ptr, first, load_bh, load_bl
    \load_bh
.if \first
    mpy     y0, [\ptr], a0          // p = bh * ah
.else
    maa     y0, [\ptr], a0          // a0 += previous (al * bl) >> 16
.endif
    \load_bl
    macus   y0, [\ptr++], a1        // a1 += ah * bh, p = bl * ah
    \load_bh
    macsu   y0, [\ptr], a0          // a0 += bl * ah, p = bh * al
    \load_bl
    macuu   y0, [\ptr++], a0        // a0 += bh * al, p = bl * al
.endm

// Adds the last (al * bl) >> 16 (the product with y0 = 0 is 0), then
// a0 += a1 << 16.
.macro Q16_END
    mov     0, y0
    maa     y0, r3, a0
    shfi    a1, a1, 16
    add     a1, a0
.endm

// Row of the matrix at "ptr" (which advances to the next row) applied to the
// vertex at r5: result in a0.
.macro ROW_Q16 ptr
    mov     r5, r3
    clr     a0, a1
    Q16_TERM \ptr, 1, "mov [r3++], y0", "mov [r3--], y0"
    modr    [r3++]
    modr    [r3++]
    Q16_TERM \ptr, 0, "mov [r3++], y0", "mov [r3--], y0"
    modr    [r3++]
    modr    [r3++]
    Q16_TERM \ptr, 0, "mov [r3++], y0", "mov [r3--], y0"
    Q16_END
    addh    [\ptr++], a0            // Translation
    addl    [\ptr++], a0
.endm

// Blends b0 (bone 0) and b1 (bone 1) with the weights at skin_q16_w, stores
// the result at [r2++].
.macro BLEND_Q16
    mov     skin_q16_w, r6
    clr     a0, a1
    Q16_TERM r6, 1, "mov b0h, y0", "mov b0l, y0"
    Q16_TERM r6, 0, "mov b1h, y0", "mov b1l, y0"
    Q16_END
    mov     a0h, [r2++]
    mov     a0l, [r2++]
.endm

// Address of the matrix of bone a0 (0 to 15) in a0, with r4 = bones[]
.macro BONE_ADDRESS_Q16
    shfi    a0, a1, 4               // a1 = index * 16
    shfi    a0, a0, 3               // a0 = index * 8
    add     a1, a0
    add     r4, a0                  // a0 = bones + index * 24
.endm

// void k_skin_q16_asm(const kvert_q16 *v, const kbone_q16 *b,
//                     kvec3_q16 *out, uint16_t n)
//
// Requires n >= 1. Vertex: x, y, z, w0 (32-bit), bones (16-bit), padding.

BEGIN_ASM_FUNC k_skin_q16_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6

    mov     a0l, r5                 // Current vertex
    mov     a1l, r4                 // bones[]
    mov     b0l, r2                 // Output
    addv    0xffff, b1l

    bkrep   b1l, 1f

    // Weights: w0 and w1 = 65536 - w0
    mov     r5, r3
    addv    6, r3
    mov     [r3++], a0h
    or      [r3++], a0              // a0 = w0
    mov     [r3], a1                // a1 = bone indices
    mov     skin_q16_w, r6
    mov     a0h, [r6++]
    mov     a0l, [r6++]
    neg     a0, always
    mov     1, b0
    shfi    b0, b0, 16
    add     b0, a0
    mov     a0h, [r6++]
    mov     a0l, [r6++]

    mov     a1l, y0                 // Keep the bone indices
    mov     a1, a0
    and     0xff, a0
    BONE_ADDRESS_Q16
    mov     a0l, r0                 // Matrix of bone 0
    mov     y0, a0
    shfi    a0, a0, -8
    BONE_ADDRESS_Q16
    mov     a0l, r1                 // Matrix of bone 1

    ROW_Q16 r0
    mov     a0, b0
    ROW_Q16 r1
    mov     a0, b1
    BLEND_Q16
    ROW_Q16 r0
    mov     a0, b0
    ROW_Q16 r1
    mov     a0, b1
    BLEND_Q16
    ROW_Q16 r0
    mov     a0, b0
    ROW_Q16 r1
    mov     a0, b1
    BLEND_Q16

    addv    10, r5                  // Next vertex
1:

    pop     r6
    pop     r5
    pop     r4
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always
