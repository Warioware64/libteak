// SPDX-License-Identifier: CC0-1.0
//
// Code generation check for the Teak LLVM toolchain.
//
// The tests in common/cgtests.c are built for the ARM9 and for the DSP. Each
// test exercises one feature of the compiler (shifts, 32-bit arithmetic, loops,
// calls, spills...). The ARM9 runs them and compares its results with the
// results of the DSP, so that a failure points at the feature that is broken.

#include <stdio.h>

#include <nds.h>

#include "cgtests.h"
#include "teak_tlf_bin.h"

#define MAX_TESTS       64
#define TESTS_PER_PAGE  17

static const char *test_names[] = {
    "const32", "zext16", "sext16",
    "add16", "sub16", "and16", "or16", "xor16", "not16", "neg16",
    "shl16", "lshr16", "ashr16", "vshl16", "vlshr16",
    "add32", "sub32", "and32", "xor32", "shl32", "lshr32", "ashr32",
    "mul16", "muls32", "mulu32", "mulk32",
    "cmp16", "cmp32",
    "loop", "loop32",
    "sumu", "sums", "sum16", "store", "global", "dot",
    "call4", "call32", "nested", "spill", "switch",
    "lshr_only", "shl_only", "xorsh1", "xorsh16",
};

#define NUM_NAMES (sizeof(test_names) / sizeof(test_names[0]))

typedef struct {
    uint32_t expected;
    uint32_t got;
    uint16_t status;
} test_result;

static test_result results[MAX_TESTS];
static int num_tests;

static void wait_forever(void)
{
    while (1)
        swiWaitForVBlank();
}

// Replies received with the wrong sequence number, and commands the DSP ignored
// because it saw them twice (see cgtests.h)
static uint32_t stale_replies;
static uint32_t repeated_cmds;

static uint16_t dsp_command(uint16_t cmd, uint32_t *value)
{
    static uint16_t seq = 0;

    seq = (seq % 15) + 1;
    uint16_t seq_bits = seq << CG_SEQ_SHIFT;

    dspSendData(0, cmd | seq_bits);

    while (1)
    {
        uint16_t rep0 = dspReceiveData(0);
        uint32_t v = (uint32_t)dspReceiveData(1) << 16;
        v |= dspReceiveData(2);

        if ((rep0 & CG_SEQ_MASK) == seq_bits)
        {
            *value = v;
            return rep0 & CG_STATUS_MASK;
        }

        stale_replies++;
    }
}

static bool test_ok(const test_result *r)
{
    return (r->status == CG_STATUS_OK) && (r->expected == r->got);
}

static void run_tests(void)
{
    for (int i = 0; i < num_tests; i++)
    {
        test_result *r = &results[i];

        r->expected = cg_run(i);
        r->status = dsp_command(CG_CMD_RUN | i, &r->got);
    }

    dsp_command(CG_CMD_STATS, &repeated_cmds);
}

static void print_page(int page, int num_pages)
{
    consoleClear();

    int errors = 0;
    for (int i = 0; i < num_tests; i++)
    {
        if (!test_ok(&results[i]))
            errors++;
    }

    printf("DSP codegen check: %d tests\n", num_tests);
    if (errors == 0)
        printf("Results: all OK\n");
    else
        printf("Results: %d FAIL\n", errors);

    // Communication glitches, recovered with the sequence numbers
    printf("Ignored: %lu replies, %lu cmds\n", stale_replies, repeated_cmds);

    // Name (9) + expected (8) + got (8) + status, separated by spaces
    printf("%-9s %-8s %-8s\n", "test", "ARM9", "DSP");

    int first = page * TESTS_PER_PAGE;
    int last = first + TESTS_PER_PAGE;
    if (last > num_tests)
        last = num_tests;

    for (int i = first; i < last; i++)
    {
        test_result *r = &results[i];
        const char *name = (i < (int)NUM_NAMES) ? test_names[i] : "?";

        if (test_ok(r))
            printf("%-9.9s %08lX OK\n", name, r->expected);
        else
            printf("%-9.9s %08lX %08lX X\n", name, r->expected, r->got);
    }

    printf("\x1b[22;0H");
    printf("Left/Right: page %d/%d\n", page + 1, num_pages);
    printf("A: run again  START: exit");
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

    cg_init();

    uint32_t value;
    if (dsp_command(CG_CMD_INIT, &value) != CG_STATUS_OK)
    {
        printf("DSP didn't reply to INIT");
        wait_forever();
    }

    num_tests = value >> 16;
    if ((num_tests != cg_test_count()) || (num_tests > MAX_TESTS))
    {
        printf("Number of tests mismatch:\n");
        printf("ARM9 %d, DSP %d\n", cg_test_count(), num_tests);
        printf("(the DSP reply is broken)");
        wait_forever();
    }

    int num_pages = (num_tests + TESTS_PER_PAGE - 1) / TESTS_PER_PAGE;
    int page = 0;

    while (1)
    {
        consoleClear();
        printf("Running...\n");
        run_tests();
        print_page(page, num_pages);

        while (1)
        {
            swiWaitForVBlank();
            scanKeys();

            uint16_t keys = keysDown();

            if (keys & KEY_START)
                return 0;

            if (keys & KEY_A)
                break;

            if (keys & (KEY_RIGHT | KEY_LEFT))
            {
                if (keys & KEY_RIGHT)
                    page = (page + 1) % num_pages;
                else
                    page = (page + num_pages - 1) % num_pages;

                print_page(page, num_pages);
            }
        }
    }

    return 0;
}
