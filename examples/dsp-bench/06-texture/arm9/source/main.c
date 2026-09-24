// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 06: animated 64x64 RGB555 plasma texture.
//
// Every frame, the ARM9 and the DSP (C and assembly versions) compute the
// same texture. The ARM9 copies
// its texture to the left of the top screen. The DSP writes its texture
// directly into BG VRAM (right of the top screen) with DMA, through the AHBM
// bus, without any work from the ARM9.
//
// While the DSP works, the ARM9 is free: it only polls for the reply. The
// screen shows how much of the frame time is free for the ARM9 when the DSP
// computes the texture (assembly version). Note: melonDS doesn't emulate the DMA transfers, so
// the right texture stays black there.

#include <stdio.h>
#include <math.h>
#include <string.h>

#include <nds.h>

#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "texture.h"
#include "ui.h"

// Free-running clock with timers 2 and 3 (timers 0 and 1 are used by the
// protocol functions), in ARM9 timer ticks (33.51 MHz)
static void clock_start(void)
{
    TIMER_CR(2) = 0;
    TIMER_CR(3) = 0;
    TIMER_DATA(2) = 0;
    TIMER_DATA(3) = 0;
    TIMER_CR(3) = TIMER_ENABLE | TIMER_CASCADE;
    TIMER_CR(2) = TIMER_ENABLE | TIMER_DIV_1;
}

static uint32_t clock_now(void)
{
    uint16_t hi, lo;
    do
    {
        hi = TIMER_DATA(3);
        lo = TIMER_DATA(2);
    } while (hi != TIMER_DATA(3));
    return ((uint32_t)hi << 16) | lo;
}

static void make_sin_table(void)
{
    for (int i = 0; i < SIN_ENTRIES; i++)
        sin_table[i] = (int16_t)lroundf(sinf(2.0f * (float)M_PI * i / 256.0f) * 32767.0f);
}

// Averages over the last frames
#define AVG_FRAMES  32

typedef struct {
    uint32_t arm_gen;       // ARM9 computes the texture
    uint32_t dsp_gen;       // DSP C computes the texture (DSP cycles)
    uint32_t dsp_asm_gen;   // DSP assembly computes the texture (DSP cycles)
    uint32_t dsp_total;     // Parameter + computation + DMA to VRAM
    uint32_t arm_free;      // Part of dsp_total when the ARM9 only waits
} frame_times;

static frame_times sum, avg;
static uint32_t frames, frames_ok, frames_total;
static uint32_t frames_asm_ok;

static uint16_t *bg_gfx;

static void copy_to_screen(const uint16_t *tex, int x0, int y0)
{
    for (int y = 0; y < TEX_H; y++)
        memcpy(&bg_gfx[(y0 + y) * VRAM_BITMAP_W + x0], &tex[y * TEX_W], TEX_W * 2);
}

static void print_stats(bool fast_cpu)
{
    consoleClear();

    printf("DSP bench 06: plasma 64x64\n");
    printf("ARM9 %s MHz, DSP 134 MHz\n", fast_cpu ? "134" : "67");
    if ((frames_ok == frames_total) && (frames_asm_ok == frames_total))
        printf("Results: all OK\n");
    else
        printf("Results: %lu FAIL\n",
               2 * frames_total - frames_ok - frames_asm_ok);
    printf("--------------------------------");

    printf("Per frame, average (us)\n\n");
    printf("ARM9 computes texture %8lu\n", arm9_ticks_to_us(avg.arm_gen));
    printf("DSP C computes it     %8lu\n", dsp_cycles_to_us(avg.dsp_gen));
    printf("DSP asm computes it   %8lu\n", dsp_cycles_to_us(avg.dsp_asm_gen));
    printf("DSP asm + DMA to VRAM %8lu\n", arm9_ticks_to_us(avg.dsp_total));
    printf("  ARM9 free meanwhile %8lu\n", arm9_ticks_to_us(avg.arm_free));
    printf("  ARM9 busy (commands)%8lu\n",
           arm9_ticks_to_us(avg.dsp_total - avg.arm_free));
    printf("\n");
    printf("Frames C:   %lu/%lu OK\n", frames_ok, frames_total);
    printf("Frames asm: %lu/%lu OK\n", frames_asm_ok, frames_total);
    ui_print_glitches();
    printf("\nTop: left ARM9, right DSP\n");
    printf("(DSP DMA not in melonDS)\n");

    printf("\x1b[23;0H");
    printf("B:ARM9 clock START:exit");
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

    printf("Sending sine table...\n");
    make_sin_table();
    if (proto_upload(TABLE_SIN, (const uint16_t *)sin_table, SIN_ENTRIES)
        != PROTO_STATUS_OK)
    {
        printf("Upload failed\n");
        while (1)
            swiWaitForVBlank();
    }

    clock_start();

    ui_state s = { 0, 1, false };
    setCpuClock(s.fast_cpu);

    uint16_t *dsp_dst = &bg_gfx[64 * VRAM_BITMAP_W + 160];

    for (uint16_t t = 0; ; t++)
    {
        // ARM9 version
        uint32_t t0 = clock_now();
        k_plasma(texture, t);
        uint32_t t1 = clock_now();
        uint32_t arm_checksum = texture_checksum(JOB_PLASMA_C);
        copy_to_screen(texture, 32, 64);

        // DSP C version (only measured and checked)
        proto_param(PARAM_FRAME, t);
        proto_run_result rc;
        proto_run(JOB_PLASMA_C, 1, &rc);

        // DSP assembly version, sent to VRAM
        uint32_t t2 = clock_now();
        proto_param(PARAM_FRAME, t);
        proto_run_start(JOB_PLASMA_ASM, 1);
        uint32_t t3 = clock_now();
        proto_run_result r;
        while (!proto_run_poll(&r));    // The ARM9 could do other work here
        uint32_t t4 = clock_now();
        proto_send(BUFFER_TEXTURE_VRAM, dsp_dst,
                   (TEX_H - 1) * VRAM_BITMAP_W * 2 + TEX_W * 2);
        uint32_t t5 = clock_now();

        frames_total++;
        if ((rc.status == PROTO_STATUS_OK) && (rc.checksum == arm_checksum))
            frames_ok++;
        if ((r.status == PROTO_STATUS_OK) && (r.checksum == arm_checksum))
            frames_asm_ok++;

        sum.arm_gen += t1 - t0;
        sum.dsp_gen += rc.cycles;
        sum.dsp_asm_gen += r.cycles;
        sum.dsp_total += t5 - t2;
        sum.arm_free += t4 - t3;
        frames++;

        if (frames == AVG_FRAMES)
        {
            avg.arm_gen = sum.arm_gen / AVG_FRAMES;
            avg.dsp_gen = sum.dsp_gen / AVG_FRAMES;
            avg.dsp_asm_gen = sum.dsp_asm_gen / AVG_FRAMES;
            avg.dsp_total = sum.dsp_total / AVG_FRAMES;
            avg.arm_free = sum.arm_free / AVG_FRAMES;
            memset(&sum, 0, sizeof(sum));
            frames = 0;
            print_stats(s.fast_cpu);
        }

        swiWaitForVBlank();

        ui_action action = ui_poll_keys(&s);
        if (action == UI_EXIT)
            break;
    }

    return 0;
}
