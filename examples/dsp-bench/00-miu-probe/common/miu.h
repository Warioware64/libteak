// SPDX-License-Identifier: CC0-1.0
//
// Settings and addresses tested by the MIU probe, shared by both CPUs.
//
// The DSP fills the data memory from MIU_FILL_START to 0x7FFF with the address
// of each word, so the value read by an instruction is the address it really
// used. For each MIU setting and each pair of addresses, the DSP reads the Y
// address with r4 and the X address with r0:
//
// - "dual": one "mpy [r4], [r0], a0" (the second operand uses the Y bus)
// - "single": two normal moves
//
// Both CPUs must see the address itself. Any other value shows which address
// the hardware read instead. When a setting makes the X memory smaller, the
// area above it is filled with -address while the setting is active, so a
// value 8000h-EFFFh (-address) comes from that fill.

#ifndef MIU_H__
#define MIU_H__

#include <stdint.h>

#include "proto.h"

#define MIU_FILL_START  0x1000

// MIU registers (DSP addresses)
#define MIU_XPAGE       0x810E
#define MIU_YPAGE       0x8110
#define MIU_PAGE0CFG    0x8114  // Bits 0-5: X size, bits 8-13: Y size (1 KiW)
#define MIU_PAGE1CFG    0x8116
#define MIU_OFFPAGECFG  0x8118
#define MIU_MISC        0x811A  // Bit 6: page mode

// MISC bits changed by the settings
#define MIU_MISC_ZSP        (1 << 4)    // Z single access mode
#define MIU_MISC_PGM        (1 << 6)    // Paging mode

typedef struct {
    int16_t zsp;        // -1: unchanged, else the new ZSP bit
    int16_t page_mode;  // -1: unchanged, else the new PGM bit
    int16_t page0cfg;   // -1: unchanged, else the new PAGE0CFG (Y << 8 | X)
} miu_setting;

// Hardware results of the first version (DSi, MISC = 0x0014 and
// PAGE0CFG = 0x1E20 at start): the dual read returns the value at the X
// address for both operands, and a smaller X size makes everything above it
// read 0 with normal moves. ZSP is the "single access" bit, so the second
// version also tests with ZSP = 0, and fills the area above the X size with
// -address while the setting is active (see miu_fill_neg()).
static const miu_setting miu_settings[] = {
    { -1, -1, -1 },         // Unchanged
    { 0, -1, -1 },          // ZSP off
    { -1, -1, 0x1010 },     // X 16 KiW, Y 16 KiW
    { 0, -1, 0x1010 },      // X 16 KiW, Y 16 KiW, ZSP off
    { 0, -1, 0x1808 },      // X 8 KiW, Y 24 KiW, ZSP off
    { 0, 1, 0x1010 },       // X 16 KiW, Y 16 KiW, ZSP off, page mode
};

#define MIU_SETTINGS    ((int)(sizeof(miu_settings) / sizeof(miu_settings[0])))

typedef struct {
    uint16_t y, x;
} miu_pair;

static const miu_pair miu_pairs[] = {
    { 0x1010, 0x1020 },
    { 0x1100, 0x1200 },
    { 0x1100, 0x6100 },
    { 0x6100, 0x1100 },
    { 0x3100, 0x4900 },
    { 0x5100, 0x6200 },
};

#define MIU_PAIRS       ((int)(sizeof(miu_pairs) / sizeof(miu_pairs[0])))

// Job ids. Each read job returns (y << 16) | x, the values read.
#define MIU_JOB(setting, pair, dual)    ((((setting) * MIU_PAIRS + (pair)) << 1) | (dual))
#define MIU_READ_JOBS                   (MIU_SETTINGS * MIU_PAIRS * 2)

// These jobs return two MIU registers: (first << 16) | second, with the
// values that the DSP had when it started.
#define JOB_REGS_XYPAGE     (MIU_READ_JOBS + 0) // XPAGE, YPAGE
#define JOB_REGS_PAGECFG    (MIU_READ_JOBS + 1) // PAGE0CFG, PAGE1CFG
#define JOB_REGS_MISC       (MIU_READ_JOBS + 2) // OFFPAGECFG, MISC

_Static_assert(JOB_REGS_MISC <= PROTO_OPERAND_MASK, "Too many jobs");

#endif // MIU_H__
