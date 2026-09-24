// SPDX-License-Identifier: CC0-1.0
//
// Blur kernels. Luminance values are 12-bit, so 16 * value fits in 16 bits.

#include "blur.h"

uint16_t img_src[IMG_SIZE];
uint16_t img_tmp[IMG_SIZE];
uint16_t img_gauss[IMG_SIZE];
uint16_t img_conv[IMG_SIZE];
conv3x3_params conv_params;

static uint16_t rng_state;

static uint16_t rng(void)
{
    uint16_t s = rng_state;
    s ^= (uint16_t)(s << 7);
    s ^= s >> 9;
    s ^= (uint16_t)(s << 8);
    rng_state = s;
    return s;
}

void blur_generate(void)
{
    rng_state = 0xB1B1;

    // Checkerboard of 8x8 squares plus a diagonal gradient and some noise
    for (int y = 0; y < IMG_H; y++)
    {
        for (int x = 0; x < IMG_W; x++)
        {
            uint16_t v = (uint16_t)((x + y) << 5);
            if (((x >> 3) ^ (y >> 3)) & 1)
                v += 1500;
            v += rng() & 0xFF;
            if (v > IMG_MAX)
                v = IMG_MAX;
            img_src[y * IMG_W + x] = v;
        }
    }

    // Sharpening kernel (the sum of the coefficients is 4)
    static const int16_t sharpen[9] = {
        -1, -2, -1,
        -2, 16, -2,
        -1, -2, -1,
    };
    for (int i = 0; i < 9; i++)
        conv_params.k[i] = sharpen[i];
    conv_params.shift = 2;
}

static inline int clamp_coord(int v, int max)
{
    if (v < 0)
        return 0;
    if (v > max)
        return max;
    return v;
}

// (a + 4b + 6c + 4d + e + 8) >> 4, with shifts and additions only
static inline uint16_t gauss_tap(uint16_t a, uint16_t b, uint16_t c,
                                 uint16_t d, uint16_t e)
{
    uint16_t sum = (uint16_t)(a + e + ((b + d) << 2) + (c << 2) + (c << 1) + 8);
    return sum >> 4;
}

void k_gauss5(const uint16_t *src, uint16_t *tmp, uint16_t *dst)
{
    // Horizontal pass
    for (int y = 0; y < IMG_H; y++)
    {
        const uint16_t *row = &src[y * IMG_W];
        uint16_t *out = &tmp[y * IMG_W];

        for (int x = 0; x < IMG_W; x++)
        {
            out[x] = gauss_tap(row[clamp_coord(x - 2, IMG_W - 1)],
                               row[clamp_coord(x - 1, IMG_W - 1)],
                               row[x],
                               row[clamp_coord(x + 1, IMG_W - 1)],
                               row[clamp_coord(x + 2, IMG_W - 1)]);
        }
    }

    // Vertical pass
    for (int y = 0; y < IMG_H; y++)
    {
        const uint16_t *r0 = &tmp[clamp_coord(y - 2, IMG_H - 1) * IMG_W];
        const uint16_t *r1 = &tmp[clamp_coord(y - 1, IMG_H - 1) * IMG_W];
        const uint16_t *r2 = &tmp[y * IMG_W];
        const uint16_t *r3 = &tmp[clamp_coord(y + 1, IMG_H - 1) * IMG_W];
        const uint16_t *r4 = &tmp[clamp_coord(y + 2, IMG_H - 1) * IMG_W];
        uint16_t *out = &dst[y * IMG_W];

        for (int x = 0; x < IMG_W; x++)
            out[x] = gauss_tap(r0[x], r1[x], r2[x], r3[x], r4[x]);
    }
}

void conv3x3_copy_border(const uint16_t *src, uint16_t *dst)
{
    for (int x = 0; x < IMG_W; x++)
    {
        dst[x] = src[x];
        dst[(IMG_H - 1) * IMG_W + x] = src[(IMG_H - 1) * IMG_W + x];
    }
    for (int y = 1; y < IMG_H - 1; y++)
    {
        dst[y * IMG_W] = src[y * IMG_W];
        dst[y * IMG_W + IMG_W - 1] = src[y * IMG_W + IMG_W - 1];
    }
}

void k_conv3x3(const uint16_t *src, uint16_t *dst, const conv3x3_params *p)
{
    conv3x3_copy_border(src, dst);

    for (int y = 1; y < IMG_H - 1; y++)
    {
        for (int x = 1; x < IMG_W - 1; x++)
        {
            const uint16_t *s = &src[(y - 1) * IMG_W + x - 1];
            int32_t acc = 0;

            for (int j = 0; j < 3; j++)
            {
                acc += (int32_t)p->k[3 * j + 0] * (int16_t)s[0];
                acc += (int32_t)p->k[3 * j + 1] * (int16_t)s[1];
                acc += (int32_t)p->k[3 * j + 2] * (int16_t)s[2];
                s += IMG_W;
            }

            acc >>= p->shift;
            if (acc < 0)
                acc = 0;
            if (acc > IMG_MAX)
                acc = IMG_MAX;

            dst[y * IMG_W + x] = (uint16_t)acc;
        }
    }
}

static void add_image(proto_checksum *c, const uint16_t *img)
{
    for (int i = 0; i < IMG_SIZE; i++)
        checksum_add16(c, img[i]);
}

uint32_t blur_checksum(blur_job job)
{
    proto_checksum c;
    checksum_init(&c);

    switch (job)
    {
        case JOB_GAUSS_C:
        case JOB_GAUSS_ASM:
        case JOB_GAUSS_B_ASM:
            add_image(&c, img_gauss);
            break;
        case JOB_CONV_C:
        case JOB_CONV_ASM:
            add_image(&c, img_conv);
            break;
        case JOB_INPUTS:
            add_image(&c, img_src);
            for (int i = 0; i < 9; i++)
                checksum_add16(&c, (uint16_t)conv_params.k[i]);
            checksum_add16(&c, (uint16_t)conv_params.shift);
            break;
        default:
            break;
    }

    return checksum_get(&c);
}
