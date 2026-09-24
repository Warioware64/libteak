// SPDX-License-Identifier: Zlib
//
// Copyright (C) 2023 Antonio Niño Díaz

#include <teak/timer.h>

// Copy of the value written to the control register of each timer. The value
// read back from the register can't be used to restore it: on hardware, after
// timerRead() restored it, the counter registers stayed frozen and every read
// returned the same value.
static u16 timer_control[2];

void timerStart(u16 index, u16 config, u32 reload_value)
{
    // Stop timer
    timerStop(index);

    // This value is used as starting value. In mode reload, it is also used as
    // reload value. It is ignored in freerun mode.
    REG_TMR_RELOAD_LO(index) = reload_value & 0xFFFF;
    REG_TMR_RELOAD_HI(index) = reload_value >> 16;

    // The restart bit only has to be written once
    timer_control[index] = config & ~TMR_CONTROL_RESTART;
    REG_TMR_CONTROL(index) = config | TMR_CONTROL_RESTART;
}

void timerStop(u16 index)
{
    timer_control[index] = TMR_CONTROL_PAUSE;
    REG_TMR_CONTROL(index) = TMR_CONTROL_PAUSE;
}

u32 timerRead(u16 index)
{
    u16 control = timer_control[index];

    // Freeze counter. This doesn't stop the counter, it just latches the value
    // of the counter registers. The value is latched when bit 9 changes from 1
    // to 0, so it is set first in case the timer was configured without it.
    // This is required to avoid race conditions when reading from the counter
    // registers.
    REG_TMR_CONTROL(index) = control | TMR_CONTROL_UNFREEZE_COUNTER;
    REG_TMR_CONTROL(index) = control & ~TMR_CONTROL_FREEZE_MASK;

    u32 value = REG_TMR_COUNTER_LO(index);
    value |= ((u32)REG_TMR_COUNTER_HI(index)) << 16;

    REG_TMR_CONTROL(index) = control;

    return value;
}
