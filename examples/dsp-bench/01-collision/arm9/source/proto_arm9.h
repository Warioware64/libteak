// SPDX-License-Identifier: CC0-1.0
//
// ARM9 side of the dsp-bench protocol (see common/proto.h).

#ifndef PROTO_ARM9_H__
#define PROTO_ARM9_H__

#include <stdbool.h>
#include <stdint.h>

#include "proto.h"

typedef struct {
    uint16_t status;
    uint32_t checksum;      // Checksum of the output of the job
    uint32_t cycles;        // DSP cycles of all repetitions
    uint32_t round_trip;    // ARM9 timer ticks from command to first reply
} proto_run_result;

// Loads and starts the DSP binary, and checks that it replies. Prints the
// reason and returns false on error.
bool proto_init(const void *tlf);

// Sends a command and waits for its reply. Returns the status.
uint16_t proto_cmd(uint16_t cmd, uint32_t arg, uint32_t *value);

// Uploads a table to the DSP (two words per command).
uint16_t proto_upload(uint16_t table, const uint16_t *data, uint16_t words);

// Sets a parameter of the example.
uint16_t proto_param(uint16_t id, uint32_t value);

// Asks the DSP to send output buffer "buffer" to "dst" with DMA. "dst" must be
// aligned to 32 bytes (a cache line) and "bytes" must be a multiple of 32.
uint16_t proto_send(uint16_t buffer, void *dst, uint32_t bytes);

// Runs a job "repeat" times on the DSP and waits for the result.
uint16_t proto_run(uint16_t job, uint16_t repeat, proto_run_result *r);

// Same as proto_run(), split in two: start it, then poll until it's done.
void proto_run_start(uint16_t job, uint16_t repeat);
bool proto_run_poll(proto_run_result *r);

// Communication glitches detected (see common/proto.h)
uint32_t proto_stale_replies(void);
uint32_t proto_repeated_cmds(void);

// Unit conversions
static inline uint32_t arm9_ticks_to_us(uint32_t ticks)
{
    return ((uint64_t)ticks * 1000000) / 33513982;
}

static inline uint32_t dsp_cycles_to_us(uint32_t cycles)
{
    return ((uint64_t)cycles * 1000000) / PROTO_DSP_CLOCK;
}

#endif // PROTO_ARM9_H__
