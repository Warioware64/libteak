// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 01: collision kernels on 512 pairs of objects.
//
// - dot3 and cross3 of Q3.12 vectors.
// - Sphere vs sphere and AABB vs AABB tests (results as bitmasks).
// - dot3 and sphere vs sphere again in Q16.16 (32-bit).
//
// ARM9 variants: C (Thumb) and hand-written ARM assembly (dsp16.s, which uses
// the SMULxy/SMLAxy instructions). DSP variants: C and hand-written assembly.
// Every variant is checked against the ARM9 C version.

#include <stdio.h>

#include <nds.h>

#include "collision.h"
#include "math_f16.h"
#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "ui.h"

// ARM9 kernels

static void arm_dot3_c(void)
{
    k_dot3(vec_a, vec_b, out_dot, N_PAIRS);
}

static void arm_dot3_asm(void)
{
    dsp16_dot3_array((const vec3_f16 *)vec_a, (const vec3_f16 *)vec_b,
                     out_dot, N_PAIRS);
}

static void arm_cross3_c(void)
{
    k_cross3(vec_a, vec_b, out_cross, N_PAIRS);
}

static void arm_cross3_asm(void)
{
    dsp16_cross_array((const vec3_f16 *)vec_a, (const vec3_f16 *)vec_b,
                      out_cross, N_PAIRS);
}

static void arm_sphere_c(void)
{
    k_sphere(sph_a, sph_b, out_sphere, N_PAIRS);
}

static void arm_aabb_c(void)
{
    k_aabb(sph_a, sph_b, out_aabb, N_PAIRS);
}

static void arm_dot3_q16_c(void)
{
    k_dot3_q16(sph_a_q16, sph_b_q16, out_dot_q16, N_PAIRS);
}

static void arm_nothing(void)
{
}

static void arm_sphere_q16_c(void)
{
    k_sphere_q16(sph_a_q16, sph_b_q16, out_sphere_q16, N_PAIRS);
}

// Rows of the tables

static bench_row rows[] = {
    { "dot3", JOB_DOT3_C, arm_dot3_c, arm_dot3_asm, JOB_DOT3_C, JOB_DOT3_ASM },
    { "cross3", JOB_CROSS3_C, arm_cross3_c, arm_cross3_asm, JOB_CROSS3_C, JOB_CROSS3_ASM },
    { "sphere", JOB_SPHERE_C, arm_sphere_c, NULL, JOB_SPHERE_C, JOB_SPHERE_ASM },
    { "aabb", JOB_AABB_C, arm_aabb_c, NULL, JOB_AABB_C, JOB_AABB_ASM },
    { "dot3Q", JOB_DOT3_Q16_C, arm_dot3_q16_c, NULL, JOB_DOT3_Q16_C, JOB_DOT3_Q16_ASM },
    { "sphQ", JOB_SPHERE_Q16_C, arm_sphere_q16_c, NULL, JOB_SPHERE_Q16_C, JOB_SPHERE_Q16_ASM },
    // Checks that both CPUs have generated the same inputs
    { "input", JOB_INPUTS, arm_nothing, NULL, JOB_INPUTS, NO_JOB },
};

#define NUM_ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static uint32_t checksum(uint16_t job)
{
    return collision_checksum(job);
}

// Hits of the Q3.12 and Q16.16 sphere tests, and how many differ
static int sphere_hits_q12;
static int sphere_hits_q16;
static int sphere_hits_diff;

static int count_bits(uint16_t v)
{
    int n = 0;
    while (v)
    {
        n += v & 1;
        v >>= 1;
    }
    return n;
}

static void run_all(void)
{
    bench_run(rows, NUM_ROWS, checksum);

    // Both computed by the ARM9 C versions above
    sphere_hits_q12 = 0;
    sphere_hits_q16 = 0;
    sphere_hits_diff = 0;
    for (int i = 0; i < MASK_WORDS; i++)
    {
        sphere_hits_q12 += count_bits(out_sphere[i]);
        sphere_hits_q16 += count_bits(out_sphere_q16[i]);
        sphere_hits_diff += count_bits(out_sphere[i] ^ out_sphere_q16[i]);
    }
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 01: collision x512", s, bench_errors(rows, NUM_ROWS));

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
            printf("Sphere hits: Q3.12 %d, Q16 %d\n", sphere_hits_q12,
                   sphere_hits_q16);
            printf("Q3.12 vs Q16 differ: %d/%d\n", sphere_hits_diff, N_PAIRS);
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

    collision_generate();

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
