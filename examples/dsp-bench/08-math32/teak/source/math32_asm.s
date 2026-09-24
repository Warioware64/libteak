// SPDX-License-Identifier: CC0-1.0
//
// Hand-written DSP kernels of the 32-bit math example. They give the same
// results as the C versions in common/math32.c and common/fixed.c.
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments in a0l, a1l,
// b0l, b1l; y0, y1 and r0-r7 must be preserved. A 32-bit value is stored in
// memory with the high half at the lower address.
//
// The accumulators have 40 bits and saturation is disabled (see crt0.s), so
// 32-bit additions wrap like in C when only the low 32 bits are stored.
//
// A 32-bit value is loaded with "mov [rN++], aXh" (high half, sign extended,
// low half cleared) followed by "or [rN++], aX" (low half).
//
// All functions require n >= 1.

#include <teak/asminc.h>

.macro PUSH_R0_R2
    push    r0
    push    r1
    push    r2
.endm

.macro POP_R0_R2
    pop     r2
    pop     r1
    pop     r0
.endm

// void k_alu_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n)
//
// out[i] = ((a[i] + b[i]) >> 3) ^ (a[i] - (b[i] << 2))

BEGIN_ASM_FUNC k_alu_asm

    PUSH_R0_R2

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    addv    0xffff, b1l             // "bkrep" runs its block count + 1 times

    bkrep   b1l, 1f
    mov     [r0++], a0h
    or      [r0++], a0              // a0 = a
    mov     [r1++], a1h
    or      [r1++], a1              // a1 = b
    mov     a0, b0
    add     a1, b0                  // b0 = a + b (the carry can reach bit 32)
    shfi    a1, a1, 2               // a1 = b << 2
    sub     a1, a0                  // a0 = a - (b << 2)
    shfi    b0, b0, 8               // Drop bits 32-39...
    shfi    b0, b0, -11             // ...then sign extend and shift right 3
    mov     b0, a1
    xor     a1, a0
    mov     a0h, [r2++]
    mov     a0l, [r2++]
1:

    POP_R0_R2
    ret     always

// void k_mul_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n)
//
// out[i] = a[i] * b[i] (low 32 bits):
//
//   al * bl + ((ah * bl + al * bh) << 16)
//
// Only al * bl needs an unsigned multiplication (macuu). The signedness of
// the other two doesn't matter because only their low 16 bits are used.

BEGIN_ASM_FUNC k_mul_asm

    push    y0
    PUSH_R0_R2

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    addv    0xffff, b1l

    bkrep   b1l, 1f
    mov     [r1++], b1              // b1l = bh
    mov     [r0++], y0              // y0 = ah
    mpy     y0, [r1], a1            // p = ah * bl
    mov     [r0++], y0              // y0 = al
    clr     a1, always
    mac     y0, b1l, a1             // a1 = ah * bl, p = al * bh
    macuu   y0, [r1++], a1          // a1 += al * bh, p = al * bl (unsigned)
    shfi    a1, a1, 16
    add     p*, a1
    mov     a1h, [r2++]
    mov     a1l, [r2++]
1:

    POP_R0_R2
    pop     y0
    ret     always

// void k_q16mul_asm(const int32_t *a, const int32_t *b, int32_t *out,
//                   uint16_t n)
//
// out[i] = (a[i] * b[i]) >> 16 (Q16.16), exactly like q16_mul():
//
//   ((ah * bh) << 16) + ((al * bl) >> 16) + bh * al + bl * ah
//
// with ah, bh signed and al, bl unsigned. "maasu" adds the previous product
// shifted right by 16 (al * bl), then starts a signed * unsigned product.

BEGIN_ASM_FUNC k_q16mul_asm

    push    y0
    PUSH_R0_R2

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    addv    0xffff, b1l

    bkrep   b1l, 1f
    mov     [r0++], y0              // y0 = ah
    mov     [r1++], a1              // a1l = bh
    mpy     y0, a1l, a0             // p = ah * bh
    mov     y0, b1l                 // b1l = ah
    mov     [r0++], y0              // y0 = al
    clr     a0, always
    macuu   y0, [r1], a0            // a0 = ah * bh, p = al * bl (unsigned)
    mov     y0, b0l                 // b0l = al
    shfi    a0, a0, 16
    mov     a1l, y0                 // y0 = bh
    maasu   y0, b0l, a0             // a0 += (al * bl) >> 16, p = bh * al
    mov     [r1++], y0              // y0 = bl
    macus   y0, b1l, a0             // a0 += bh * al, p = bl * ah
    add     p*, a0
    mov     a0h, [r2++]
    mov     a0l, [r2++]
1:

    POP_R0_R2
    pop     y0
    ret     always

// void k_div_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n)
//
// out[i] = a[i] / b[i], rounded towards zero (b[i] != 0), like sdiv32().
//
// Unsigned long division of |a| by |b|, one bit per step:
//
// - b0: |a| << 8, so that its highest bit is bit 39 of the accumulator. Each
//   "rol b0" moves its highest bit into the carry and the carry (the previous
//   quotient bit) into bit 0. After 33 steps, the quotient is in bits 0-31.
// - a0: remainder. "rol a0" shifts the next bit of |a| into it.
// - b1: -|b|. Adding it to the remainder sets the carry when the remainder is
//   greater than or equal to |b|: that is the quotient bit, and the
//   difference becomes the new remainder.

BEGIN_ASM_FUNC k_div_asm

    PUSH_R0_R2
    push    r3
    push    r4

    mov     a0l, r0
    mov     a1l, r1
    mov     b0l, r2
    addv    0xffff, b1l

    bkrep   b1l, 1f
    mov     [r0++], a0h
    or      [r0++], a0              // a0 = a
    mov     a0h, r3                 // Sign of a in bit 15
    neg     a0, lt                  // a0 = |a|
    mov     [r1++], a1h
    or      [r1++], a1              // a1 = b
    mov     a1h, r4                 // Sign of b in bit 15
    neg     a1, ge                  // a1 = -|b|
    mov     a1, b1
    shfi    a0, b0, 8
    clr     a0, always

    bkrep   31, 2f
    rol     b0, always
    rol     a0, always
    copy    a1, always              // a1 = a0
    add     b1, a1                  // a1 = remainder - |b|, carry = quotient bit
    copy    a0, c
2:
    rol     b0, always              // Last quotient bit

    mov     b0, a0                  // a0 = |a / b| (bits 0-31)
    mov     r3, a1
    xor     r4, a1
    tstb    a1l, 15                 // Signs of a and b differ?
    neg     a0, eq                  // (eq: the tested bit is 1)
    mov     a0h, [r2++]
    mov     a0l, [r2++]
1:

    pop     r4
    pop     r3
    POP_R0_R2
    ret     always

// void k_sqrt_asm(const int32_t *a, int32_t *out, uint16_t n)
//
// out[i] = floor(sqrt((uint32_t)a[i])), like isqrt32().
//
// Digit by digit: 2 bits of the input go into the remainder at each step,
// and the root gets one bit:
//
//   rem = (rem << 2) | next 2 bits; t = 4 * root + 1
//   if rem >= t: rem -= t, root = 2 * root + 1; else root = 2 * root
//
// - b0: input << 8 (see k_div_asm), a0: remainder, b1: root.
// - a1 = ~(4 * root) = -t. Adding the remainder sets the carry when
//   rem >= t, and "rol b1" shifts the carry into the root.

BEGIN_ASM_FUNC k_sqrt_asm

    PUSH_R0_R2

    mov     a0l, r0
    mov     a1l, r2
    addv    0xffff, b0l

    bkrep   b0l, 1f
    mov     [r0++], a0h
    or      [r0++], a0
    shfi    a0, b0, 8               // Bits 32-39 of a0 are shifted out
    clr     a0, always
    clr     b1, always

    bkrep   15, 2f
    rol     b0, always
    rol     a0, always
    rol     b0, always
    rol     a0, always
    shfi    b1, a1, 2
    not     a1, always              // a1 = -(4 * root + 1)
    add     a0, a1                  // a1 = rem - t, carry = (rem >= t)
    copy    a0, c
    rol     b1, always
2:

    mov     b1h, [r2++]
    mov     b1l, [r2++]
1:

    POP_R0_R2
    ret     always
