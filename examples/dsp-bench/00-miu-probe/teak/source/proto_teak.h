// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the dsp-bench protocol (see common/proto.h). proto_teak.c has
// main(): it runs the command loop and calls the app_*() functions, which each
// example implements (they are direct calls: the compiler doesn't support calls
// through function pointers yet).

#ifndef PROTO_TEAK_H__
#define PROTO_TEAK_H__

#include <stdint.h>

#include "proto.h"

// Called once at startup.
void app_init(void);

// Returns where to store table "table" of "words" words, or NULL if the table
// doesn't exist or is too small.
uint16_t *app_upload_buffer(uint16_t table, uint16_t words);

// Runs job "job" once. Returns a PROTO_STATUS_* value.
uint16_t app_job(uint16_t job);

// Checksum of the output of job "job" (see checksum_*() in proto.h).
uint32_t app_checksum(uint16_t job);

// Sends output buffer "buffer" to ARM9 address "address" (see proto_dma_out()).
uint16_t app_send(uint16_t buffer, uint32_t address);

// Sets parameter "id" to "value".
uint16_t app_param(uint16_t id, uint32_t value);

// Copies "words" words from DSP memory to ARM9 memory with DMA, split so that
// no transfer crosses a 1 KB boundary of ARM9 memory. "address" must be even.
uint16_t proto_dma_out(const void *src, uint32_t address, uint16_t words);

#endif // PROTO_TEAK_H__
