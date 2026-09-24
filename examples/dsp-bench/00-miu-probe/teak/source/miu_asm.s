// SPDX-License-Identifier: CC0-1.0
//
// Reads of the MIU probe. Each function sets the MIU registers MIU_PAGE0CFG
// (0x8114) and MIU_MISC (0x811A) to the given values, reads memory, and puts
// the old values back before touching the stack or any global variable.
//
// Nothing between the two register writes uses the stack or low memory, so a
// setting that moves the data memory can't break the program.

#include <teak/asminc.h>

#define MIU_PAGE0CFG    0x8114
#define MIU_MISC        0x811A

// uint16_t miu_read(uint16_t address)
BEGIN_ASM_FUNC miu_read
    push    r1
    mov     a0l, r1
    mov     [r1], a0
    pop     r1
    ret     always

// void miu_fill(uint16_t start, uint16_t count_minus_1)
//
// Writes its own address to each word of the area.
BEGIN_ASM_FUNC miu_fill
    push    r0
    mov     a0l, r0
    bkrep   a1l, 1f
    mov     r0, a0
    mov     a0l, [r0++]
1:
    pop     r0
    ret     always

// Common start of both reads: a0l = new MISC, a1l = new PAGE0CFG,
// b0l = Y address (r4), b1l = X address (r0). The old values stay in r2/r3.
.macro SET_MIU
    push    y0
    push    r0
    push    r1
    push    r2
    push    r3
    push    r4
    push    r5
    mov     b0l, r4
    mov     b1l, r0
    mov     MIU_PAGE0CFG, r1
    mov     MIU_MISC, r5
    mov     [r1], r2
    mov     [r5], r3
    mov     a1l, [r1]
    mov     a0l, [r5]
    nop
    nop
.endm

.macro RESTORE_MIU
    nop
    nop
    mov     r2, [r1]
    mov     r3, [r5]
    nop
    nop
.endm

.macro POP_ALL
    pop     r5
    pop     r4
    pop     r3
    pop     r2
    pop     r1
    pop     r0
    pop     y0
.endm

// uint32_t miu_dual(uint16_t misc, uint16_t page0cfg, uint16_t y_address,
//                   uint16_t x_address)
//
// One "mpy [r4], [r0]": y0 is read from [r4] (Y bus) and x0 from [r0]
// (X bus). Stores y0 in miu_dual_y and returns the product x0 * y0.
BEGIN_ASM_FUNC miu_dual
    SET_MIU
    mpy     [r4], [r0], a0
    RESTORE_MIU
    mov     y0, a1l
    mov     a1l, [miu_dual_y]
    clr     a0, always
    add     p*, a0
    POP_ALL
    ret     always

// uint32_t miu_single(uint16_t misc, uint16_t page0cfg, uint16_t y_address,
//                     uint16_t x_address)
//
// The same addresses read with normal single-bus moves. Returns
// ([y_address] << 16) | [x_address].
BEGIN_ASM_FUNC miu_single
    SET_MIU
    mov     [r4], b0
    mov     [r0], b1
    RESTORE_MIU
    mov     b0l, a0h
    mov     b1l, a1l
    or      a1, a0
    POP_ALL
    ret     always

// void miu_fill_neg(uint16_t misc, uint16_t page0cfg, uint16_t start,
//                   uint16_t count_minus_1)
//
// With the given settings, writes -address to each word of the area. This
// shows where the writes go when the setting maps that area to another
// memory (Y memory) than the one filled by miu_fill().
BEGIN_ASM_FUNC miu_fill_neg
    SET_MIU
    bkrep   b1l, 1f
    mov     r4, a0
    neg     a0, always
    mov     a0l, [r4++]
1:
    RESTORE_MIU
    POP_ALL
    ret     always
