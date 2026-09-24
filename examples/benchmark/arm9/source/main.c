// SPDX-License-Identifier: CC0-1.0
//
// ARM9 vs DSP benchmark.
//
// The same kernels run on the ARM9 (C) and on the DSP (C built with the Teak
// LLVM toolchain, and hand-written assembly). The DSP results are checked
// against the ARM9 results, and the time of BENCH_REPEAT calls is shown:
//
// - ARM9 time is measured with the ARM9 timers.
// - DSP time is measured by the DSP itself with its own timer, in DSP cycles.
// - "Round trip" is the time measured by the ARM9 from sending the command to
//   receiving the result, so it includes the communication overhead.

#include <stdio.h>

#include <nds.h>

#include "bench.h"
#include "teak_tlf_bin.h"

// "a" is the first BENCH_N words, "b" the next BENCH_N words. See bench.h for
// the alignment requirements.
ALIGN(4096) static int16_t buffers[BENCH_N * 2];

#define buffer_a    (&buffers[0])
#define buffer_b    (&buffers[BENCH_N])

static const char *kernel_names[BENCH_KERNEL_COUNT] = {
    "dot16", "sum16", "xorsh"
};

typedef struct {
    uint32_t arm9_result;
    uint32_t arm9_us;
    uint32_t dsp_result[BENCH_VARIANT_COUNT];
    uint32_t dsp_us[BENCH_VARIANT_COUNT];
    uint32_t round_trip_us[BENCH_VARIANT_COUNT];
    uint16_t dsp_status[BENCH_VARIANT_COUNT];
} bench_row;

static bench_row rows[BENCH_KERNEL_COUNT];

static void wait_forever(void)
{
    while (1)
        swiWaitForVBlank();
}

static void fill_buffers(void)
{
    bench_fill(buffer_a, BENCH_N, BENCH_SEED_A);
    bench_fill(buffer_b, BENCH_N, BENCH_SEED_B);

    // The DSP reads this copy with DMA
    DC_FlushRange(buffers, sizeof(buffers));
}

// Sequence number of the last command, already shifted (see bench.h)
static uint16_t cmd_seq;

// Replies received with the wrong sequence number (or the wrong part of the
// reply), and commands the DSP ignored because it saw them twice.
static uint32_t stale_replies;
static uint32_t repeated_cmds;

static void dsp_send(uint16_t cmd, uint32_t arg)
{
    static uint16_t seq = 0;

    seq = (seq % 15) + 1;
    cmd_seq = seq << BENCH_SEQ_SHIFT;

    // The argument is written first: the DSP reads it when it sees CMD0
    dspSendData(1, arg >> 16);
    dspSendData(2, arg & 0xFFFF);
    dspSendData(0, cmd | cmd_seq);
}

// Waits for the reply of the last command. "part" is 0 for the first reply and
// BENCH_REPLY_SECOND for the second one. It returns the status.
static uint16_t dsp_receive(uint16_t part, uint32_t *value)
{
    while (1)
    {
        uint16_t rep0 = dspReceiveData(0);
        uint32_t v = (uint32_t)dspReceiveData(1) << 16;
        v |= dspReceiveData(2);

        if ((rep0 & (BENCH_SEQ_MASK | BENCH_REPLY_SECOND)) == (cmd_seq | part))
        {
            *value = v;
            return rep0 & BENCH_STATUS_MASK;
        }

        stale_replies++;
    }
}

static uint32_t arm9_ticks_to_us(uint32_t ticks)
{
    return ((uint64_t)ticks * 1000000) / BUS_CLOCK;
}

static uint32_t dsp_cycles_to_us(uint32_t cycles)
{
    return ((uint64_t)cycles * 1000000) / BENCH_DSP_CLOCK;
}

static uint32_t arm9_run_kernel(bench_kernel kernel)
{
    const uint16_t *buffer_u = (const uint16_t *)buffer_a;

    switch (kernel)
    {
        case BENCH_KERNEL_DOT16:
            return bench_dot16_c(buffer_a, buffer_b, BENCH_N);
        case BENCH_KERNEL_SUM16:
            return bench_sum16_c(buffer_u, BENCH_N);
        default:
            return bench_xorshift16_c(BENCH_SEED, BENCH_N);
    }
}

static void run_benchmark(void)
{
    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
    {
        bench_row *row = &rows[k];

        uint32_t result = 0;

        cpuStartTiming(0);
        for (int i = 0; i < BENCH_REPEAT; i++)
            result = arm9_run_kernel(k);
        row->arm9_us = arm9_ticks_to_us(cpuEndTiming());
        row->arm9_result = result;

        for (int v = 0; v < BENCH_VARIANT_COUNT; v++)
        {
            uint16_t cmd = BENCH_CMD_RUN | (k << BENCH_KERNEL_SHIFT) | v;
            uint32_t cycles;

            cpuStartTiming(0);
            dsp_send(cmd, BENCH_REPEAT);
            row->dsp_status[v] = dsp_receive(0, &row->dsp_result[v]);
            row->round_trip_us[v] = arm9_ticks_to_us(cpuEndTiming());

            dsp_receive(BENCH_REPLY_SECOND, &cycles);
            row->dsp_us[v] = dsp_cycles_to_us(cycles);
        }
    }

    dsp_send(BENCH_CMD_STATS, 0);
    dsp_receive(0, &repeated_cmds);
}

// The console is 32x24 characters. Tables are 30 characters wide.

static void print_speedup(uint32_t arm9_us, uint32_t dsp_us)
{
    if (dsp_us == 0)
    {
        printf("%8s", "-");
        return;
    }

    uint32_t x100 = (arm9_us * 100) / dsp_us;
    printf(" %3lu.%02lux", x100 / 100, x100 % 100);
}

static const char *dma_status_str(uint16_t status)
{
    switch (status)
    {
        case BENCH_STATUS_OK:
            return "OK";
        case BENCH_STATUS_DMA_ERROR:
            return "ERROR";
        default:
            return "FAIL";
    }
}

static bool result_ok(const bench_row *row, int variant)
{
    return (row->dsp_status[variant] == BENCH_STATUS_OK) &&
           (row->dsp_result[variant] == row->arm9_result);
}

static int count_errors(void)
{
    int errors = 0;

    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
    {
        for (int v = 0; v < BENCH_VARIANT_COUNT; v++)
        {
            if (!result_ok(&rows[k], v))
                errors++;
        }
    }

    return errors;
}

// The results are split in pages because they don't fit in one screen

typedef enum {
    PAGE_TIME,
    PAGE_SPEEDUP,
    PAGE_ROUND_TRIP,
    PAGE_CHECK,
    PAGE_COUNT
} results_page;

static void print_page_time(void)
{
    printf("Time of %d calls (us)\n\n", BENCH_REPEAT);

    printf("%-6s%8s%8s%8s\n", "", "ARM9", "DSP-C", "DSP-asm");
    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
    {
        bench_row *row = &rows[k];
        printf("%-6s%8lu%8lu%8lu\n", kernel_names[k], row->arm9_us,
               row->dsp_us[BENCH_VARIANT_C], row->dsp_us[BENCH_VARIANT_ASM]);
    }

    printf("\nARM9: measured by the ARM9\n");
    printf("DSP: cycles counted by the DSP\n");
}

static void print_page_speedup(void)
{
    printf("Speedup of the DSP over ARM9\n\n");

    printf("%-14s%8s%8s\n", "", "DSP-C", "DSP-asm");
    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
    {
        bench_row *row = &rows[k];
        printf("%-14s", kernel_names[k]);
        print_speedup(row->arm9_us, row->dsp_us[BENCH_VARIANT_C]);
        print_speedup(row->arm9_us, row->dsp_us[BENCH_VARIANT_ASM]);
        printf("\n");
    }

    printf("\nAbove 1.00x the DSP is faster\n");
}

static void print_page_round_trip(void)
{
    printf("Round trip seen by ARM9 (us)\n\n");

    printf("%-14s%8s%8s\n", "", "DSP-C", "DSP-asm");
    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
    {
        bench_row *row = &rows[k];
        printf("%-14s%8lu%8lu\n", kernel_names[k],
               row->round_trip_us[BENCH_VARIANT_C],
               row->round_trip_us[BENCH_VARIANT_ASM]);
    }

    printf("\nFrom sending the command to\n");
    printf("receiving the result\n");
}

static void print_page_check(uint16_t dma_status, uint32_t dma_mismatches)
{
    printf("DSP results vs ARM9 results\n\n");

    printf("%-14s%8s%8s\n", "", "DSP-C", "DSP-asm");
    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
    {
        bench_row *row = &rows[k];
        printf("%-14s", kernel_names[k]);
        for (int v = 0; v < BENCH_VARIANT_COUNT; v++)
            printf("%8s", result_ok(row, v) ? "OK" : "FAIL");
        printf("\n");
    }

    // Results in hexadecimal: one row per CPU, one column per kernel
    printf("\n%-5s", "");
    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
        printf("%-9s", kernel_names[k]);
    printf("\n");

    printf("%-5s", "ARM9");
    for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
        printf(k == 0 ? "%08lX" : " %08lX", rows[k].arm9_result);
    printf("\n");

    static const char *variant_names[BENCH_VARIANT_COUNT] = { "C", "asm" };
    for (int v = 0; v < BENCH_VARIANT_COUNT; v++)
    {
        printf("%-5s", variant_names[v]);
        for (int k = 0; k < BENCH_KERNEL_COUNT; k++)
            printf(k == 0 ? "%08lX" : " %08lX", rows[k].dsp_result[v]);
        printf("\n");
    }

    printf("\nDMA ARM9->DSP: %s", dma_status_str(dma_status));
    if (dma_status == BENCH_STATUS_DMA_MISMATCH)
        printf(" (%lu bad)", dma_mismatches);
    printf("\n");
    printf("(not emulated by melonDS)\n");

    // Communication glitches, recovered with the sequence numbers
    printf("Ignored: %lu replies, %lu cmds\n", stale_replies, repeated_cmds);
}

static void print_results(results_page page, bool fast_cpu,
                          uint16_t dma_status, uint32_t dma_mismatches)
{
    consoleClear();

    // Header, shown in all pages
    printf("ARM9 vs DSP  N=%d x%d\n", BENCH_N, BENCH_REPEAT);
    printf("ARM9 %s MHz, DSP 134 MHz\n", fast_cpu ? "134" : "67");

    int errors = count_errors();
    if (errors == 0)
        printf("Results: all OK\n");
    else
        printf("Results: %d FAIL (page %d)\n", errors, PAGE_CHECK + 1);

    printf("--------------------------------");

    switch (page)
    {
        case PAGE_TIME:
            print_page_time();
            break;
        case PAGE_SPEEDUP:
            print_page_speedup();
            break;
        case PAGE_ROUND_TRIP:
            print_page_round_trip();
            break;
        default:
            print_page_check(dma_status, dma_mismatches);
            break;
    }

    // Footer, at the bottom of the screen
    printf("\x1b[22;0H");
    printf("Left/Right: page %d/%d\n", page + 1, PAGE_COUNT);
    printf("A:rerun B:ARM9 clock START:exit");
}

int main(int argc, char **argv)
{
    consoleDemoInit();

    if (!isDSiMode())
    {
        printf("DSP only available on DSi");
        wait_forever();
    }

    if (dspExecuteDefaultTLF(teak_tlf_bin) != DSP_EXEC_OK)
    {
        printf("Failed to execute TLF");
        wait_forever();
    }

    fill_buffers();

    // The kernels don't depend on the result of the DMA check
    uint32_t dma_mismatches;
    dsp_send(BENCH_CMD_LOAD, (uintptr_t)buffers);
    uint16_t dma_status = dsp_receive(0, &dma_mismatches);

    bool fast_cpu = false;
    setCpuClock(fast_cpu);

    results_page page = PAGE_TIME;

    keysSetRepeat(20, 8);

    while (1)
    {
        consoleClear();
        printf("Running...\n");
        run_benchmark();
        print_results(page, fast_cpu, dma_status, dma_mismatches);

        while (1)
        {
            swiWaitForVBlank();
            scanKeys();

            uint16_t keys = keysDownRepeat();

            if (keys & KEY_START)
                return 0;

            if (keys & KEY_A)
                break;

            if (keys & KEY_B)
            {
                fast_cpu = !fast_cpu;
                setCpuClock(fast_cpu);
                break;
            }

            if (keys & (KEY_RIGHT | KEY_LEFT))
            {
                if (keys & KEY_RIGHT)
                    page = (page + 1) % PAGE_COUNT;
                else
                    page = (page + PAGE_COUNT - 1) % PAGE_COUNT;

                print_results(page, fast_cpu, dma_status, dma_mismatches);
            }
        }
    }

    return 0;
}
