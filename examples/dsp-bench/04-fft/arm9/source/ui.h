// SPDX-License-Identifier: CC0-1.0
//
// Pages, keys and timing helpers of the dsp-bench examples (ARM9 side).
//
// The console is 32x24 characters. Every page has a header (4 lines) and a
// footer (2 lines at the bottom).

#ifndef UI_H__
#define UI_H__

#include <stdbool.h>
#include <stdint.h>

#include "proto_arm9.h"

// Number of measurements of each variant. The minimum is kept.
#define UI_RUNS     4

typedef struct {
    int page;
    int pages;
    bool fast_cpu;
} ui_state;

typedef enum {
    UI_NONE,
    UI_REDRAW,  // The page has changed
    UI_RERUN,   // Run the measurements again (A, or B after changing clock)
    UI_EXIT,
} ui_action;

// Clears the screen and prints the header of a page
void ui_header(const char *title, const ui_state *s, int errors);

// Prints the footer at the bottom of the screen
void ui_footer(const ui_state *s);

// Prints a speedup in 8 characters ("  12.34x")
void ui_print_speedup(uint32_t ref_us, uint32_t us);

// Prints the communication glitches that have been recovered
void ui_print_glitches(void);

// Handles the keys. Call it once per frame.
ui_action ui_poll_keys(ui_state *s);

// Minimum time in microseconds of UI_RUNS calls of fn()
typedef void (*arm9_fn)(void);
uint32_t measure_arm9_us(arm9_fn fn);

// Runs a DSP job UI_RUNS times (once per run) and keeps the minimum times
typedef struct {
    uint16_t status;
    uint32_t checksum;
    uint32_t dsp_us;        // Measured by the DSP
    uint32_t round_trip_us; // Measured by the ARM9
} dsp_measure;

void measure_dsp(uint16_t job, dsp_measure *m);

// Results tables
// --------------
//
// Each row is a kernel measured in up to 4 variants: ARM9 C (Thumb), ARM9
// assembly, DSP C and DSP assembly. The ARM9 C version is the reference: the
// checksum of the output of every other variant must be the same.

#define NO_JOB  0xFFFF

// Checksum of the output of a job, computed by the ARM9 (see common/proto.h)
typedef uint32_t (*checksum_fn)(uint16_t job);

typedef struct {
    const char *name;       // Up to 6 characters
    uint16_t job;           // Job id used for the checksum of the output
    arm9_fn arm_c;
    arm9_fn arm_asm;        // NULL if there is no ARM9 assembly version
    uint16_t dsp_c_job;     // NO_JOB if there is no DSP C version
    uint16_t dsp_asm_job;   // NO_JOB if there is no DSP assembly version

    // Results
    uint32_t ref_checksum;
    uint32_t arm_c_us;
    uint32_t arm_asm_us;
    uint32_t arm_asm_checksum;
    dsp_measure dsp_c;
    dsp_measure dsp_asm;
} bench_row;

// Measures all rows. The ARM9 C version runs last, so the output buffers keep
// its results.
void bench_run(bench_row *rows, int n, checksum_fn checksum);

// Number of variants whose results don't match the ARM9 C version
int bench_errors(const bench_row *rows, int n);

// Standard pages: times, speedup of the DSP, round trip, results
void bench_print_times(const bench_row *rows, int n);
void bench_print_speedup(const bench_row *rows, int n);
void bench_print_round_trip(const bench_row *rows, int n);
void bench_print_results(const bench_row *rows, int n);

#endif // UI_H__
