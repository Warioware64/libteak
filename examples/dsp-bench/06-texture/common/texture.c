// SPDX-License-Identifier: CC0-1.0

#include "texture.h"

int16_t sin_table[SIN_ENTRIES];
uint16_t texture[TEX_SIZE];
uint16_t plasma_frame;

// 5-bit color channel from a phase: 16 + sin(phase) * 15
static inline uint16_t channel(uint16_t phase)
{
    return (uint16_t)(16 + (sin_table[phase & 0xFF] >> 11));
}

void k_plasma(uint16_t *dst, uint16_t t)
{
    for (uint16_t y = 0; y < TEX_H; y++)
    {
        int16_t sy = sin_table[(uint16_t)(3 * y - 2 * t) & 0xFF];

        for (uint16_t x = 0; x < TEX_W; x++)
        {
            int16_t sx = sin_table[(uint16_t)(4 * x + t) & 0xFF];
            int16_t sd = sin_table[(uint16_t)(2 * (x + y) + 3 * t) & 0xFF];
            int16_t sm = sin_table[(uint16_t)((sx >> 9) + (sy >> 10) + t) & 0xFF];

            // Sum of 4 values in [-32767, 32767], divided by 4
            int16_t v = (int16_t)((sx >> 2) + (sy >> 2) + (sd >> 2) + (sm >> 2));
            uint16_t phase = (uint16_t)(v >> 8);

            *dst++ = (uint16_t)(channel(phase)
                   | (channel((uint16_t)(phase + 85)) << 5)
                   | (channel((uint16_t)(phase + 170)) << 10)
                   | 0x8000);
        }
    }
}

uint32_t texture_checksum(texture_job job)
{
    (void)job;

    proto_checksum c;
    checksum_init(&c);
    for (int i = 0; i < TEX_SIZE; i++)
        checksum_add16(&c, texture[i]);
    return checksum_get(&c);
}
