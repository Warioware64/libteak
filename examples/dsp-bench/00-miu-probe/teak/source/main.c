// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the MIU probe (see common/miu.h).

#include <stdbool.h>
#include <stddef.h>

#include <teak/teak.h>

#include "miu.h"
#include "proto_teak.h"

uint16_t miu_read(uint16_t address);
void miu_fill(uint16_t start, uint16_t count_minus_1);
uint32_t miu_dual(uint16_t misc, uint16_t page0cfg, uint16_t y_address,
                  uint16_t x_address);
uint32_t miu_single(uint16_t misc, uint16_t page0cfg, uint16_t y_address,
                    uint16_t x_address);
void miu_fill_neg(uint16_t misc, uint16_t page0cfg, uint16_t start,
                  uint16_t count_minus_1);

// y0 after miu_dual()
uint16_t miu_dual_y;

// Values of the registers at the start
static uint16_t regs[6];

static uint32_t result;

void app_init(void)
{
    static const uint16_t addresses[6] = {
        MIU_XPAGE, MIU_YPAGE, MIU_PAGE0CFG, MIU_PAGE1CFG, MIU_OFFPAGECFG, MIU_MISC
    };
    for (int i = 0; i < 6; i++)
        regs[i] = miu_read(addresses[i]);

    miu_fill(MIU_FILL_START, 0x8000 - MIU_FILL_START - 1);
}

uint16_t *app_upload_buffer(uint16_t table, uint16_t words)
{
    (void)table;
    (void)words;
    return NULL;
}

// x0 of miu_dual(), from the product x0 * y0 (both are signed)
static uint16_t recover_x(uint32_t product, uint16_t y)
{
    if (y == 0)
        return 0;

    bool negative = false;
    uint32_t p = product;
    if (p & 0x80000000)
    {
        p = -p;
        negative = true;
    }
    uint16_t d = y;
    if (d & 0x8000)
    {
        d = -d;
        negative = !negative;
    }

    // Shift and subtract: the quotient fits in 16 bits
    uint16_t q = 0;
    for (int bit = 15; bit >= 0; bit--)
    {
        uint32_t t = (uint32_t)d << bit;
        if (t <= p)
        {
            p -= t;
            q |= 1u << bit;
        }
    }

    return negative ? (uint16_t)-q : q;
}

static uint32_t run_read(uint16_t job)
{
    const miu_setting *s = &miu_settings[(job >> 1) / MIU_PAIRS];
    const miu_pair *pair = &miu_pairs[(job >> 1) % MIU_PAIRS];

    uint16_t misc = regs[5];
    if (s->zsp >= 0)
        misc = (misc & ~MIU_MISC_ZSP) | (s->zsp ? MIU_MISC_ZSP : 0);
    if (s->page_mode >= 0)
        misc = (misc & ~MIU_MISC_PGM) | (s->page_mode ? MIU_MISC_PGM : 0);

    uint16_t page0cfg = regs[2];
    if (s->page0cfg >= 0)
        page0cfg = (uint16_t)s->page0cfg;

    // Fill again the memory, then the area above the X memory with the
    // setting active
    miu_fill(MIU_FILL_START, 0x8000 - MIU_FILL_START - 1);
    uint16_t x_end = (uint16_t)((page0cfg & 0x3F) << 10);
    if ((x_end >= MIU_FILL_START) && (x_end < 0x8000))
        miu_fill_neg(misc, page0cfg, x_end, 0x8000 - x_end - 1);

    if (job & 1)
    {
        uint32_t p = miu_dual(misc, page0cfg, pair->y, pair->x);
        uint16_t y = miu_dual_y;
        return ((uint32_t)y << 16) | recover_x(p, y);
    }

    return miu_single(misc, page0cfg, pair->y, pair->x);
}

uint16_t app_job(uint16_t job)
{
    if (job < MIU_READ_JOBS)
        result = run_read(job);
    else if (job == JOB_REGS_XYPAGE)
        result = ((uint32_t)regs[0] << 16) | regs[1];
    else if (job == JOB_REGS_PAGECFG)
        result = ((uint32_t)regs[2] << 16) | regs[3];
    else if (job == JOB_REGS_MISC)
        result = ((uint32_t)regs[4] << 16) | regs[5];
    else
        return PROTO_STATUS_BAD_ARG;

    return PROTO_STATUS_OK;
}

uint32_t app_checksum(uint16_t job)
{
    (void)job;
    return result;
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    (void)buffer;
    (void)address;
    return PROTO_STATUS_BAD_CMD;
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    (void)id;
    (void)value;
    return PROTO_STATUS_BAD_CMD;
}
