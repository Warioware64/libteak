// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the code generation check: it runs the tests requested by the
// ARM9 and sends back the results.

#include <teak/teak.h>

#include "cgtests.h"

static void reply(u16 status, u32 value)
{
    apbpSendData(0, status);
    apbpSendData(1, value >> 16);
    apbpSendData(2, value & 0xFFFF);
}

int main(void)
{
    teakInit();

    u16 last_seq = 0xFFFF;
    u32 repeated_cmds = 0;

    while (1)
    {
        u16 cmd = apbpReceiveData(0);
        u16 seq = cmd & CG_SEQ_MASK;

        // The same command seen again
        if (seq == last_seq)
        {
            repeated_cmds++;
            continue;
        }
        last_seq = seq;

        if ((cmd & CG_CMD_MASK) == CG_CMD_INIT)
        {
            cg_init();
            reply(seq | CG_STATUS_OK, (u32)cg_test_count() << 16);
        }
        else if ((cmd & CG_CMD_MASK) == CG_CMD_RUN)
        {
            reply(seq | CG_STATUS_OK, cg_run(cmd & CG_INDEX_MASK));
        }
        else if ((cmd & CG_CMD_MASK) == CG_CMD_STATS)
        {
            reply(seq | CG_STATUS_OK, repeated_cmds);
        }
        else
        {
            reply(seq | CG_STATUS_BAD_CMD, 0);
        }
    }

    return 0;
}
