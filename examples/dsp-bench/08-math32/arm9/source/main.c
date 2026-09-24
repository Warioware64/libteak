// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 08: 32-bit arithmetic on 1024 values.
//
// The DSP multiplier takes 16-bit operands and the DSP has no division, so
// 32-bit operations cost more there. The same C code is built for both CPUs:
//
// - alu:    additions, subtractions, shifts and xor
// - mul:    32x32 -> 32 multiplication (3 multiplications on the DSP)
// - q16mul: Q16.16 multiplication, from 16-bit pieces (fixed.c)
// - div:    signed division by shift and subtract (fixed.c). The "ARMas"
//           column is the C "/" operator of the ARM9 (libgcc).
// - divHW:  the division hardware of the DS (ARM9 only)
// - sqrt:   integer square root

#include <stdio.h>

#include <nds.h>

#include "math32.h"
#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "ui.h"

static void arm_alu(void)
{
    k_alu(val_a, val_b, val_out, N_VALUES);
}

static void arm_mul(void)
{
    k_mul(val_a, val_b, val_out, N_VALUES);
}

static void arm_q16mul(void)
{
    k_q16mul(val_a, val_b, val_out, N_VALUES);
}

static void arm_div(void)
{
    k_div(val_a, val_b, val_out, N_VALUES);
}

static void arm_div_native(void)
{
    for (int i = 0; i < N_VALUES; i++)
        val_out[i] = val_a[i] / val_b[i];
}

static void arm_div_hw(void)
{
    for (int i = 0; i < N_VALUES; i++)
        val_out[i] = div32(val_a[i], val_b[i]);
}

static void arm_sqrt(void)
{
    k_sqrt(val_a, val_out, N_VALUES);
}

static void arm_nothing(void)
{
}

static bench_row rows[] = {
    { "alu", JOB_ALU_C, arm_alu, NULL, JOB_ALU_C, JOB_ALU_ASM },
    { "mul", JOB_MUL_C, arm_mul, NULL, JOB_MUL_C, JOB_MUL_ASM },
    { "q16mul", JOB_Q16MUL_C, arm_q16mul, NULL, JOB_Q16MUL_C, JOB_Q16MUL_ASM },
    { "div", JOB_DIV_C, arm_div, arm_div_native, JOB_DIV_C, JOB_DIV_ASM },
    { "divHW", JOB_DIV_C, arm_div_hw, NULL, NO_JOB, NO_JOB },
    { "sqrt", JOB_SQRT_C, arm_sqrt, NULL, JOB_SQRT_C, JOB_SQRT_ASM },
    // Checks that both CPUs have generated the same inputs
    { "input", JOB_INPUTS, arm_nothing, NULL, JOB_INPUTS, NO_JOB },
};

#define NUM_ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static uint32_t checksum(uint16_t job)
{
    return math32_checksum(job);
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 08: 32-bit x1024", s, bench_errors(rows, NUM_ROWS));

    switch (s->page)
    {
        case 0:
            bench_print_times(rows, NUM_ROWS);
            printf("div ARMas = C \"/\" (libgcc)\n");
            printf("divHW = DS division hardware\n");
            break;
        case 1:
            bench_print_speedup(rows, NUM_ROWS);
            break;
        case 2:
            bench_print_round_trip(rows, NUM_ROWS);
            break;
        default:
            bench_print_results(rows, NUM_ROWS);
            break;
    }

    ui_footer(s);
}

int main(int argc, char **argv)
{
    consoleDemoInit();

    if (!proto_init(teak_tlf_bin))
    {
        while (1)
            swiWaitForVBlank();
    }

    math32_generate();

    ui_state s = { 0, 4, false };
    setCpuClock(s.fast_cpu);

    while (1)
    {
        consoleClear();
        printf("Running...\n");
        bench_run(rows, NUM_ROWS, checksum);
        print_page(&s);

        ui_action action = UI_NONE;
        while ((action != UI_RERUN) && (action != UI_EXIT))
        {
            swiWaitForVBlank();
            action = ui_poll_keys(&s);
            if (action == UI_REDRAW)
                print_page(&s);
        }

        if (action == UI_EXIT)
            break;
    }

    return 0;
}
