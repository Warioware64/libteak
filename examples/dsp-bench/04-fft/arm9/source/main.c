// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 04: 256-point complex FFT in Q15 (radix-2 decimation in time, the
// values are divided by 2 in every stage).
//
// The twiddle factors are computed by the ARM9 with floats and sent to the DSP
// once. Then both CPUs build the same input signal from them. The integer FFT
// is bit-exact on both CPUs; its precision is measured against a float FFT
// computed by the ARM9.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>

#include "fft.h"
#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "ui.h"

static void arm_setup(void)
{
    fft_setup();
}

static void arm_fft_c(void)
{
    k_fft(fft_in, fft_out);
}

static bench_row rows[] = {
    { "fft", JOB_FFT_C, arm_fft_c, NULL, JOB_FFT_C, JOB_FFT_ASM },
    // Whole FFT in DSP assembly
    { "fft2", JOB_FFT_C, arm_fft_c, NULL, NO_JOB, JOB_FFT2_ASM },
    // Builds the input from the twiddle factors sent by the ARM9, and checks
    // that both CPUs have the same input
    { "input", JOB_SETUP, arm_setup, NULL, JOB_SETUP, NO_JOB },
};

#define NUM_ROWS ((int)(sizeof(rows) / sizeof(rows[0])))

static uint32_t checksum(uint16_t job)
{
    return fft_checksum(job);
}

static void make_twiddles(void)
{
    for (int k = 0; k < FFT_N / 2; k++)
    {
        float a = 2.0f * (float)M_PI * k / FFT_N;
        twiddle[2 * k] = (int16_t)lroundf(cosf(a) * 32767.0f);
        twiddle[2 * k + 1] = (int16_t)lroundf(sinf(a) * 32767.0f);
    }
}

// Float FFT of fft_in, same algorithm, no scaling
static float ref_re[FFT_N], ref_im[FFT_N];

static void float_fft(void)
{
    for (int i = 0; i < FFT_N; i++)
    {
        int r = 0;
        for (int b = 0, v = i; b < 8; b++, v >>= 1)
            r = (r << 1) | (v & 1);
        ref_re[r] = fft_in[i].re;
        ref_im[r] = fft_in[i].im;
    }

    for (int half = 1; half < FFT_N; half <<= 1)
    {
        for (int j = 0; j < half; j++)
        {
            float a = (float)M_PI * j / half;
            float c = cosf(a), s = sinf(a);

            for (int k = j; k < FFT_N; k += 2 * half)
            {
                float br = ref_re[k + half], bi = ref_im[k + half];
                float tr = br * c + bi * s;
                float ti = bi * c - br * s;
                float ur = ref_re[k], ui = ref_im[k];
                ref_re[k] = ur + tr;
                ref_im[k] = ui + ti;
                ref_re[k + half] = ur - tr;
                ref_im[k + half] = ui - ti;
            }
        }
    }
}

// Precision of the integer FFT (fft_out) against the float FFT / N
static float snr_db;
static float max_error;

static void measure_precision(void)
{
    float_fft();

    float signal = 0.0f, noise = 0.0f;
    max_error = 0.0f;

    for (int i = 0; i < FFT_N; i++)
    {
        float rr = ref_re[i] / FFT_N, ri = ref_im[i] / FFT_N;
        float er = fft_out[i].re - rr, ei = fft_out[i].im - ri;

        signal += rr * rr + ri * ri;
        noise += er * er + ei * ei;

        float e = sqrtf(er * er + ei * ei);
        if (e > max_error)
            max_error = e;
    }

    snr_db = (noise > 0.0f) ? 10.0f * log10f(signal / noise) : 999.0f;
}

// Stage by stage comparison of the assembly butterflies (JOB_FFT_ASM) with
// the C version: they give different results on DSi hardware only. Needs the
// DMA transfer from the DSP, so it's only useful on hardware.
typedef struct {
    uint16_t status;
    int bad;            // Wrong values (re or im)
    int index;          // First wrong element
    bool im;            // The wrong value is the imaginary part
    int16_t got, expected;
} stage_diff;

static stage_diff stage_diffs[9];

ALIGN(32) static kcplx dsp_out[FFT_N];
static kcplx stage_ref[FFT_N];

static void check_stages(void)
{
    for (int st = 0; st <= 8; st++)
    {
        stage_diff *d = &stage_diffs[st];
        memset(d, 0, sizeof(*d));
        d->index = -1;

        proto_param(PARAM_STAGES, st);
        proto_run_result r;
        proto_run(JOB_FFT_ASM, 1, &r);
        memset(dsp_out, 0, sizeof(dsp_out));
        d->status = proto_send(0, dsp_out, sizeof(dsp_out));

        k_fft_stages(fft_in, stage_ref, st);
        for (int i = 0; i < FFT_N; i++)
        {
            int bad = (dsp_out[i].re != stage_ref[i].re)
                    + (dsp_out[i].im != stage_ref[i].im);
            if (bad && (d->index < 0))
            {
                d->index = i;
                d->im = (dsp_out[i].re == stage_ref[i].re);
                d->got = d->im ? dsp_out[i].im : dsp_out[i].re;
                d->expected = d->im ? stage_ref[i].im : stage_ref[i].re;
            }
            d->bad += bad;
        }
    }

    proto_param(PARAM_STAGES, 8);
}

static void print_stages(void)
{
    printf("fft asm vs C, stage by stage\n");
    printf("(after the bit reversed copy)\n");
    for (int st = 0; st <= 8; st++)
    {
        const stage_diff *d = &stage_diffs[st];
        printf("%d: ", st);
        if (d->status != PROTO_STATUS_OK)
            printf("DMA error\n");
        else if (d->bad == 0)
            printf("OK\n");
        else
            printf("%3d bad %3d%s %6d/%6d\n", d->bad, d->index,
                   d->im ? "i" : "r", d->got, d->expected);
    }
    printf("First bad element (r/i part):\n");
    printf("got/expected\n");
}

static void run_all(void)
{
    // The input must be ready before the FFT rows run
    fft_setup();
    proto_run_result r;
    proto_run(JOB_SETUP, 1, &r);

    bench_run(rows, NUM_ROWS, checksum);

    check_stages();

    // bench_run() leaves the ARM9 C results in fft_out
    k_fft(fft_in, fft_out);
    measure_precision();
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 04: FFT 256 Q15", s, bench_errors(rows, NUM_ROWS));

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
            print_stages();
            break;
        default:
            bench_print_results(rows, NUM_ROWS);
            printf("Integer FFT vs float FFT:\n");
            printf("SNR %d.%d dB, max error %d.%02d\n", (int)snr_db,
                   (int)(snr_db * 10) % 10, (int)max_error,
                   (int)(max_error * 100) % 100);
            printf("(same bits on ARM9 and DSP)\n");
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

    printf("Sending twiddle factors...\n");
    make_twiddles();
    if (proto_upload(TABLE_TWIDDLE, (const uint16_t *)twiddle, TWIDDLE_WORDS)
        != PROTO_STATUS_OK)
    {
        printf("Upload failed\n");
        while (1)
            swiWaitForVBlank();
    }

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
