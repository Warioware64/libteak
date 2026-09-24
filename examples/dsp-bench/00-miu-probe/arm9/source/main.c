// SPDX-License-Identifier: CC0-1.0
//
// DSP bench 00: MIU probe.
//
// The dual-operand multiplications of the DSP ("mpy [r4++], [r0++]") read
// their second operand through the Y bus of the Memory Interface Unit. With
// the setup used by libteak, that operand came from the wrong address on the
// DSi, while emulators return the right value. This ROM shows the MIU
// registers and, for several MIU settings, which address each bus really
// read (see common/miu.h). The first page shows the registers, then there is
// one page per setting.
//
// Version 2: settings with the ZSP bit (Z single access) cleared, and a
// second fill of the area above the X memory (see common/miu.h).

#include <stdio.h>

#include <nds.h>

#include "miu.h"
#include "proto_arm9.h"
#include "teak_tlf_bin.h"
#include "ui.h"

// Names of miu_settings[]
static const char *setting_names[MIU_SETTINGS] = {
    "unchanged",
    "ZSP off",
    "X 16K, Y 16K",
    "X 16K, Y 16K, ZSP off",
    "X 8K, Y 24K, ZSP off",
    "X16K Y16K ZSPoff pagemode",
};

static uint32_t reg_values[3];
static uint32_t single[MIU_SETTINGS][MIU_PAIRS];
static uint32_t dual[MIU_SETTINGS][MIU_PAIRS];
static bool failed;

static uint32_t run_job(uint16_t job)
{
    proto_run_result r;
    proto_run(job, 1, &r);
    if (r.status != PROTO_STATUS_OK)
        failed = true;
    return r.checksum;
}

static void run_all(void)
{
    failed = false;
    reg_values[0] = run_job(JOB_REGS_XYPAGE);
    reg_values[1] = run_job(JOB_REGS_PAGECFG);
    reg_values[2] = run_job(JOB_REGS_MISC);

    for (int s = 0; s < MIU_SETTINGS; s++)
    {
        for (int p = 0; p < MIU_PAIRS; p++)
        {
            single[s][p] = run_job(MIU_JOB(s, p, 0));
            dual[s][p] = run_job(MIU_JOB(s, p, 1));
        }
    }
}

static uint32_t expected(int p)
{
    return ((uint32_t)miu_pairs[p].y << 16) | miu_pairs[p].x;
}

static int count_errors(void)
{
    int errors = failed ? 1 : 0;
    for (int s = 0; s < MIU_SETTINGS; s++)
    {
        for (int p = 0; p < MIU_PAIRS; p++)
        {
            if (single[s][p] != expected(p))
                errors++;
            if (dual[s][p] != expected(p))
                errors++;
        }
    }
    return errors;
}

static void print_registers(void)
{
    printf("MIU registers at start\n\n");
    printf("XPAGE      0810E %04lX\n", reg_values[0] >> 16);
    printf("YPAGE      08110 %04lX\n", reg_values[0] & 0xFFFF);
    printf("PAGE0CFG   08114 %04lX\n", reg_values[1] >> 16);
    printf("PAGE1CFG   08116 %04lX\n", reg_values[1] & 0xFFFF);
    printf("OFFPAGECFG 08118 %04lX\n", reg_values[2] >> 16);
    printf("MISC       0811A %04lX\n", reg_values[2] & 0xFFFF);
    printf("\n");
    printf("Next pages: one MIU setting\n");
    printf("each. Y/X: addresses given,\n");
    printf("then values read by 2 moves\n");
    printf("and by mpy [r4],[r0] (dual).\n");
    printf("A value = the address read.\n");
    printf("-addr (8000+): written with\n");
    printf("the setting active (Y mem?)\n");
    printf("????: unknown (y0 was 0)\n");
}

static void print_setting(int s)
{
    printf("%s\n\n", setting_names[s]);
    printf("Y/X addr  single    dual\n");
    for (int p = 0; p < MIU_PAIRS; p++)
    {
        uint32_t e = expected(p);
        printf("%04X/%04X %04lX %04lX %04lX ",
               miu_pairs[p].y, miu_pairs[p].x,
               single[s][p] >> 16, single[s][p] & 0xFFFF,
               dual[s][p] >> 16);
        if ((dual[s][p] >> 16) == 0)
            printf("????");
        else
            printf("%04lX", dual[s][p] & 0xFFFF);
        printf("%s\n", (single[s][p] == e && dual[s][p] == e) ? "" : "!");
    }
}

static void print_page(const ui_state *s)
{
    ui_header("DSP bench 00: MIU probe", s, count_errors());

    if (failed)
        printf("Some jobs failed!\n");

    if (s->page == 0)
        print_registers();
    else
        print_setting(s->page - 1);

    ui_footer(s);
}

int main(int argc, char **argv)
{
    consoleDemoInit();

    if (!proto_init(teak_tlf_bin))
    {
        while (1)
            swiWaitForVBlank();
    }

    ui_state s = { 0, 1 + MIU_SETTINGS, false };

    while (1)
    {
        consoleClear();
        printf("Running...\n");
        run_all();
        print_page(&s);

        ui_action action = UI_NONE;
        while ((action != UI_RERUN) && (action != UI_EXIT))
        {
            swiWaitForVBlank();
            action = ui_poll_keys(&s);
            if (action == UI_REDRAW)
                print_page(&s);
        }

        if (action == UI_EXIT)
            break;
    }

    return 0;
}
