// SPDX-License-Identifier: CC0-1.0
//
// Hand-written DSP versions of the kernels in common/bench_kernels.c.
//
// Calling convention of the Teak LLVM toolchain: 16-bit arguments are passed
// in a0l, a1l, b0l, b1l; 32-bit results are returned in a0. a0, a1, b0, b1,
// x0, x1, p0 and sv can be clobbered; y0, y1 and r0-r7 must be preserved.

#include <teak/asminc.h>

// uint32_t bench_dot16_asm(const int16_t *a, const int16_t *b, uint16_t n)
//
// Requires n >= 2. "mac" adds the previous product to the accumulator and
// starts the next multiplication, so the first product is started with "mpy"
// and the last one is added after the loop.
//
// The forms of "mpy"/"mac" with two memory operands (one multiply per cycle)
// aren't used: they read the second operand through the Y data bus, and on DSi
// hardware that returned the data of a[] instead of b[] (the Y memory space is
// configured by the MIU, and emulators don't model it). b[] is loaded into y0
// with a normal read instead, so each element takes two instructions.

BEGIN_ASM_FUNC bench_dot16_asm

    push    y0
    push    r0
    push    r1

    mov     a0l, r0                 // a[]
    mov     a1l, r1                 // b[]
    addv    0xfffe, b0l             // "bkrep" runs its block count + 1 times

    clr     a0, always
    mov     [r1++], y0
    mpy     y0, [r0++], a0          // p0 = a[0] * b[0]
    bkrep   b0l, 1f
    mov     [r1++], y0
    mac     y0, [r0++], a0          // a0 += p0, p0 = a[i] * b[i]
1:
    add     p*, a0                  // a0 += last product

    pop     r1
    pop     r0
    pop     y0
    ret     always

// uint32_t bench_sum16_asm(const uint16_t *a, uint16_t n)
//
// Requires n >= 1. "addl" adds a zero-extended 16-bit value.

BEGIN_ASM_FUNC bench_sum16_asm

    push    r0

    mov     a0l, r0
    addv    0xffff, a1l             // n - 1

    clr     a0, always
    rep     a1l
    addl    [r0++], a0

    pop     r0
    ret     always

// uint32_t bench_xorshift16_asm(uint16_t seed, uint16_t steps)
//
// Requires steps >= 1. The state is kept zero-extended in a0 and the xor
// accumulator in a1, so right shifts of a0 are logical and "xor" with a 16-bit
// register only changes the low 16 bits.

BEGIN_ASM_FUNC bench_xorshift16_asm

    push    r3

    mov     a1l, r3
    addv    0xffff, r3              // steps - 1 ("bkrep" runs count + 1 times)

    clr     a1, always
    or      a0l, a1                 // Zero-extend the state into a1...
    mov     a1, a0                  // ...and move it back to a0
    clr     a1, always

    bkrep   r3, 1f
    shfi    a0, b0, 7
    xor     b0l, a0                 // s ^= s << 7
    shfi    a0, b0, -9
    xor     b0l, a0                 // s ^= s >> 9
    shfi    a0, b0, 8
    xor     b0l, a0                 // s ^= s << 8
    xor     a0l, a1                 // acc ^= s
1:

    shfi    a1, a1, 16
    or      a0l, a1                 // (acc << 16) | s
    mov     a1, a0

    pop     r3
    ret     always
