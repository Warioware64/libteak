// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 05: 8x8 integer DCT and IDCT of the 64 blocks of a 64x64 image.
//
// The DSP assembly version only replaces the 8-point transform (8 dot products
// of 8 values), called 16 times per block. The top screen shows the source
// image, the image after DCT + IDCT (from the DSP if the DMA transfer works)
// and the error multiplied by 2048.

#include <stdio.h>
#include <string.h>

#include <nds.h>

#include "dct.h"
#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "ui.h"

static void arm_dct_c(void)
{
    k_dct_image(DCT8_C);
}

static void arm_idct_c(void)
{
    k_idct_image(DCT8_C);
}

static void arm_nothing(void)
{
}

// The IDCT row uses the coefficients of the DCT row, so the order matters
static bench_row rows[] = {
    { "dct", JOB_DCT_C, arm_dct_c, NULL, JOB_DCT_C, JOB_DCT_ASM },
    { "idct", JOB_IDCT_C, arm_idct_c, NULL, JOB_IDCT_C, JOB_IDCT_ASM },
    // Whole image in DSP assembly (no C code per block)
    { "dct2", JOB_DCT_C, arm_dct_c, NULL, NO_JOB, JOB_DCT_IMAGE_ASM },
    { "idct2", JOB_IDCT_C, arm_idct_c, NULL, NO_JOB, JOB_IDCT_IMAGE_ASM },
    // Checks that both CPUs have generated the same inputs
    { "input", JOB_INPUTS, arm_nothing, NULL, JOB_INPUTS, NO_JOB },
};

#define NUM_ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static uint32_t checksum(uint16_t job)
{
    return dct_checksum(job);
}

ALIGN(32) static uint16_t dsp_recon[IMG_SIZE];
static bool dma_out_ok;

// Error of DCT + IDCT
static int max_error;
static int mean_error_x100;

static uint16_t *bg_gfx;

static void draw_image(const uint16_t *img, int x0, int y0, int gain)
{
    for (int y = 0; y < IMG_H; y++)
    {
        for (int x = 0; x < IMG_W; x++)
        {
            int g = (img[y * IMG_W + x] * gain) >> 7; // 12 bits -> 5 bits
            if (g > 31)
                g = 31;
            bg_gfx[(y0 + y) * 256 + x0 + x] = RGB15(g, g, g) | BIT(15);
        }
    }
}

static uint16_t error_img[IMG_SIZE];

static void run_all(void)
{
    bench_run(rows, NUM_ROWS, checksum);

    int sum = 0;
    max_error = 0;
    for (int i = 0; i < IMG_SIZE; i++)
    {
        int e = img_recon[i] - img_src[i];
        if (e < 0)
            e = -e;
        sum += e;
        if (e > max_error)
            max_error = e;
        error_img[i] = (uint16_t)e;
    }
    mean_error_x100 = (sum * 100) / IMG_SIZE;

    memset(dsp_recon, 0, sizeof(dsp_recon));
    uint16_t st = proto_send(BUFFER_RECON, dsp_recon, sizeof(dsp_recon));
    dma_out_ok = (st == PROTO_STATUS_OK)
              && (memcmp(dsp_recon, img_recon, sizeof(dsp_recon)) == 0);

    draw_image(img_src, 8, 64, 1);
    draw_image(dma_out_ok ? dsp_recon : img_recon, 96, 64, 1);
    draw_image(error_img, 184, 64, 2048);
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 05: DCT 8x8 x64", s, bench_errors(rows, NUM_ROWS));

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
        default:
            bench_print_results(rows, NUM_ROWS);
            printf("DCT + IDCT error: max %d,\n", max_error);
            printf("mean %d.%02d (of 4095)\n", mean_error_x100 / 100,
                   mean_error_x100 % 100);
            printf("DMA DSP->ARM9: %s\n", dma_out_ok ? "OK" : "FAIL");
            printf("Top: source, DCT+IDCT (%s),\n", dma_out_ok ? "DSP" : "ARM9");
            printf("error x2048\n");
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

    dct_generate();

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
