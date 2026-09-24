// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 02: skinning of 256 vertices with 2 weighted bones each (out of
// 16 bones, 4x3 matrices), in Q3.12 and in Q16.16 (32-bit).
//
// Variants: ARM9 C (Thumb), DSP C and DSP assembly for Q3.12, ARM9 C and DSP C
// for Q16.16. Every variant is checked against the ARM9 C version.

#include <stdio.h>

#include <nds.h>

#include "proto_arm9.h"
#include "skinning.h"
#include "teak_tlf_bin.h"
#include "ui.h"

static void arm_skin_c(void)
{
    k_skin(verts, bones, out_verts, N_VERTS);
}

static void arm_skin_q16_c(void)
{
    k_skin_q16(verts_q16, bones_q16, out_verts_q16, N_VERTS);
}

static void arm_nothing(void)
{
}

static bench_row rows[] = {
    { "skin", JOB_SKIN_C, arm_skin_c, NULL, JOB_SKIN_C, JOB_SKIN_ASM },
    // Faster DSP assembly version (vertex in registers, no stored rows)
    { "skin2", JOB_SKIN_C, arm_skin_c, NULL, NO_JOB, JOB_SKIN2_ASM },
    { "skinQ", JOB_SKIN_Q16_C, arm_skin_q16_c, NULL, JOB_SKIN_Q16_C, JOB_SKIN_Q16_ASM },
    // Checks that both CPUs have generated the same inputs
    { "input", JOB_INPUTS, arm_nothing, NULL, JOB_INPUTS, NO_JOB },
};

#define NUM_ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static uint32_t checksum(uint16_t job)
{
    return skinning_checksum(job);
}

// Largest difference between the Q3.12 and Q16.16 results, in 1/65536 units
static int32_t max_diff_q16;

static int32_t abs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static void run_all(void)
{
    bench_run(rows, NUM_ROWS, checksum);

    max_diff_q16 = 0;
    for (int i = 0; i < N_VERTS; i++)
    {
        int32_t d;

        d = abs32(((int32_t)out_verts[i].x << 4) - out_verts_q16[i].x);
        if (d > max_diff_q16)
            max_diff_q16 = d;
        d = abs32(((int32_t)out_verts[i].y << 4) - out_verts_q16[i].y);
        if (d > max_diff_q16)
            max_diff_q16 = d;
        d = abs32(((int32_t)out_verts[i].z << 4) - out_verts_q16[i].z);
        if (d > max_diff_q16)
            max_diff_q16 = d;
    }
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 02: skinning x256", s, bench_errors(rows, NUM_ROWS));

    switch (s->page)
    {
        case 0:
            bench_print_times(rows, NUM_ROWS);
            printf("Q = Q16.16 (32-bit)\n");
            break;
        case 1:
            bench_print_speedup(rows, NUM_ROWS);
            break;
        case 2:
            bench_print_round_trip(rows, NUM_ROWS);
            break;
        default:
            bench_print_results(rows, NUM_ROWS);
            printf("Q3.12 vs Q16.16, largest\n");
            printf("difference: %ld / 65536\n", max_diff_q16);
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

    skinning_generate();

    ui_state s = { 0, 4, false };
    setCpuClock(s.fast_cpu);

    while (1)
    {
        consoleClear();
        printf("Running...\n");
        run_all();
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
