// SPDX-License-Identifier: CC0-1.0
//
// ARM9 side of the dsp-bench protocol (see common/proto.h).

#include <stdio.h>

#include <nds.h>

#include "proto_arm9.h"

static uint16_t cmd_seq; // Sequence number of the last command, shifted
static uint32_t stale_replies;

static uint32_t run_start_ticks;

static void send(uint16_t cmd, uint32_t arg)
{
    static uint16_t seq = 0;

    seq = (seq % 15) + 1;
    cmd_seq = seq << PROTO_SEQ_SHIFT;

    // The argument goes first: the DSP reads it when it sees CMD0
    dspSendData(1, arg >> 16);
    dspSendData(2, arg & 0xFFFF);
    dspSendData(0, cmd | cmd_seq);
}

// Waits for the reply "part" (0 or PROTO_REPLY_SECOND) of the last command
static uint16_t receive(uint16_t part, uint32_t *value)
{
    while (1)
    {
        uint16_t rep0 = dspReceiveData(0);
        uint32_t v = (uint32_t)dspReceiveData(1) << 16;
        v |= dspReceiveData(2);

        if ((rep0 & (PROTO_SEQ_MASK | PROTO_REPLY_SECOND)) == (cmd_seq | part))
        {
            if (value)
                *value = v;
            return rep0 & PROTO_STATUS_MASK;
        }

        stale_replies++;
    }
}

bool proto_init(const void *tlf)
{
    if (!isDSiMode())
    {
        printf("DSP only available on DSi\n");
        return false;
    }

    if (dspExecuteDefaultTLF(tlf) != DSP_EXEC_OK)
    {
        printf("Failed to execute TLF\n");
        return false;
    }

    if (proto_cmd(PROTO_CMD_STATS, 0, NULL) != PROTO_STATUS_OK)
    {
        printf("The DSP doesn't reply\n");
        return false;
    }

    return true;
}

uint16_t proto_cmd(uint16_t cmd, uint32_t arg, uint32_t *value)
{
    send(cmd, arg);
    return receive(0, value);
}

uint16_t proto_upload(uint16_t table, const uint16_t *data, uint16_t words)
{
    uint16_t status = proto_cmd(PROTO_CMD_UPLOAD_SELECT | table, words, NULL);
    if (status != PROTO_STATUS_OK)
        return status;

    for (uint16_t i = 0; i < words; i += 2)
    {
        uint32_t arg = (uint32_t)data[i] << 16;
        if (i + 1 < words)
            arg |= data[i + 1];

        status = proto_cmd(PROTO_CMD_UPLOAD_DATA, arg, NULL);
        if (status != PROTO_STATUS_OK)
            return status;
    }

    return PROTO_STATUS_OK;
}

uint16_t proto_param(uint16_t id, uint32_t value)
{
    return proto_cmd(PROTO_CMD_PARAM | id, value, NULL);
}

uint16_t proto_send(uint16_t buffer, void *dst, uint32_t bytes)
{
    // Write back dirty cache lines so that they can't overwrite the data
    // written by the DSP later, and drop the lines after the transfer.
    DC_FlushRange(dst, bytes);

    uint16_t status = proto_cmd(PROTO_CMD_SEND | buffer, (uintptr_t)dst, NULL);

    DC_InvalidateRange(dst, bytes);

    return status;
}

void proto_run_start(uint16_t job, uint16_t repeat)
{
    cpuStartTiming(0);
    run_start_ticks = 0;
    send(PROTO_CMD_RUN | job, repeat);
}

bool proto_run_poll(proto_run_result *r)
{
    if (!dspReceiveDataReady(0))
        return false;

    r->status = receive(0, &r->checksum);
    r->round_trip = cpuEndTiming() - run_start_ticks;
    receive(PROTO_REPLY_SECOND, &r->cycles);
    return true;
}

uint16_t proto_run(uint16_t job, uint16_t repeat, proto_run_result *r)
{
    proto_run_start(job, repeat);
    while (!proto_run_poll(r));
    return r->status;
}

uint32_t proto_stale_replies(void)
{
    return stale_replies;
}

uint32_t proto_repeated_cmds(void)
{
    uint32_t value = 0;
    proto_cmd(PROTO_CMD_STATS, 0, &value);
    return value;
}
