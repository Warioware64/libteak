// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 03: blur of a 64x64 image of 12-bit luminance.
//
// - Separable Gaussian [1 4 6 4 1] / 16 (shifts and additions only).
// - Generic 3x3 convolution (the coefficients and the shift are data).
//
// The top screen shows the source image, the Gaussian and the 3x3 result. They
// are sent back by the DSP with DMA and compared with the ARM9 results. If the
// transfer doesn't work (melonDS doesn't emulate it), the ARM9 results are
// shown instead. The conversion to RGB for the display isn't timed.
//
// The last page compares the results of the DSP assembly Gaussians with the
// ARM9 ones, pass by pass, to find hardware differences.

#include <stdio.h>
#include <string.h>

#include <nds.h>

#include "blur.h"
#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "ui.h"

static void arm_gauss_c(void)
{
    k_gauss5(img_src, img_tmp, img_gauss);
}

static void arm_conv_c(void)
{
    k_conv3x3(img_src, img_conv, &conv_params);
}

static void arm_nothing(void)
{
}

static bench_row rows[] = {
    { "gauss", JOB_GAUSS_C, arm_gauss_c, NULL, JOB_GAUSS_C, JOB_GAUSS_ASM },
    // Same Gaussian in DSP assembly with other instruction forms
    { "gaussB", JOB_GAUSS_C, arm_gauss_c, NULL, NO_JOB, JOB_GAUSS_B_ASM },
    { "conv3", JOB_CONV_C, arm_conv_c, NULL, JOB_CONV_C, JOB_CONV_ASM },
    // Checks that both CPUs have generated the same inputs
    { "input", JOB_INPUTS, arm_nothing, NULL, JOB_INPUTS, NO_JOB },
};

#define NUM_ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static uint32_t checksum(uint16_t job)
{
    return blur_checksum(job);
}

// Outputs sent by the DSP with DMA (32-byte aligned, see proto_send())
ALIGN(32) static uint16_t dsp_gauss[IMG_SIZE];
ALIGN(32) static uint16_t dsp_conv[IMG_SIZE];

static bool dma_out_ok;

// Comparison of the results of a DSP assembly Gaussian with the ARM9 ones,
// pass by pass (the DSP sends img_tmp and img_gauss after the job)
#define DIFF_SHOWN  2

typedef struct {
    int count;              // Wrong pixels
    int edge;               // Wrong pixels with x or y in {0, 1, 62, 63}
    int x[DIFF_SHOWN], y[DIFF_SHOWN];
    uint16_t got[DIFF_SHOWN], expected[DIFF_SHOWN];
} pass_diff;

typedef struct {
    bool dma_ok;
    pass_diff h, v;
} gauss_diff;

static gauss_diff diff_a, diff_b;

ALIGN(32) static uint16_t dsp_tmp[IMG_SIZE];

static bool is_edge(int c)
{
    return (c < 2) || (c > IMG_W - 3);
}

static void compare_pass(pass_diff *d, const uint16_t *got,
                         const uint16_t *expected)
{
    memset(d, 0, sizeof(*d));
    for (int i = 0; i < IMG_SIZE; i++)
    {
        if (got[i] == expected[i])
            continue;

        int x = i % IMG_W, y = i / IMG_W;
        if (d->count < DIFF_SHOWN)
        {
            d->x[d->count] = x;
            d->y[d->count] = y;
            d->got[d->count] = got[i];
            d->expected[d->count] = expected[i];
        }
        d->count++;
        if (is_edge(x) || is_edge(y))
            d->edge++;
    }
}

static void check_gauss_asm(uint16_t job, gauss_diff *d)
{
    proto_run_result r;
    proto_run(job, 1, &r);

    memset(dsp_tmp, 0, sizeof(dsp_tmp));
    memset(dsp_gauss, 0, sizeof(dsp_gauss));
    uint16_t st1 = proto_send(BUFFER_TMP, dsp_tmp, sizeof(dsp_tmp));
    uint16_t st2 = proto_send(BUFFER_GAUSS, dsp_gauss, sizeof(dsp_gauss));
    d->dma_ok = (st1 == PROTO_STATUS_OK) && (st2 == PROTO_STATUS_OK);

    compare_pass(&d->h, dsp_tmp, img_tmp);
    compare_pass(&d->v, dsp_gauss, img_gauss);
}

static void print_pass(const char *name, const pass_diff *d)
{
    printf(" %s: %4d bad, %4d at edges\n", name, d->count, d->edge);
    for (int i = 0; (i < d->count) && (i < DIFF_SHOWN); i++)
    {
        printf("  x%2d y%2d got %4u exp %4u\n", d->x[i], d->y[i],
               d->got[i], d->expected[i]);
    }
}

static void print_gauss_diff(const char *name, const gauss_diff *d)
{
    printf("%s (DMA %s)\n", name, d->dma_ok ? "OK" : "FAIL");
    print_pass("H", &d->h);
    print_pass("V", &d->v);
}

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

    // Pass by pass comparison of both assembly Gaussians (it needs the DMA
    // transfers, so it's only useful on hardware). img_tmp and img_gauss of
    // the ARM9 are the ones of the ARM9 C version.
    check_gauss_asm(JOB_GAUSS_ASM, &diff_a);
    check_gauss_asm(JOB_GAUSS_B_ASM, &diff_b);

    // Leave the result of the C version in the DSP for the display. The last
    // DSP jobs that wrote img_conv were the C and asm versions of the 3x3
    // convolution: both give the same image.
    proto_run_result r;
    proto_run(JOB_GAUSS_C, 1, &r);

    memset(dsp_gauss, 0, sizeof(dsp_gauss));
    memset(dsp_conv, 0, sizeof(dsp_conv));

    uint16_t st1 = proto_send(BUFFER_GAUSS, dsp_gauss, sizeof(dsp_gauss));
    uint16_t st2 = proto_send(BUFFER_CONV, dsp_conv, sizeof(dsp_conv));

    dma_out_ok = (st1 == PROTO_STATUS_OK) && (st2 == PROTO_STATUS_OK)
              && (memcmp(dsp_gauss, img_gauss, sizeof(dsp_gauss)) == 0)
              && (memcmp(dsp_conv, img_conv, sizeof(dsp_conv)) == 0);

    draw_image(img_src, 8, 64);
    draw_image(dma_out_ok ? dsp_gauss : img_gauss, 96, 64);
    draw_image(dma_out_ok ? dsp_conv : img_conv, 184, 64);
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 03: blur 64x64", s, bench_errors(rows, NUM_ROWS));

    switch (s->page)
    {
        case 0:
            bench_print_times(rows, NUM_ROWS);
            break;
        case 1:
            bench_print_speedup(rows, NUM_ROWS);
            break;
        case 2:
            bench_print_round_trip(rows, NUM_ROWS);
            break;
        case 4:
            printf("DSP asm Gaussian vs ARM9, H/V\n");
            print_gauss_diff("gauss", &diff_a);
            print_gauss_diff("gaussB", &diff_b);
            break;
        default:
            bench_print_results(rows, NUM_ROWS);
            printf("DMA DSP->ARM9: %s\n", dma_out_ok ? "OK" : "FAIL");
            printf("Top screen: source, Gaussian,\n");
            printf("3x3 sharpen (from the %s)\n", dma_out_ok ? "DSP" : "ARM9");
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

    blur_generate();

    ui_state s = { 0, 5, false };
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
