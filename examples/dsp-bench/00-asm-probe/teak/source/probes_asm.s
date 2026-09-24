// SPDX-License-Identifier: CC0-1.0
//
// Small snippets of DSP assembly. Each one returns a 32-bit value in a0 that
// depends on the exact behaviour of the instructions it uses. The ARM9
// compares them with the values given by the teakra emulator, so any
// difference between the hardware and the emulator shows up.
//
// All snippets use pdata[] (32 words, set by the C code before each snippet)
// and the scratch words below.

#include <teak/asminc.h>

    .section .bss.probes, "aw", %nobits
    .align 2
pscratch:   .space 32

.macro PROLOGUE
    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
.endm

.macro EPILOGUE
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
    ret     always
.endm

// a0 += (rN - pdata) << 16, to check where a pointer ended
.macro ADD_PTR_OFFSET reg
    mov     \reg, a1
    mov     pdata, r3
    sub     r3, a1
    shfi    a1, a1, 16
    add     a1, a0
.endm

// ---- Multiplications with one memory operand ----

BEGIN_ASM_FUNC probe_mpy_zero_step
    PROLOGUE
    mov     pdata, r1
    mov     0x4d2, y0
    clr     a0, always
    mpy     y0, [r1], a0
    add     p*, a0
    ADD_PTR_OFFSET r1
    EPILOGUE

BEGIN_ASM_FUNC probe_mpy_postinc
    PROLOGUE
    mov     pdata, r1
    mov     0x4d2, y0
    clr     a0, always
    mpy     y0, [r1++], a0
    add     p*, a0
    ADD_PTR_OFFSET r1
    EPILOGUE

BEGIN_ASM_FUNC probe_mpy_postdec
    PROLOGUE
    mov     pdata+2, r1
    mov     0x4d2, y0
    clr     a0, always
    mpy     y0, [r1--], a0
    add     p*, a0
    ADD_PTR_OFFSET r1
    EPILOGUE

BEGIN_ASM_FUNC probe_mac_zero_step
    PROLOGUE
    mov     pdata+1, r1
    mov     0x64, y0
    clr     a0, always
    mpy     y0, [r1], a0
    mac     y0, [r1], a0
    add     p*, a0
    ADD_PTR_OFFSET r1
    EPILOGUE

// Dot product of pdata[0..3] and pdata[4..7], single bus
BEGIN_ASM_FUNC probe_mac_chain
    PROLOGUE
    mov     pdata, r0
    mov     pdata+4, r1
    clr     a0, always
    mov     [r1++], y0
    mpy     y0, [r0++], a0
    mov     [r1++], y0
    mac     y0, [r0++], a0
    mov     [r1++], y0
    mac     y0, [r0++], a0
    mov     [r1++], y0
    mac     y0, [r0++], a0
    add     p*, a0
    EPILOGUE

// y0 loaded from memory and used by the next instruction
BEGIN_ASM_FUNC probe_y0_load_use
    PROLOGUE
    mov     pdata+16, r2
    mov     pdata+2, r1
    clr     a0, always
    mov     [r2++], y0
    mpy     y0, [r1], a0
    add     p*, a0
    EPILOGUE

// Product used by the next instruction
BEGIN_ASM_FUNC probe_mpy_add_p_next
    PROLOGUE
    mov     pdata+3, r1
    mov     0x7b, y0
    mov     0x10, a0
    mpy     y0, [r1], a0
    add     p*, a0
    EPILOGUE

// Accumulation into a1, as in the FFT butterfly
BEGIN_ASM_FUNC probe_mac_into_a1
    PROLOGUE
    mov     pdata+5, r1
    clr     a1, always
    mov     0x37, y0
    mpy     y0, [r1++], a1
    mov     0x42, y0
    mac     y0, [r1], a1
    add     p*, a1
    mov     a1, a0
    EPILOGUE

// The complex multiplication of the FFT butterfly: b = pdata[16..17],
// (c, s, c, -s) = pdata[20..23]. Returns (tr << 16) | (ti & 0xFFFF).
BEGIN_ASM_FUNC probe_fft_twiddle
    PROLOGUE
    mov     pdata+16, r1
    mov     pdata+20, r2
    clr     a0, always
    mov     [r2++], y0
    mpy     y0, [r1++], a0
    mov     [r2++], y0
    mac     y0, [r1], a0
    add     p*, a0
    clr     a1, always
    mov     [r2++], y0
    mpy     y0, [r1--], a1
    mov     [r2++], y0
    mac     y0, [r1], a1
    add     p*, a1
    shfi    a0, a0, -15
    shfi    a1, a1, -15
    shfi    a0, a0, 16
    and     0xffff, a1
    or      a1l, a0
    EPILOGUE

// ---- Accumulator arithmetic ----

BEGIN_ASM_FUNC probe_sub_ab_bx
    PROLOGUE
    mov     0x3e8, a0
    mov     0x1388, b0l
    sub     a0, b0                  // b0 = b0 - a0
    mov     b0, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_add_ab_bx
    PROLOGUE
    mov     0x3e8, a0
    mov     0x1388, b0l
    add     a0, b0                  // b0 = b0 + a0
    mov     b0, a0
    EPILOGUE

// b0 = mem - tr, like the butterfly (mem is signed)
BEGIN_ASM_FUNC probe_sub_mem_bx
    PROLOGUE
    mov     pdata+17, r0            // -2000
    mov     0x1f4, a0               // 500
    mov     [r0], b0
    sub     a0, b0
    shfi    b0, b0, -1
    mov     b0, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_shfi_right15
    PROLOGUE
    mov     0x1234, a0
    shfi    a0, a0, 16
    or      0x5678, a0
    shfi    a0, a0, -15
    EPILOGUE

BEGIN_ASM_FUNC probe_shfi_negative
    PROLOGUE
    mov     0xedcc, a0              // -0x1234 sign-extended
    shfi    a0, a0, 16
    or      0x5678, a0
    shfi    a0, a0, -15
    EPILOGUE

// Stores of b0l with and without post-increment, read back
BEGIN_ASM_FUNC probe_store_b0l
    PROLOGUE
    mov     pdata+24, r1
    mov     0x1234, b0l
    mov     b0l, [r1++]
    mov     0x9abc, b0l
    mov     b0l, [r1]
    mov     pdata+24, r0
    mov     [r0++], a0
    shfi    a0, a0, 16
    or      [r0], a0
    ADD_PTR_OFFSET r1
    EPILOGUE

// add [addr16], aX with a value written just before
BEGIN_ASM_FUNC probe_add_mem_abs
    PROLOGUE
    mov     0x3e8, a1
    mov     a1l, [pscratch]
    mov     0x7, a0
    add     [pscratch], a0
    EPILOGUE

// Pointer update through an accumulator (end of a butterfly group)
BEGIN_ASM_FUNC probe_ptr_add_mem
    PROLOGUE
    mov     0x5, a1
    mov     a1l, [pscratch]
    mov     pdata+3, r0
    mov     r0, a0
    add     [pscratch], a0
    mov     a0l, r0
    mov     [r0], a0                // pdata[8]
    ADD_PTR_OFFSET r0
    EPILOGUE

// ---- Loops ----

// Last instruction of the block updates the pointer used by the next pass
BEGIN_ASM_FUNC probe_bkrep_last_ptr
    PROLOGUE
    mov     pdata, r1
    clr     a0, always
    bkrep   3, 1f
    mov     [r1], a1
    add     a1, a0
    mov     r1, a1
    add     0x2, a1
    mov     a1l, r1
1:
    ADD_PTR_OFFSET r1
    EPILOGUE

// Last instruction of the block is 2 words long
BEGIN_ASM_FUNC probe_bkrep_last_addv
    PROLOGUE
    mov     pdata, r1
    clr     a0, always
    bkrep   3, 1f
    mov     [r1], a1
    add     a1, a0
    addv    3, r1
1:
    ADD_PTR_OFFSET r1
    EPILOGUE

BEGIN_ASM_FUNC probe_nested_bkrep
    PROLOGUE
    mov     pdata, r1
    clr     a0, always
    bkrep   2, 2f
    mov     0x1, a1
    bkrep   a1l, 1f
    mov     [r1++], a1
    add     a1, a0
1:
    modr    [r1++]
2:
    ADD_PTR_OFFSET r1
    EPILOGUE

BEGIN_ASM_FUNC probe_rep_mac
    PROLOGUE
    mov     pdata, r1
    mov     0xb, y0
    clr     a0, always
    clr0
    rep     7
    mac     y0, [r1++], a0
    add     p*, a0
    ADD_PTR_OFFSET r1
    EPILOGUE

// ---- 32-bit multiplications ----

BEGIN_ASM_FUNC probe_mpysu
    PROLOGUE
    mov     pdata+23, r1            // 0xFFFF as unsigned
    mov     0xfffd, y0              // -3
    clr     a0, always
    mpysu   y0, [r1], a0
    add     p*, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_macsu
    PROLOGUE
    mov     pdata+23, r1
    mov     0xfffd, y0
    mov     0x100, a0
    mpysu   y0, [r1], a0
    macsu   y0, [r1], a0
    add     p*, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_macus
    PROLOGUE
    mov     pdata+21, r1            // -0x8000
    mov     0xffff, y0              // 65535 as unsigned
    clr     a0, always
    mpy     y0, [r1], a0
    macus   y0, [r1], a0
    add     p*, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_macuu
    PROLOGUE
    mov     pdata+23, r1            // 0xFFFF
    mov     0xfffe, y0              // 0xFFFE
    clr     a0, always
    mpy     y0, [r1], a0
    macuu   y0, [r1], a0
    add     p*, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_maa
    PROLOGUE
    mov     pdata+22, r1            // 0x1234
    mov     0x3, y0
    mov     0x7654, a0
    shfi    a0, a0, 16
    or      0x3210, a0
    mpy     y0, [r1], a0            // p0 = 3 * 0x1234
    maa     y0, [r1], a0            // a0 = (a0 >> 16) + p0 ?
    add     p*, a0
    EPILOGUE

// ---- Others ----

// "divs" reads its divisor from page 0 (word 0: the far end of the stack area,
// never reached by the stack)
BEGIN_ASM_FUNC probe_divs
    PROLOGUE
    mov     [0x0], a1
    push    a1l
    mov     0x7, a1                 // Divisor
    mov     a1l, [0x0]
    mov     0x3e8, a0               // 1000 / 7
    rep     15
    divs    [page:0x0u8], a0
    pop     a1l
    mov     a1l, [0x0]
    EPILOGUE

BEGIN_ASM_FUNC probe_exp
    PROLOGUE
    mov     0x1234, a1
    exp     a1, a0                  // Normalization shift of a1
    EPILOGUE

BEGIN_ASM_FUNC probe_clr_cond
    PROLOGUE
    mov     0xfff0, a0              // -16
    cmp     0x0, a0
    clr     a0, lt
    mov     0x20, a1
    cmp     0x0, a1
    clr     a1, lt
    shfi    a1, a1, 8
    add     a1, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_sqr
    PROLOGUE
    mov     pdata+17, r0            // -2000
    clr     a0, always
    sqr     [r0], a0
    add     p*, a0
    EPILOGUE

BEGIN_ASM_FUNC probe_shfc_sv
    PROLOGUE
    mov     0xfffb, a1              // sv = -5
    mov     a1l, sv
    mov     0x4000, a0
    shfi    a0, a0, 8
    shfc    a0, a0, always
    EPILOGUE

// ---- Forms used only by k_gauss5_asm (03-blur), which fails on hardware ----

// ALU operations with r7 + offset operands
BEGIN_ASM_FUNC probe_alu_r7_offset
    PROLOGUE
    push    r7
    mov     pdata, r7
    mov     [r7+0x1], a0            // -5
    add     [r7+0x3], a0            // + 11
    sub     [r7+0x5], a0            // - 17
    or      [r7+0x1f], a0           // | 12
    add     [r7], a0                // + 3
    mov     [r7+0x2], a1            // 7
    add     [r7+0x4], a1            // - 13
    shfi    a1, a1, 16
    add     a1, a0
    pop     r7
    EPILOGUE

// Loop count loaded with "mov imm16" just before "bkrep": 3840 iterations
BEGIN_ASM_FUNC probe_long_count
    PROLOGUE
    clr     a0, always
    mov     3839, b1
    bkrep   b1l, 1f
    inc     a0, always
    nop
1:
    EPILOGUE

// "modr" and "addv" on a register just written by a move
BEGIN_ASM_FUNC probe_modr_after_mov
    PROLOGUE
    push    r7
    mov     pdata, r7
    mov     r7, r1
    modr    [r1++]
    mov     [r1], a0                // pdata[1]
    mov     r7, r2
    addv    2, r2
    mov     [r2], a1                // pdata[2]
    shfi    a1, a1, 16
    add     a1, a0
    pop     r7
    EPILOGUE

// Pointer modified by "addv" just before "bkrep"
BEGIN_ASM_FUNC probe_addv_before_bkrep
    PROLOGUE
    push    r4
    clr     a0, always
    mov     pdata, r4
    addv    4, r4
    bkrep   3, 1f
    add     [r4++], a0              // pdata[4] + ... + pdata[7]
    nop
1:
    ADD_PTR_OFFSET r4
    pop     r4
    EPILOGUE

// Same as the GAUSS macro of 03-blur
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

// Sum of the 12 words at pscratch, each shifted by 3 more bits than the
// previous one (low 32 bits)
.macro SCRATCH_SUM count
    mov     pscratch, r5
    clr     a0, always
    bkrep   \count - 1, 9f
    shfi    a0, a0, 3
    add     [r5++], a0
9:
.endm

// The 4 edge pixels of a row of k_gauss5_asm (r7 + offset operands). The
// last one reads [r7 - 3] just after "addv 8, r7": on DSi hardware, that read
// uses the old r7 (pdata[5] instead of pdata[13]), so the result is 0x369F
// instead of 0x369B. r7off_after_nop is the same with a "nop" after "addv".
.macro GAUSS_EDGES after_addv
    PROLOGUE
    push    r5
    push    r7
    mov     pdata + 8, r7
    mov     pscratch, r5
    GAUSS   [r7], [r7], [r7], [r7], [r7+0x1], [r7+0x2]
    GAUSS   [r7], [r7], [r7+0x1], [r7+0x1], [r7+0x2], [r7+0x3]
    GAUSS   [r7+0x4], [r7+0x5], [r7+0x6], [r7+0x6], [r7+0x7], [r7+0x7]
    addv    8, r7
    \after_addv
    GAUSS   [r7-0x3], [r7-0x2], [r7-0x1], [r7-0x1], [r7-0x1], [r7-0x1]
    SCRATCH_SUM 4
    pop     r7
    pop     r5
    EPILOGUE
.endm

BEGIN_ASM_FUNC probe_r7off_after_addv
    GAUSS_EDGES ""

BEGIN_ASM_FUNC probe_r7off_after_nop
    GAUSS_EDGES "nop"

// The vertical loop of k_gauss5_asm: 5 pointers set just before "bkrep",
// count loaded with "mov imm16"
BEGIN_ASM_FUNC probe_gauss_loop
    PROLOGUE
    push    r4
    push    r5
    push    r6
    mov     pdata, r6
    mov     pscratch, r5
    mov     r6, r0
    mov     r6, r1
    addv    1, r1
    mov     r6, r2
    addv    2, r2
    mov     r6, r3
    addv    3, r3
    mov     r6, r4
    addv    4, r4
    mov     11, b1
    bkrep   b1l, 1f
    GAUSS   [r0++], [r1++], [r2], [r2++], [r3++], [r4++]
1:
    SCRATCH_SUM 12
    pop     r6
    pop     r5
    pop     r4
    EPILOGUE
