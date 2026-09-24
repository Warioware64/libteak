// SPDX-License-Identifier: CC0-1.0
//
// Definitions shared by the ARM9 and the DSP sides of the benchmark.

#ifndef BENCH_H__
#define BENCH_H__

#include <stdint.h>

// Number of 16-bit elements processed by each kernel call
#define BENCH_N             1024

// Number of times each kernel is called per measurement
#define BENCH_REPEAT        16

// Seed of the xorshift kernel (it must not be zero)
#define BENCH_SEED          0xACE1

// Seeds used to generate the input buffers (they must not be zero)
#define BENCH_SEED_A        0x1234
#define BENCH_SEED_B        0xBEEF

// DSP clock of the DSi, in Hz. The DSP timers count DSP cycles.
#define BENCH_DSP_CLOCK     134055928

// DMA channel used by the DSP to read the input buffers (1 to 7)
#define BENCH_DMA_CHANNEL   1

// Both CPUs generate the same input buffers with bench_fill(), so the kernels
// don't depend on DMA. The ARM9 copy is also read by the DSP with DMA to check
// that transfers work (melonDS doesn't emulate them, so it fails there).
//
// Input buffers in ARM9 memory: BENCH_N words for "a", then BENCH_N for "b".
// The DSP reads them in chunks of 512 words, and a chunk can't cross a 1 KB
// boundary, so the buffer must be aligned to at least 1 KB.
#define BENCH_CHUNK_WORDS   512

// Commands, sent by the ARM9 in CMD0. CMD1 and CMD2 are the high and low halves
// of the 32-bit argument (always sent, even if the command doesn't use it).
// They are written before CMD0, and the DSP reads them when it sees CMD0.
//
// Every command carries a sequence number (1 to 15) that is copied to every
// reply. On hardware, the ARM9 can see a reply that isn't new and the DSP can
// see the same command twice, so both sides use it to discard stale data:
// the DSP ignores a command with the same sequence number as the previous one,
// and the ARM9 ignores replies with the wrong sequence number.
//
// - BENCH_CMD_LOAD: argument = address of the input buffers in ARM9 memory.
//   The DSP generates its buffers and compares them with the ARM9 copy.
//   Reply: REP0 = status, REP1:REP2 = number of words that don't match.
//
// - BENCH_CMD_RUN | kernel | variant: argument = number of repetitions.
//   Reply 1: REP0 = status, REP1:REP2 = result of the last repetition.
//   Reply 2: REP0 = status | BENCH_REPLY_SECOND, REP1:REP2 = DSP cycles of all
//   repetitions.
//
// - BENCH_CMD_STATS: Reply: REP0 = status, REP1:REP2 = number of repeated
//   commands ignored by the DSP.
//
// REP0 = sequence number | reply flags | status.
#define BENCH_CMD_LOAD      0x1000
#define BENCH_CMD_RUN       0x2000
#define BENCH_CMD_STATS     0x3000
#define BENCH_CMD_MASK      0xF000

#define BENCH_SEQ_SHIFT     8
#define BENCH_SEQ_MASK      (0xF << BENCH_SEQ_SHIFT)

#define BENCH_REPLY_SECOND  0x1000
#define BENCH_STATUS_MASK   0x00FF

#define BENCH_KERNEL_SHIFT  4
#define BENCH_KERNEL_MASK   (0xF << BENCH_KERNEL_SHIFT)
#define BENCH_VARIANT_MASK  0xF

#define BENCH_STATUS_OK             0
#define BENCH_STATUS_DMA_ERROR      1 // A transfer couldn't be started
#define BENCH_STATUS_DMA_MISMATCH   2 // The data received doesn't match
#define BENCH_STATUS_BAD_CMD        3

typedef enum {
    BENCH_KERNEL_DOT16 = 0,     // int32 sum of a[i] * b[i]
    BENCH_KERNEL_SUM16 = 1,     // uint32 sum of (uint16) a[i]
    BENCH_KERNEL_XORSHIFT = 2,  // BENCH_N xorshift16 steps
    BENCH_KERNEL_COUNT
} bench_kernel;

typedef enum {
    BENCH_VARIANT_C = 0,
    BENCH_VARIANT_ASM = 1,
    BENCH_VARIANT_COUNT
} bench_variant;

// Shared code in C, built for both the ARM9 and the DSP (bench_kernels.c)

// Fills a buffer with pseudo-random values in [-1024, 1023], so that the dot
// product of two buffers can't overflow 32 bits.
void bench_fill(int16_t *buffer, uint16_t n, uint16_t seed);

uint32_t bench_dot16_c(const int16_t *a, const int16_t *b, uint16_t n);
uint32_t bench_sum16_c(const uint16_t *a, uint16_t n);
uint32_t bench_xorshift16_c(uint16_t seed, uint16_t steps);

#ifdef TEAK
// Hand-written DSP kernels (teak/source/kernels_asm.s). They return the same
// results as the C versions.
uint32_t bench_dot16_asm(const int16_t *a, const int16_t *b, uint16_t n); // n >= 2
uint32_t bench_sum16_asm(const uint16_t *a, uint16_t n); // n >= 1
uint32_t bench_xorshift16_asm(uint16_t seed, uint16_t steps); // steps >= 1
#endif

#endif // BENCH_H__
