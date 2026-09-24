// SPDX-License-Identifier: CC0-1.0
//
// ARM9 <-> DSP protocol of the dsp-bench examples. Every example has its own
// copy of this file (they are standalone), so keep them identical.
//
// Commands are sent through the APBP command registers:
//
//   CMD1:CMD2  32-bit argument (high:low), written FIRST
//   CMD0       command | sequence number | operand, written LAST
//
// The DSP waits for CMD0, then reads CMD1 and CMD2 directly. On hardware,
// waiting for the "new" flags of CMD1/CMD2 returned stale values, and the DSP
// sometimes saw the same CMD0 twice, so:
//
// - Every command has a sequence number (1 to 15). The DSP ignores a command
//   with the same number as the previous one.
// - Every reply has the sequence number of its command in REP0. The ARM9
//   ignores replies with another sequence number.
//
// Replies go through REP0 (sequence number | PROTO_REPLY_SECOND | status) and
// REP1:REP2 (32-bit value, high:low).

#ifndef PROTO_H__
#define PROTO_H__

#include <stdint.h>

// CMD0 layout
#define PROTO_CMD_MASK          0xF000
#define PROTO_SEQ_SHIFT         8
#define PROTO_SEQ_MASK          (0xF << PROTO_SEQ_SHIFT)
#define PROTO_OPERAND_MASK      0x00FF

// Commands

// Selects the table to upload. Operand = table id, argument = size in words.
// Reply: status.
#define PROTO_CMD_UPLOAD_SELECT 0x1000
// Uploads two words of the selected table (argument = word n : word n + 1).
// With an odd size, the low half of the last argument is ignored.
// Reply: status.
#define PROTO_CMD_UPLOAD_DATA   0x2000
// Runs a job "argument" times. Operand = job id.
// Reply 1: status, checksum of the output of the job.
// Reply 2 (PROTO_REPLY_SECOND): status, DSP cycles of all repetitions.
#define PROTO_CMD_RUN           0x3000
// Sends an output buffer to ARM9 memory with DMA. Operand = buffer id,
// argument = ARM9 address. Reply: status.
#define PROTO_CMD_SEND          0x4000
// Sets a parameter of the example. Operand = parameter id, argument = value.
// Reply: status.
#define PROTO_CMD_PARAM         0x5000
// Reply: status, number of repeated commands ignored by the DSP.
#define PROTO_CMD_STATS         0x6000

// REP0 layout
#define PROTO_REPLY_SECOND      0x1000
#define PROTO_STATUS_MASK       0x00FF

#define PROTO_STATUS_OK         0
#define PROTO_STATUS_BAD_CMD    1
#define PROTO_STATUS_BAD_ARG    2
#define PROTO_STATUS_DMA_ERROR  3

// DSP clock of the DSi, in Hz. DSP timer 0 counts DSP cycles.
#define PROTO_DSP_CLOCK         134055928

// Checksum of an output, computed the same way on both CPUs from the values
// (not from the memory layout, which differs for 32-bit values).
typedef struct {
    uint32_t a, b;
} proto_checksum;

static inline void checksum_init(proto_checksum *c)
{
    c->a = 1;
    c->b = 0;
}

static inline void checksum_add16(proto_checksum *c, uint16_t v)
{
    c->a += v;
    c->b += c->a;
}

static inline void checksum_add32(proto_checksum *c, uint32_t v)
{
    checksum_add16(c, (uint16_t)v);
    checksum_add16(c, (uint16_t)(v >> 16));
}

static inline uint32_t checksum_get(const proto_checksum *c)
{
    return (c->b << 16) ^ c->a;
}

#endif // PROTO_H__
