// SPDX-License-Identifier: CC0-1.0

#include "fixed.h"
#include "perlin.h"

uint16_t perm[PERM_ENTRIES];
int16_t grad[GRAD_ENTRIES * 2];

uint16_t img_noise[IMG_SIZE];
uint16_t img_fbm[IMG_SIZE];
uint16_t img_noise_q16[IMG_SIZE];
uint16_t img_fbm_q16[IMG_SIZE];

// Size of a cell of the first octave, in pixels
#define CELL_SHIFT      4

static inline uint16_t hash(uint16_t ix, uint16_t iy)
{
    return perm[(perm[ix & 0xFF] + iy) & 0xFF] & (GRAD_ENTRIES - 1);
}

// Q8 version
// ----------

static inline int32_t mul8(int32_t a, int32_t b)
{
    return (a * b) >> 8;
}

// 6t^5 - 15t^4 + 10t^3 for t in [0, 256) (Q8)
static inline int32_t fade8(int32_t t)
{
    int32_t f = t * 6 - 15 * 256;       // 6t - 15
    f = mul8(t, f) + 10 * 256;          // 6t^2 - 15t + 10
    int32_t t3 = mul8(mul8(t, t), t);
    return mul8(t3, f);
}

static inline int32_t grad_dot8(uint16_t h, int32_t fx, int32_t fy)
{
    return mul8(grad[2 * h], fx) + mul8(grad[2 * h + 1], fy);
}

static inline int32_t lerp8(int32_t a, int32_t b, int32_t w)
{
    return a + mul8(b - a, w);
}

// Noise at (x, y) in Q8 (integer part = cell), result about [-256, 256]
static int32_t noise8(int32_t x, int32_t y)
{
    uint16_t ix = (uint16_t)(x >> 8);
    uint16_t iy = (uint16_t)(y >> 8);
    int32_t fx = x & 0xFF;
    int32_t fy = y & 0xFF;

    int32_t n00 = grad_dot8(hash(ix, iy), fx, fy);
    int32_t n10 = grad_dot8(hash(ix + 1, iy), fx - 256, fy);
    int32_t n01 = grad_dot8(hash(ix, iy + 1), fx, fy - 256);
    int32_t n11 = grad_dot8(hash(ix + 1, iy + 1), fx - 256, fy - 256);

    int32_t u = fade8(fx);
    int32_t v = fade8(fy);

    return lerp8(lerp8(n00, n10, u), lerp8(n01, n11, u), v);
}

// Noise (about [-1, 1]) to 12-bit gray: 2048 + v * 2048
static uint16_t clamp_gray(int32_t g)
{
    if (g < 0)
        return 0;
    if (g > IMG_MAX)
        return IMG_MAX;
    return (uint16_t)g;
}

static uint16_t to_gray8(int32_t v)
{
    return clamp_gray(2048 + (v << 3));
}

static uint16_t to_gray16(int32_t v)
{
    return clamp_gray(2048 + (v >> 5));
}

void k_noise(uint16_t *dst, int octaves)
{
    for (int py = 0; py < IMG_H; py++)
    {
        for (int px = 0; px < IMG_W; px++)
        {
            int32_t x = (int32_t)px << (8 - CELL_SHIFT);
            int32_t y = (int32_t)py << (8 - CELL_SHIFT);
            int32_t sum = 0;

            for (int o = 0; o < octaves; o++)
            {
                sum += noise8(x, y) >> o;
                x <<= 1;
                y <<= 1;
            }

            *dst++ = to_gray8(sum);
        }
    }
}

// Q16.16 version
// --------------

static inline int32_t fade16(int32_t t)
{
    int32_t f = t * 6 - 15 * 65536;
    f = q16_mul(t, f) + 10 * 65536;
    int32_t t3 = q16_mul(q16_mul(t, t), t);
    return q16_mul(t3, f);
}

static inline int32_t grad_dot16(uint16_t h, int32_t fx, int32_t fy)
{
    return q16_mul((int32_t)grad[2 * h] << 8, fx)
         + q16_mul((int32_t)grad[2 * h + 1] << 8, fy);
}

static inline int32_t lerp16(int32_t a, int32_t b, int32_t w)
{
    return a + q16_mul(b - a, w);
}

static int32_t noise16(int32_t x, int32_t y)
{
    uint16_t ix = (uint16_t)(x >> 16);
    uint16_t iy = (uint16_t)(y >> 16);
    int32_t fx = x & 0xFFFF;
    int32_t fy = y & 0xFFFF;

    int32_t n00 = grad_dot16(hash(ix, iy), fx, fy);
    int32_t n10 = grad_dot16(hash(ix + 1, iy), fx - 65536, fy);
    int32_t n01 = grad_dot16(hash(ix, iy + 1), fx, fy - 65536);
    int32_t n11 = grad_dot16(hash(ix + 1, iy + 1), fx - 65536, fy - 65536);

    int32_t u = fade16(fx);
    int32_t v = fade16(fy);

    return lerp16(lerp16(n00, n10, u), lerp16(n01, n11, u), v);
}

void k_noise_q16(uint16_t *dst, int octaves)
{
    for (int py = 0; py < IMG_H; py++)
    {
        for (int px = 0; px < IMG_W; px++)
        {
            int32_t x = (int32_t)px << (16 - CELL_SHIFT);
            int32_t y = (int32_t)py << (16 - CELL_SHIFT);
            int32_t sum = 0;

            for (int o = 0; o < octaves; o++)
            {
                sum += noise16(x, y) >> o;
                x <<= 1;
                y <<= 1;
            }

            *dst++ = to_gray16(sum);
        }
    }
}

static void add_image(proto_checksum *c, const uint16_t *img)
{
    for (int i = 0; i < IMG_SIZE; i++)
        checksum_add16(c, img[i]);
}

uint32_t perlin_checksum(perlin_job job)
{
    proto_checksum c;
    checksum_init(&c);

    switch (job)
    {
        case JOB_NOISE_C:
        case JOB_NOISE_ASM:
            add_image(&c, img_noise);
            break;
        case JOB_FBM_C:
        case JOB_FBM_ASM:
            add_image(&c, img_fbm);
            break;
        case JOB_NOISE_Q16_C:
        case JOB_NOISE_Q16_ASM:
            add_image(&c, img_noise_q16);
            break;
        case JOB_FBM_Q16_C:
        case JOB_FBM_Q16_ASM:
            add_image(&c, img_fbm_q16);
            break;
        case JOB_INPUTS:
            for (int i = 0; i < PERM_ENTRIES; i++)
                checksum_add16(&c, perm[i]);
            for (int i = 0; i < GRAD_ENTRIES * 2; i++)
                checksum_add16(&c, (uint16_t)grad[i]);
            break;
        default:
            break;
    }

    return checksum_get(&c);
}
