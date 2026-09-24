// SPDX-License-Identifier: CC0-1.0
//
// Link test for libteak: it is never run, it only has to build and link
// against every module of the library.

#include <teak/teak.h>

static u16 buffer[16];

int main(void)
{
    teakInit();

    cpuDisableIrqs();
    icuIrqSetup(BIT(10), 0);
    cpuEnableInt0();
    cpuEnableIrqs();

    timerStart(0, 0, 0x1000);

    dmaTransferArm9ToDsp(0, 0x02000000, buffer, 16);
    dmaTransferDspToArm9(0, buffer, 0x02000000, 16);

    btdmpSetupOutputSpeakers(0, 1);

    apbpSendData(0, (u16)timerRead(0));

    while (1)
        apbpReceiveData(0);
}
