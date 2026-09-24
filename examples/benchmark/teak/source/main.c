// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the benchmark. It generates the input buffers, checks that it can
// read the ARM9 copy of them with DMA, then runs the kernels requested by the
// ARM9 and reports the result and the number of DSP cycles it took.

#include <teak/teak.h>

#include "bench.h"

static int16_t buffer_a[BENCH_N];
static int16_t buffer_b[BENCH_N];

static int16_t dma_chunk[BENCH_CHUNK_WORDS];

// Generates the input buffers and compares them with the ARM9 copy, read with
// DMA in chunks. The number of words that don't match is returned in
// "mismatches".
static u16 load_buffers(u32 address, u32 *mismatches)
{
    bench_fill(buffer_a, BENCH_N, BENCH_SEED_A);
    bench_fill(buffer_b, BENCH_N, BENCH_SEED_B);

    *mismatches = 0;

    for (u16 chunk = 0; chunk < (BENCH_N * 2) / BENCH_CHUNK_WORDS; chunk++)
    {
        u32 src = address + (u32)chunk * (BENCH_CHUNK_WORDS * 2);

        for (u16 i = 0; i < BENCH_CHUNK_WORDS; i++)
            dma_chunk[i] = 0;

        while (ahbmIsBusy());

        if (dmaTransferArm9ToDsp(BENCH_DMA_CHANNEL, src, dma_chunk,
                                 BENCH_CHUNK_WORDS) != 0)
            return BENCH_STATUS_DMA_ERROR;

        const int16_t *expected = (chunk < 2) ? buffer_a : buffer_b;
        expected += (chunk & 1) * BENCH_CHUNK_WORDS;

        for (u16 i = 0; i < BENCH_CHUNK_WORDS; i++)
        {
            if (dma_chunk[i] != expected[i])
                (*mismatches)++;
        }
    }

    return (*mismatches == 0) ? BENCH_STATUS_OK : BENCH_STATUS_DMA_MISMATCH;
}

static u32 run_kernel(u16 kernel, u16 variant)
{
    const uint16_t *buffer_u = (const uint16_t *)buffer_a;

    if (variant == BENCH_VARIANT_ASM)
    {
        switch (kernel)
        {
            case BENCH_KERNEL_DOT16:
                return bench_dot16_asm(buffer_a, buffer_b, BENCH_N);
            case BENCH_KERNEL_SUM16:
                return bench_sum16_asm(buffer_u, BENCH_N);
            default:
                return bench_xorshift16_asm(BENCH_SEED, BENCH_N);
        }
    }
    else
    {
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
}

static void reply(u16 status, u32 value)
{
    apbpSendData(0, status);
    apbpSendData(1, value >> 16);
    apbpSendData(2, value & 0xFFFF);
}

int main(void)
{
    teakInit();

    // Timer 0 counts down one step per DSP cycle
    const u16 timer_config = TMR_CONTROL_PRESCALE_1
                           | TMR_CONTROL_MODE_ONCE
                           | TMR_CONTROL_UNPAUSE | TMR_CONTROL_UNFREEZE_COUNTER
                           | TMR_CONTROL_CLOCK_INTERNAL
                           | TMR_CONTROL_AUTOCLEAR_OFF;

    u16 last_seq = 0xFFFF;
    u32 repeated_cmds = 0;

    while (1)
    {
        u16 cmd = apbpReceiveData(0);
        u16 seq = cmd & BENCH_SEQ_MASK;

        // The same command seen again
        if (seq == last_seq)
        {
            repeated_cmds++;
            continue;
        }
        last_seq = seq;

        // The ARM9 writes the argument before the command, so it's already
        // there. The registers are read directly: waiting for their "new"
        // flags isn't reliable on hardware (it returned stale values).
        u32 arg = ((u32)REG_APBP_CMD1 << 16) | REG_APBP_CMD2;

        if ((cmd & BENCH_CMD_MASK) == BENCH_CMD_LOAD)
        {
            u32 mismatches;
            u16 status = load_buffers(arg, &mismatches);
            reply(seq | status, mismatches);
        }
        else if ((cmd & BENCH_CMD_MASK) == BENCH_CMD_RUN)
        {
            u16 kernel = (cmd & BENCH_KERNEL_MASK) >> BENCH_KERNEL_SHIFT;
            u16 variant = cmd & BENCH_VARIANT_MASK;
            u16 repeat = arg;

            if (kernel >= BENCH_KERNEL_COUNT || variant >= BENCH_VARIANT_COUNT)
            {
                reply(seq | BENCH_STATUS_BAD_CMD, 0);
                reply(seq | BENCH_REPLY_SECOND | BENCH_STATUS_BAD_CMD, 0);
                continue;
            }

            u32 result = 0;

            timerStart(0, timer_config, 0xFFFFFFFF);
            u32 start = timerRead(0);

            for (u16 i = 0; i < repeat; i++)
                result = run_kernel(kernel, variant);

            u32 end = timerRead(0);
            timerStop(0);

            reply(seq | BENCH_STATUS_OK, result);
            reply(seq | BENCH_REPLY_SECOND | BENCH_STATUS_OK, start - end);
        }
        else if ((cmd & BENCH_CMD_MASK) == BENCH_CMD_STATS)
        {
            reply(seq | BENCH_STATUS_OK, repeated_cmds);
        }
        else
        {
            reply(seq | BENCH_STATUS_BAD_CMD, 0);
        }
    }

    return 0;
}
