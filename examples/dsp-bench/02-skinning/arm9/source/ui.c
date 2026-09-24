// SPDX-License-Identifier: CC0-1.0

#include <stdio.h>

#include <nds.h>

#include "ui.h"

void ui_header(const char *title, const ui_state *s, int errors)
{
    consoleClear();

    printf("%s\n", title);
    printf("ARM9 %s MHz, DSP 134 MHz\n", s->fast_cpu ? "134" : "67");
    if (errors == 0)
        printf("Results: all OK\n");
    else
        printf("Results: %d FAIL\n", errors);
    printf("--------------------------------");
}

void ui_footer(const ui_state *s)
{
    printf("\x1b[22;0H");
    printf("Left/Right: page %d/%d\n", s->page + 1, s->pages);
    printf("A:rerun B:ARM9 clock START:exit");
}

void ui_print_speedup(uint32_t ref_us, uint32_t us)
{
    if (us == 0)
    {
        printf("%8s", "-");
        return;
    }

    uint32_t x100 = (uint32_t)(((uint64_t)ref_us * 100) / us);
    printf(" %3lu.%02lux", x100 / 100, x100 % 100);
}

void ui_print_glitches(void)
{
    printf("Ignored: %lu replies, %lu cmds\n", proto_stale_replies(),
           proto_repeated_cmds());
}

ui_action ui_poll_keys(ui_state *s)
{
    scanKeys();

    uint16_t keys = keysDown();

    if (keys & KEY_START)
        return UI_EXIT;

    if (keys & KEY_A)
        return UI_RERUN;

    if (keys & KEY_B)
    {
        s->fast_cpu = !s->fast_cpu;
        setCpuClock(s->fast_cpu);
        return UI_RERUN;
    }

    if (keys & KEY_RIGHT)
    {
        s->page = (s->page + 1) % s->pages;
        return UI_REDRAW;
    }

    if (keys & KEY_LEFT)
    {
        s->page = (s->page + s->pages - 1) % s->pages;
        return UI_REDRAW;
    }

    return UI_NONE;
}

uint32_t measure_arm9_us(arm9_fn fn)
{
    uint32_t best = UINT32_MAX;

    for (int i = 0; i < UI_RUNS; i++)
    {
        cpuStartTiming(0);
        fn();
        uint32_t t = cpuEndTiming();
        if (t < best)
            best = t;
    }

    return arm9_ticks_to_us(best);
}

void measure_dsp(uint16_t job, dsp_measure *m)
{
    m->status = PROTO_STATUS_OK;
    m->dsp_us = UINT32_MAX;
    m->round_trip_us = UINT32_MAX;

    for (int i = 0; i < UI_RUNS; i++)
    {
        proto_run_result r;
        proto_run(job, 1, &r);

        m->status |= r.status;
        m->checksum = r.checksum;

        uint32_t dsp_us = dsp_cycles_to_us(r.cycles);
        if (dsp_us < m->dsp_us)
            m->dsp_us = dsp_us;

        uint32_t rt_us = arm9_ticks_to_us(r.round_trip);
        if (rt_us < m->round_trip_us)
            m->round_trip_us = rt_us;
    }
}

// Results tables

void bench_run(bench_row *rows, int n, checksum_fn checksum)
{
    for (int i = 0; i < n; i++)
    {
        bench_row *r = &rows[i];

        if (r->arm_asm)
        {
            r->arm_asm_us = measure_arm9_us(r->arm_asm);
            r->arm_asm_checksum = checksum(r->job);
        }

        r->arm_c_us = measure_arm9_us(r->arm_c);
        r->ref_checksum = checksum(r->job);

        if (r->dsp_c_job != NO_JOB)
            measure_dsp(r->dsp_c_job, &r->dsp_c);
        if (r->dsp_asm_job != NO_JOB)
            measure_dsp(r->dsp_asm_job, &r->dsp_asm);
    }
}

static bool dsp_ok(const bench_row *r, const dsp_measure *m)
{
    return (m->status == PROTO_STATUS_OK) && (m->checksum == r->ref_checksum);
}

int bench_errors(const bench_row *rows, int n)
{
    int errors = 0;

    for (int i = 0; i < n; i++)
    {
        const bench_row *r = &rows[i];

        if (r->arm_asm && (r->arm_asm_checksum != r->ref_checksum))
            errors++;
        if ((r->dsp_c_job != NO_JOB) && !dsp_ok(r, &r->dsp_c))
            errors++;
        if ((r->dsp_asm_job != NO_JOB) && !dsp_ok(r, &r->dsp_asm))
            errors++;
    }

    return errors;
}

// Prints a time in microseconds in a column of "width" characters (in
// milliseconds if it doesn't fit), or "-" without value
static void print_col(int width, bool present, uint32_t value)
{
    if (!present)
        printf("%*s", width, "-");
    else if (value < 100000)
        printf("%*lu", width, value);
    else
        printf("%*lums", width - 2, value / 1000);
}

void bench_print_times(const bench_row *rows, int n)
{
    printf("Time (us)\n\n");
    printf("%-6s%6s%6s%6s%6s\n", "", "ARM-C", "ARMas", "DSP-C", "DSPas");
    for (int i = 0; i < n; i++)
    {
        const bench_row *r = &rows[i];
        printf("%-6.6s", r->name);
        print_col(6, true, r->arm_c_us);
        print_col(6, r->arm_asm != NULL, r->arm_asm_us);
        print_col(6, r->dsp_c_job != NO_JOB, r->dsp_c.dsp_us);
        print_col(6, r->dsp_asm_job != NO_JOB, r->dsp_asm.dsp_us);
        printf("\n");
    }
    printf("\nARM: ARM9 C (Thumb) / ARM asm\n");
    printf("DSP: C / asm, cycles counted\n");
    printf("by the DSP\n");
}

void bench_print_speedup(const bench_row *rows, int n)
{
    printf("Speedup of the DSP over ARM9 C\n\n");
    printf("%-14s%8s%8s\n", "", "DSP-C", "DSP-asm");
    for (int i = 0; i < n; i++)
    {
        const bench_row *r = &rows[i];
        printf("%-14.14s", r->name);
        if (r->dsp_c_job != NO_JOB)
            ui_print_speedup(r->arm_c_us, r->dsp_c.dsp_us);
        else
            printf("%8s", "-");
        if (r->dsp_asm_job != NO_JOB)
            ui_print_speedup(r->arm_c_us, r->dsp_asm.dsp_us);
        else
            printf("%8s", "-");
        printf("\n");
    }
    printf("\nAbove 1.00x the DSP is faster\n");
}

void bench_print_round_trip(const bench_row *rows, int n)
{
    printf("Round trip seen by ARM9 (us)\n\n");
    printf("%-14s%8s%8s\n", "", "DSP-C", "DSP-asm");
    for (int i = 0; i < n; i++)
    {
        const bench_row *r = &rows[i];
        printf("%-14.14s", r->name);
        print_col(8, r->dsp_c_job != NO_JOB, r->dsp_c.round_trip_us);
        print_col(8, r->dsp_asm_job != NO_JOB, r->dsp_asm.round_trip_us);
        printf("\n");
    }
    printf("\nIncludes the communication\n");
}

static const char *ok_str(bool ok)
{
    return ok ? "OK" : "FAIL";
}

void bench_print_results(const bench_row *rows, int n)
{
    printf("Results vs ARM9 C\n\n");
    printf("%-6s%8s%8s%8s\n", "", "ARM-asm", "DSP-C", "DSP-asm");
    for (int i = 0; i < n; i++)
    {
        const bench_row *r = &rows[i];
        printf("%-6.6s", r->name);
        if (r->arm_asm)
            printf("%8s", ok_str(r->arm_asm_checksum == r->ref_checksum));
        else
            printf("%8s", "-");
        if (r->dsp_c_job != NO_JOB)
            printf("%8s", ok_str(dsp_ok(r, &r->dsp_c)));
        else
            printf("%8s", "-");
        if (r->dsp_asm_job != NO_JOB)
            printf("%8s", ok_str(dsp_ok(r, &r->dsp_asm)));
        else
            printf("%8s", "-");
        printf("\n");
    }
    printf("\n");
    ui_print_glitches();
}
