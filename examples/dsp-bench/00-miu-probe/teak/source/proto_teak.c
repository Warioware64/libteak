// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the dsp-bench protocol (see common/proto.h).

#include <teak/teak.h>

#include <stddef.h>

#include "proto_teak.h"

// DMA channel 0 is used by the FIFO functions of libteak
#define DMA_CHANNEL         1

// Largest DMA transfer allowed by libteak, in words
#define DMA_MAX_WORDS       512

static void reply(uint16_t rep0, uint32_t value)
{
    apbpSendData(0, rep0);
    apbpSendData(1, value >> 16);
    apbpSendData(2, value & 0xFFFF);
}

uint16_t proto_dma_out(const void *src, uint32_t address, uint16_t words)
{
    const uint16_t *p = src;

    while (words > 0)
    {
        // Words until the next 1 KB boundary of ARM9 memory
        uint16_t n = (uint16_t)((1024 - (address & 1023)) >> 1);
        if (n > DMA_MAX_WORDS)
            n = DMA_MAX_WORDS;
        if (n > words)
            n = words;

        while (ahbmIsBusy());

        if (dmaTransferDspToArm9(DMA_CHANNEL, p, address, n) != 0)
            return PROTO_STATUS_DMA_ERROR;

        p += n;
        address += (uint32_t)n * 2;
        words -= n;
    }

    return PROTO_STATUS_OK;
}

int main(void)
{
    teakInit();
    app_init();

    // Timer 0 counts down one step per DSP cycle
    const uint16_t timer_config = TMR_CONTROL_PRESCALE_1
                                | TMR_CONTROL_MODE_ONCE
                                | TMR_CONTROL_UNPAUSE
                                | TMR_CONTROL_UNFREEZE_COUNTER
                                | TMR_CONTROL_CLOCK_INTERNAL
                                | TMR_CONTROL_AUTOCLEAR_OFF;

    uint16_t last_seq = 0xFFFF;
    uint32_t repeated_cmds = 0;

    uint16_t *upload_ptr = NULL;
    uint16_t upload_left = 0;

    while (1)
    {
        uint16_t cmd = apbpReceiveData(0);
        uint16_t seq = cmd & PROTO_SEQ_MASK;

        // The same command seen twice
        if (seq == last_seq)
        {
            repeated_cmds++;
            continue;
        }
        last_seq = seq;

        // Written by the ARM9 before CMD0
        uint32_t arg = ((uint32_t)REG_APBP_CMD1 << 16) | REG_APBP_CMD2;

        uint16_t operand = cmd & PROTO_OPERAND_MASK;

        switch (cmd & PROTO_CMD_MASK)
        {
            case PROTO_CMD_UPLOAD_SELECT:
            {
                uint16_t words = (uint16_t)arg;
                upload_ptr = app_upload_buffer(operand, words);
                if (upload_ptr == NULL)
                {
                    upload_left = 0;
                    reply(seq | PROTO_STATUS_BAD_ARG, 0);
                }
                else
                {
                    upload_left = words;
                    reply(seq | PROTO_STATUS_OK, 0);
                }
                break;
            }
            case PROTO_CMD_UPLOAD_DATA:
            {
                if (upload_left == 0)
                {
                    reply(seq | PROTO_STATUS_BAD_ARG, 0);
                    break;
                }

                *upload_ptr++ = arg >> 16;
                upload_left--;

                if (upload_left > 0)
                {
                    *upload_ptr++ = arg & 0xFFFF;
                    upload_left--;
                }

                reply(seq | PROTO_STATUS_OK, 0);
                break;
            }
            case PROTO_CMD_RUN:
            {
                uint16_t repeat = (uint16_t)arg;
                uint16_t status = PROTO_STATUS_OK;

                timerStart(0, timer_config, 0xFFFFFFFF);
                uint32_t start = timerRead(0);

                for (uint16_t i = 0; i < repeat; i++)
                    status |= app_job(operand);

                uint32_t end = timerRead(0);
                timerStop(0);

                uint32_t checksum = app_checksum(operand);

                reply(seq | status, checksum);
                reply(seq | PROTO_REPLY_SECOND | status, start - end);
                break;
            }
            case PROTO_CMD_SEND:
                reply(seq | app_send(operand, arg), 0);
                break;
            case PROTO_CMD_PARAM:
                reply(seq | app_param(operand, arg), 0);
                break;
            case PROTO_CMD_STATS:
                reply(seq | PROTO_STATUS_OK, repeated_cmds);
                break;
            default:
                reply(seq | PROTO_STATUS_BAD_CMD, 0);
                break;
        }
    }

    return 0;
}
