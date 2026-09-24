// SPDX-License-Identifier: CC0-1.0
//
// Hand-written DSP kernels of the collision example.
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved. A 32-bit value is stored in
// memory with the high half at the lower address.

#include <teak/asminc.h>

// void k_dot3_asm(const kvec3 *a, const kvec3 *b, int32_t *out, uint16_t n)
//
// Requires n >= 1. The forms of "mpy"/"mac" with two memory operands aren't
// used (their second operand is read through the Y bus, which doesn't see the
// same memory on DSi hardware): b[] is loaded into y0 with a normal read.

BEGIN_ASM_FUNC k_dot3_asm

    push    y0
    push    r0
    push    r1
    push    r2

    mov     a0l, r0                 // a[]
    mov     a1l, r1                 // b[]
    mov     b0l, r2                 // out[]
    addv    0xffff, b1l             // "bkrep" runs its block count + 1 times

    bkrep   b1l, 1f
    clr     a0, always
    mov     [r1++], y0
    mpy     y0, [r0++], a0          // p0 = a.x * b.x
    mov     [r1++], y0
    mac     y0, [r0++], a0          // a0 += p0, p0 = a.y * b.y
    mov     [r1++], y0
    mac     y0, [r0++], a0          // a0 += p0, p0 = a.z * b.z
    add     p*, a0
    mov     a0h, [r2++]             // High half first
    mov     a0l, [r2++]
1:

    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always

// void k_cross3_asm(const kvec3 *a, const kvec3 *b, int32_t *out, uint16_t n)
//
// Requires n >= 1. b is loaded into r3-r5, and each component of a into y0,
// which is multiplied by two components of b:
//
//   out[0] (a0) = ay * bz - az * by
//   out[1] (b0) = az * bx - ax * bz
//   out[2] (a1) = ax * by - ay * bx

BEGIN_ASM_FUNC k_cross3_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    addv    0xffff, b1l

    bkrep   b1l, 1f
    mov     [r1++], r3              // bx
    mov     [r1++], r4              // by
    mov     [r1++], r5              // bz
    clr     a0, a1
    clr     b0, always
    mov     [r0++], y0              // ax
    mpy     y0, r5, a0              // p = ax * bz
    sub     p0, b0
    mpy     y0, r4, a0              // p = ax * by
    mov     [r0++], y0              // ay
    mac     y0, r5, a1              // a1 += ax * by, p = ay * bz
    mac     y0, r3, a0              // a0 += ay * bz, p = ay * bx
    sub     p*, a1
    mov     [r0++], y0              // az
    mpy     y0, r4, a0              // p = az * by
    sub     p*, a0
    mpy     y0, r3, a0              // p = az * bx
    add     p0, b0
    mov     a0h, [r2++]
    mov     a0l, [r2++]
    mov     b0h, [r2++]
    mov     b0l, [r2++]
    mov     a1h, [r2++]
    mov     a1l, [r2++]
1:

    pop     r5
    pop     r4
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always

// Bitmask outputs
// ---------------
//
// The test result of each pair goes into the carry, then "ror b1" moves it
// into bit 39 of b1. After 16 pairs, the first result is in bit 24 and the
// last one in bit 39: shifting b1 right by 8 puts the word in b1h. That's why
// n must be a multiple of 16 (like in the C versions, which only write full
// words).
//
// A test "x <= 0" becomes a carry with "dec" (x - 1 < 0) and "rol" (the sign
// bit, bit 39, goes into the carry).

// void k_sphere_asm(const ksphere *a, const ksphere *b, uint16_t *mask,
//                   uint16_t n)
//
// Bit i = 1 if |ca - cb|^2 <= (ra + rb)^2. Requires n = 16 * k, k >= 1.

BEGIN_ASM_FUNC k_sphere_asm

    push    y0
    push    r0
    push    r1
    push    r2

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    shfi    b1, b1, -4
    addv    0xffff, b1l

    bkrep   b1l, 1f

    bkrep   15, 2f
    clr     a0, always
    mov     [r0++], a1
    sub     [r1++], a1              // dx
    mov     a1l, y0
    mpy     y0, a1l, a0             // p = dx * dx
    mov     [r0++], a1
    sub     [r1++], a1              // dy
    mov     a1l, y0
    mac     y0, a1l, a0             // p = dy * dy
    mov     [r0++], a1
    sub     [r1++], a1              // dz
    mov     a1l, y0
    mac     y0, a1l, a0             // p = dz * dz
    mov     [r0++], a1
    add     [r1++], a1              // ra + rb
    mov     a1l, y0
    mac     y0, a1l, a0             // p = (ra + rb)^2
    sub     p*, a0                  // a0 = d2 - r2
    dec     a0, always
    rol     a0, always              // carry = (d2 <= r2)
    ror     b1, always
2:
    shfi    b1, b1, -8
    mov     b1h, [r2++]
1:

    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always

// void k_aabb_asm(const ksphere *a, const ksphere *b, uint16_t *mask,
//                 uint16_t n)
//
// Bit i = 1 if max(|dx|, |dy|, |dz|) <= ra + rb. Requires n = 16 * k, k >= 1.

BEGIN_ASM_FUNC k_aabb_asm

    push    r0
    push    r1
    push    r2

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    shfi    b1, b1, -4
    addv    0xffff, b1l

    bkrep   b1l, 1f

    bkrep   15, 2f
    mov     [r0++], a0
    sub     [r1++], a0
    neg     a0, lt                  // |dx|
    mov     [r0++], a1
    sub     [r1++], a1
    neg     a1, lt                  // |dy|
    max_ge  a0, ^r0                 // a0 = max(a0, a1) (r0 is only copied to mixp)
    mov     [r0++], a1
    sub     [r1++], a1
    neg     a1, lt                  // |dz|
    max_ge  a0, ^r0
    mov     [r0++], a1
    add     [r1++], a1              // ra + rb
    sub     a1, a0
    dec     a0, always
    rol     a0, always              // carry = (max <= ra + rb)
    ror     b1, always
2:
    shfi    b1, b1, -8
    mov     b1h, [r2++]
1:

    pop     r2
    pop     r1
    pop     r0
    ret     always

// Q16.16 versions
// ---------------
//
// q16_mul(a, b) = ((ah * bh) << 16) + ((al * bl) >> 16) + ah * bl + al * bh
// (ah, bh signed, al, bl unsigned). The terms of several products are added
// in the accumulators before any shift:
//
// - a1 or b0: sum of ah * bh, shifted left by 16 at the end
// - a0: the other terms. "maa" adds the previous product shifted right by 16
//   (al * bl) while it starts the next multiplication.

// One component of the dot product. r0 and r1 point to the component
// (high half first). The first component uses "mpy" instead of "mac", as
// there is no previous product to add.
.macro DOT_Q16 first
    mov     [r1++], b0              // b0l = bh
    mov     [r1++], b1              // b1l = bl
    mov     [r0++], y0              // y0 = ah
.if \first
    mpy     y0, b0l, a0             // p = ah * bh
.else
    mac     y0, b0l, a0             // a0 += previous bh * al, p = ah * bh
.endif
    macsu   y0, b1l, a1             // a1 += ah * bh, p = ah * bl
    mov     [r0++], y0              // y0 = al
    macuu   y0, b1l, a0             // a0 += ah * bl, p = al * bl
    mov     y0, r3                  // r3 = al
    mov     b0l, y0                 // y0 = bh
    maasu   y0, r3, a0              // a0 += (al * bl) >> 16, p = bh * al
.endm

// void k_dot3_q16_asm(const ksphere_q16 *a, const ksphere_q16 *b,
//                     int32_t *out, uint16_t n)
//
// Requires n >= 1. The result wraps like the C version.

BEGIN_ASM_FUNC k_dot3_q16_asm

    push    y0
    push    r0
    push    r1
    push    r2
    push    r3

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    addv    0xffff, b1l

    bkrep   b1l, 1f
    clr     a0, a1
    DOT_Q16 1
    DOT_Q16 0
    DOT_Q16 0
    add     p*, a0
    shfi    a1, a1, 16
    add     a1, a0
    mov     a0h, [r2++]
    mov     a0l, [r2++]
    modr    [r0++]                  // Skip r
    modr    [r0++]
    modr    [r1++]
    modr    [r1++]
1:

    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always

// Adds q16_mul(d, d) of one component to a0 and b0, with d = a - b. The
// first component uses "mpy" instead of "maa", as there is no previous
// product to add. Leaves dl * dl in p.
.macro SQUARE_DIFF_Q16 first
    mov     [r0++], a1h
    or      [r0++], a1              // a1 = a
    subh    [r1++], a1
    subl    [r1++], a1              // a1 = d = a - b
    mov     a1h, y0                 // y0 = dh
.if \first
    mpy     y0, a1h, a0             // p = dh * dh
.else
    maa     y0, a1h, a0             // a0 += previous (dl * dl) >> 16
.endif
    add     p0, b0                  // b0 += dh * dh
    mpysu   y0, a1l, a0             // p = dh * dl
    add     p*, a0
    mov     a1l, y0                 // y0 = dl
    macuu   y0, a1l, a0             // a0 += dh * dl, p = dl * dl
.endm

// void k_sphere_q16_asm(const ksphere_q16 *a, const ksphere_q16 *b,
//                       uint16_t *mask, uint16_t n)
//
// Bit i = 1 if the sum of q16_mul(d, d) <= q16_mul(ra + rb, ra + rb).
// Requires n = 16 * k, k >= 1. The comparison uses the exact difference of
// both sides: it is the same as the C version while they don't overflow 32
// bits (the coordinates of this example are in [-2, 2)).

BEGIN_ASM_FUNC k_sphere_q16_asm

    push    y0
    push    r0
    push    r1
    push    r2

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    shfi    b1, b1, -4
    addv    0xffff, b1l

    bkrep   b1l, 1f

    bkrep   15, 2f
    clr     a0, always
    clr     b0, always
    SQUARE_DIFF_Q16 1
    SQUARE_DIFF_Q16 0
    SQUARE_DIFF_Q16 0

    // Subtract q16_mul(rs, rs), with rs = ra + rb
    mov     [r0++], a1h
    or      [r0++], a1
    addh    [r1++], a1
    addl    [r1++], a1              // a1 = rs
    mov     a1h, y0                 // y0 = rh
    maa     y0, a1h, a0             // a0 += (dzl * dzl) >> 16, p = rh * rh
    sub     p0, b0
    mpysu   y0, a1l, a0             // p = rh * rl
    sub     p*, a0
    sub     p*, a0
    mov     a1l, y0                 // y0 = rl
    macuu   y0, a1l, a1             // p = rl * rl (a1 isn't used any more)
    clr     a1, always
    add     p*, a1
    shfi    a1, a1, -16
    sub     a1, a0

    shfi    b0, b0, 16
    add     b0, a0                  // a0 = d2 - r2
    dec     a0, always
    rol     a0, always              // carry = (d2 <= r2)
    ror     b1, always
2:
    shfi    b1, b1, -8
    mov     b1h, [r2++]
1:

    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always
