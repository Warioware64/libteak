// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 07: 2D gradient (Perlin) noise on a 64x64 image, one octave and
// fBm of 4 octaves, in Q8 and in Q16.16 (32-bit).
//
// The permutation and gradient tables are created by the ARM9 and sent to the
// DSP once. The top screen shows the Q8 noise, the Q8 fBm (from the DSP if the
// DMA transfer works) and the Q16.16 fBm.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nds.h>

#include "perlin.h"
#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "ui.h"

static void arm_noise(void)
{
    k_noise(img_noise, 1);
}

static void arm_fbm(void)
{
    k_noise(img_fbm, 4);
}

static void arm_noise_q16(void)
{
    k_noise_q16(img_noise_q16, 1);
}

static void arm_fbm_q16(void)
{
    k_noise_q16(img_fbm_q16, 4);
}

static void arm_nothing(void)
{
}

static bench_row rows[] = {
    { "noise", JOB_NOISE_C, arm_noise, NULL, JOB_NOISE_C, JOB_NOISE_ASM },
    { "fbm4", JOB_FBM_C, arm_fbm, NULL, JOB_FBM_C, JOB_FBM_ASM },
    { "noiseQ", JOB_NOISE_Q16_C, arm_noise_q16, NULL, JOB_NOISE_Q16_C, JOB_NOISE_Q16_ASM },
    { "fbm4Q", JOB_FBM_Q16_C, arm_fbm_q16, NULL, JOB_FBM_Q16_C, JOB_FBM_Q16_ASM },
    // Checks that the DSP has received the same tables
    { "input", JOB_INPUTS, arm_nothing, NULL, JOB_INPUTS, NO_JOB },
};

#define NUM_ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static uint32_t checksum(uint16_t job)
{
    return perlin_checksum(job);
}

static void make_tables(void)
{
    srand(7);
    for (int i = 0; i < PERM_ENTRIES; i++)
        perm[i] = (uint16_t)i;
    for (int i = PERM_ENTRIES - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        uint16_t t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }

    for (int k = 0; k < GRAD_ENTRIES; k++)
    {
        float a = 2.0f * (float)M_PI * k / GRAD_ENTRIES;
        grad[2 * k] = (int16_t)lroundf(cosf(a) * 256.0f);
        grad[2 * k + 1] = (int16_t)lroundf(sinf(a) * 256.0f);
    }
}

ALIGN(32) static uint16_t dsp_fbm[IMG_SIZE];
static bool dma_out_ok;

// Largest difference between the Q8 and Q16.16 fBm, in gray levels (of 4095)
static int max_diff;

static uint16_t *bg_gfx;

static void draw_image(const uint16_t *img, int x0, int y0)
{
    for (int y = 0; y < IMG_H; y++)
    {
        for (int x = 0; x < IMG_W; x++)
        {
            uint16_t g = img[y * IMG_W + x] >> 7; // 12 bits -> 5 bits
            bg_gfx[(y0 + y) * 256 + x0 + x] = RGB15(g, g, g) | BIT(15);
        }
    }
}

static void run_all(void)
{
    bench_run(rows, NUM_ROWS, checksum);

    max_diff = 0;
    for (int i = 0; i < IMG_SIZE; i++)
    {
        int d = img_fbm[i] - img_fbm_q16[i];
        if (d < 0)
            d = -d;
        if (d > max_diff)
            max_diff = d;
    }

    memset(dsp_fbm, 0, sizeof(dsp_fbm));
    uint16_t st = proto_send(BUFFER_FBM, dsp_fbm, sizeof(dsp_fbm));
    dma_out_ok = (st == PROTO_STATUS_OK)
              && (memcmp(dsp_fbm, img_fbm, sizeof(dsp_fbm)) == 0);

    draw_image(img_noise, 8, 64);
    draw_image(dma_out_ok ? dsp_fbm : img_fbm, 96, 64);
    draw_image(img_fbm_q16, 184, 64);
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 07: Perlin 64x64", s, bench_errors(rows, NUM_ROWS));

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
            printf("fBm Q8 vs Q16.16: max diff %d\n", max_diff);
            printf("DMA DSP->ARM9: %s\n", dma_out_ok ? "OK" : "FAIL");
            printf("Top: noise, fBm (%s),\n", dma_out_ok ? "DSP" : "ARM9");
            printf("fBm Q16.16\n");
            break;
    }

    ui_footer(s);
}

int main(int argc, char **argv)
{
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bg_gfx = bgGetGfxPtr(bg);
    memset(bg_gfx, 0, 256 * 192 * 2);

    consoleDemoInit();

    if (!proto_init(teak_tlf_bin))
    {
        while (1)
            swiWaitForVBlank();
    }

    printf("Sending tables...\n");
    make_tables();
    if ((proto_upload(TABLE_PERM, perm, PERM_ENTRIES) != PROTO_STATUS_OK) ||
        (proto_upload(TABLE_GRAD, (const uint16_t *)grad, GRAD_ENTRIES * 2)
         != PROTO_STATUS_OK))
    {
        printf("Upload failed\n");
        while (1)
            swiWaitForVBlank();
    }

    ui_state s = { 0, 4, false };
    setCpuClock(s.fast_cpu);

    while (1)
    {
        consoleClear();
        printf("Running (it takes a while)...\n");
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
