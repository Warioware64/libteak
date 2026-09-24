// SPDX-License-Identifier: CC0-1.0
//
// Each test exercises one feature of the code generator. Inputs are volatile so
// that the compiler can't compute results at build time, and tests aren't
// inlined so that each one is compiled on its own.
//
// "int" is 16 bits wide on the DSP and 32 bits wide on the ARM9, so every
// expression uses fixed-width types and casts to behave the same on both CPUs.

#include "cgtests.h"

#define NOINLINE __attribute__((noinline))

#define ARRAY_LEN 16

volatile uint16_t in_u16[4] = { 0x1234, 0xFEDC, 0x8001, 0x0007 };
volatile int16_t in_s16[4] = { -1000, 1234, -32768, 77 };
volatile uint32_t in_u32[2] = { 0x12345678, 0x9ABCDEF0 };
volatile uint16_t in_n = 1000;
volatile uint16_t in_len = ARRAY_LEN;

static uint16_t array_u[ARRAY_LEN];
static int16_t array_s[ARRAY_LEN];
static uint16_t global_counter;

void cg_init(void)
{
    for (uint16_t i = 0; i < ARRAY_LEN; i++)
    {
        array_u[i] = (uint16_t)(0x1111u * (i + 1));
        array_s[i] = (int16_t)(i * 700) - 5000;
    }

    global_counter = 0;
}

// Returns and constants

NOINLINE static uint32_t t_const32(void)
{
    return 0x12345678;
}

NOINLINE static uint32_t t_zext16(void)
{
    return in_u16[1];
}

NOINLINE static uint32_t t_sext16(void)
{
    return (uint32_t)(int32_t)in_s16[0];
}

// 16-bit ALU

NOINLINE static uint32_t t_add16(void)
{
    return (uint16_t)(in_u16[0] + in_u16[1]);
}

NOINLINE static uint32_t t_sub16(void)
{
    return (uint16_t)(in_u16[0] - in_u16[1]);
}

NOINLINE static uint32_t t_and16(void)
{
    return (uint16_t)(in_u16[0] & in_u16[1]);
}

NOINLINE static uint32_t t_or16(void)
{
    return (uint16_t)(in_u16[0] | in_u16[2]);
}

NOINLINE static uint32_t t_xor16(void)
{
    return (uint16_t)(in_u16[0] ^ in_u16[1]);
}

NOINLINE static uint32_t t_not16(void)
{
    return (uint16_t)~in_u16[0];
}

NOINLINE static uint32_t t_neg16(void)
{
    return (uint16_t)(-in_s16[1]);
}

// 16-bit shifts

NOINLINE static uint32_t t_shl16(void)
{
    return (uint16_t)(in_u16[0] << 3);
}

NOINLINE static uint32_t t_lshr16(void)
{
    return (uint16_t)(in_u16[1] >> 5);
}

NOINLINE static uint32_t t_ashr16(void)
{
    return (uint16_t)(in_s16[0] >> 3);
}

NOINLINE static uint32_t t_vshl16(void)
{
    return (uint16_t)(in_u16[0] << in_u16[3]);
}

NOINLINE static uint32_t t_vlshr16(void)
{
    return (uint16_t)(in_u16[1] >> in_u16[3]);
}

// 32-bit ALU and shifts

NOINLINE static uint32_t t_add32(void)
{
    return in_u32[0] + in_u32[1];
}

NOINLINE static uint32_t t_sub32(void)
{
    return in_u32[0] - in_u32[1];
}

NOINLINE static uint32_t t_and32(void)
{
    return in_u32[0] & in_u32[1];
}

NOINLINE static uint32_t t_xor32(void)
{
    return in_u32[0] ^ in_u32[1];
}

NOINLINE static uint32_t t_shl32(void)
{
    return (uint32_t)in_u16[0] << 16;
}

NOINLINE static uint32_t t_lshr32(void)
{
    return in_u32[1] >> 12;
}

NOINLINE static uint32_t t_ashr32(void)
{
    return (uint32_t)((int32_t)in_u32[1] >> 12);
}

// Multiplication

NOINLINE static uint32_t t_mul16(void)
{
    return (uint16_t)(in_u16[0] * in_u16[3]);
}

NOINLINE static uint32_t t_muls32(void)
{
    return (uint32_t)((int32_t)in_s16[0] * in_s16[1]);
}

NOINLINE static uint32_t t_mulu32(void)
{
    return (uint32_t)in_u16[0] * in_u16[1];
}

NOINLINE static uint32_t t_mulk32(void)
{
    return (uint32_t)((int32_t)in_s16[1] * 1000);
}

// Comparisons and branches: each bit is the result of one comparison

NOINLINE static uint32_t t_cmp16(void)
{
    int16_t a = in_s16[0], b = in_s16[1];
    uint16_t ua = in_u16[0], ub = in_u16[1];
    uint32_t r = 0;

    if (a < b)
        r |= 1 << 0;
    if (a > b)
        r |= 1 << 1;
    if (a == b)
        r |= 1 << 2;
    if (ua < ub)
        r |= 1 << 3;
    if (ua > ub)
        r |= 1 << 4;
    if (a < 0)
        r |= 1 << 5;
    if (ua >= 0x8000)
        r |= 1 << 6;

    return r;
}

NOINLINE static uint32_t t_cmp32(void)
{
    uint32_t a = in_u32[0], b = in_u32[1];
    uint32_t r = 0;

    if (a < b)
        r |= 1 << 0;
    if (a > b)
        r |= 1 << 1;
    if ((int32_t)a < (int32_t)b)
        r |= 1 << 2;
    if (a == 0x12345678)
        r |= 1 << 3;

    return r;
}

// Loops

NOINLINE static uint32_t t_loop(void)
{
    uint16_t n = in_n;
    uint16_t count = 0;

    for (uint16_t i = 0; i < n; i++)
        count += 3;

    return count;
}

NOINLINE static uint32_t t_loop32(void)
{
    uint16_t n = in_n;
    uint32_t count = 0;

    for (uint16_t i = 0; i < n; i++)
        count += 100;

    return count;
}

// Memory

NOINLINE static uint32_t t_sumu(void)
{
    uint32_t sum = 0;

    for (uint16_t i = 0; i < in_len; i++)
        sum += array_u[i];

    return sum;
}

NOINLINE static uint32_t t_sums(void)
{
    uint32_t sum = 0;

    for (uint16_t i = 0; i < in_len; i++)
        sum += (uint32_t)(int32_t)array_s[i];

    return sum;
}

NOINLINE static uint32_t t_sum16(void)
{
    uint16_t sum = 0;

    for (uint16_t i = 0; i < in_len; i++)
        sum += array_u[i];

    return sum;
}

// Note: local arrays aren't supported by the compiler yet (taking the address
// of a stack object fails), so this buffer is static.
static uint16_t tmp[ARRAY_LEN];

NOINLINE static uint32_t t_store(void)
{

    for (uint16_t i = 0; i < in_len; i++)
        tmp[i] = (uint16_t)(array_u[i] ^ 0x5555);

    uint16_t x = 0;
    for (uint16_t i = 0; i < in_len; i++)
        x = (uint16_t)(x + tmp[i] * (i + 1));

    return x;
}

NOINLINE static uint32_t t_global(void)
{
    for (uint16_t i = 0; i < 10; i++)
        global_counter += in_u16[3];

    return global_counter;
}

NOINLINE static uint32_t t_dot(void)
{
    uint32_t sum = 0;

    for (uint16_t i = 0; i < in_len; i++)
        sum += (uint32_t)((int32_t)array_s[i] * array_s[ARRAY_LEN - 1 - i]);

    return sum;
}

// Calls

NOINLINE static uint16_t helper4(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    return (uint16_t)((a ^ b) + (c & d));
}

NOINLINE static uint32_t helper32(uint32_t a, uint32_t b)
{
    return a - (b >> 1);
}

NOINLINE static uint32_t t_call4(void)
{
    return helper4(in_u16[0], in_u16[1], in_u16[2], in_u16[3]);
}

NOINLINE static uint32_t t_call32(void)
{
    return helper32(in_u32[0], in_u32[1]);
}

NOINLINE static uint32_t t_nested(void)
{
    uint16_t x = helper4(in_u16[0], 1, 2, 3);
    uint16_t y = helper4(x, in_u16[1], 0xFFFF, in_u16[3]);
    return ((uint32_t)x << 16) | y;
}

// Many live values, to force spills to the stack

NOINLINE static uint32_t t_spill(void)
{
    uint16_t a = in_u16[0], b = in_u16[1], c = in_u16[2], d = in_u16[3];
    uint16_t e = (uint16_t)(a + b), f = (uint16_t)(c ^ d), g = (uint16_t)(a - d);
    uint16_t h = (uint16_t)(b & c), i = (uint16_t)(e | f), j = (uint16_t)(g + h);

    uint16_t x = helper4(a, b, c, d);

    return ((uint32_t)(uint16_t)(a + b + c + d + e + f + g + h + i + j) << 16) | x;
}

// Switch statement

NOINLINE static uint16_t select_value(uint16_t k)
{
    switch (k)
    {
        case 0:
            return 0x1111;
        case 1:
            return 0x2222;
        case 2:
            return 0x3333;
        case 3:
            return 0x4444;
        case 4:
            return 0x5555;
        case 7:
            return 0x7777;
        default:
            return 0xDEAD;
    }
}

NOINLINE static uint32_t t_switch(void)
{
    uint16_t r = 0;

    for (uint16_t k = 0; k < 9; k++)
        r = (uint16_t)(r + select_value(k) * (k + 1));

    return r;
}

// The kernels of the benchmark, with small sizes

NOINLINE static uint32_t xorshift16(uint16_t s, uint16_t steps)
{
    uint16_t acc = 0;

    for (uint16_t i = 0; i < steps; i++)
    {
        s ^= (uint16_t)(s << 7);
        s ^= s >> 9;
        s ^= (uint16_t)(s << 8);
        acc ^= s;
    }

    return ((uint32_t)acc << 16) | s;
}

NOINLINE static uint32_t t_xorsh1(void)
{
    return xorshift16(in_u16[0], 1);
}

NOINLINE static uint32_t t_xorsh16(void)
{
    return xorshift16(in_u16[0], in_len);
}

NOINLINE static uint32_t t_lshr_only(void)
{
    // Only the logical shift of the xorshift step
    uint16_t s = in_u16[1];
    s ^= s >> 9;
    return s;
}

NOINLINE static uint32_t t_shl_only(void)
{
    // Only the left shifts of the xorshift step
    uint16_t s = in_u16[1];
    s ^= (uint16_t)(s << 7);
    s ^= (uint16_t)(s << 8);
    return s;
}

static uint32_t run(uint16_t index)
{
    switch (index)
    {
        case 0: return t_const32();
        case 1: return t_zext16();
        case 2: return t_sext16();
        case 3: return t_add16();
        case 4: return t_sub16();
        case 5: return t_and16();
        case 6: return t_or16();
        case 7: return t_xor16();
        case 8: return t_not16();
        case 9: return t_neg16();
        case 10: return t_shl16();
        case 11: return t_lshr16();
        case 12: return t_ashr16();
        case 13: return t_vshl16();
        case 14: return t_vlshr16();
        case 15: return t_add32();
        case 16: return t_sub32();
        case 17: return t_and32();
        case 18: return t_xor32();
        case 19: return t_shl32();
        case 20: return t_lshr32();
        case 21: return t_ashr32();
        case 22: return t_mul16();
        case 23: return t_muls32();
        case 24: return t_mulu32();
        case 25: return t_mulk32();
        case 26: return t_cmp16();
        case 27: return t_cmp32();
        case 28: return t_loop();
        case 29: return t_loop32();
        case 30: return t_sumu();
        case 31: return t_sums();
        case 32: return t_sum16();
        case 33: return t_store();
        case 34: return t_global();
        case 35: return t_dot();
        case 36: return t_call4();
        case 37: return t_call32();
        case 38: return t_nested();
        case 39: return t_spill();
        case 40: return t_switch();
        case 41: return t_lshr_only();
        case 42: return t_shl_only();
        case 43: return t_xorsh1();
        case 44: return t_xorsh16();
        default: return 0xFFFFFFFF;
    }
}

uint16_t cg_test_count(void)
{
    return 45;
}

uint32_t cg_run(uint16_t index)
{
    // Some tests modify global state, so it's reset before each test
    cg_init();
    return run(index);
}
