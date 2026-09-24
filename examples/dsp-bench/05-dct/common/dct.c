// SPDX-License-Identifier: CC0-1.0

#include "dct.h"

uint16_t img_src[IMG_SIZE];
int16_t img_coef[IMG_SIZE];
uint16_t img_recon[IMG_SIZE];

// round(s(u) * cos((2x + 1) u pi / 16) * 2^14), s(0) = sqrt(1/8), else 1/2
int16_t dct_matrix[64] = {
      5793,   5793,   5793,   5793,   5793,   5793,   5793,   5793,
      8035,   6811,   4551,   1598,  -1598,  -4551,  -6811,  -8035,
      7568,   3135,  -3135,  -7568,  -7568,  -3135,   3135,   7568,
      6811,  -1598,  -8035,  -4551,   4551,   8035,   1598,  -6811,
      5793,  -5793,  -5793,   5793,   5793,  -5793,  -5793,   5793,
      4551,  -8035,   1598,   6811,  -6811,  -1598,   8035,  -4551,
      3135,  -7568,   7568,  -3135,  -3135,   7568,  -7568,   3135,
      1598,  -4551,   6811,  -8035,   8035,  -6811,   4551,  -1598,
};

int16_t idct_matrix[64];

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

void dct_generate(void)
{
    for (int u = 0; u < 8; u++)
    {
        for (int x = 0; x < 8; x++)
            idct_matrix[8 * x + u] = dct_matrix[8 * u + x];
    }

    // Smooth gradients, a few edges and some noise
    rng_state = 0xDC7;
    for (int y = 0; y < IMG_H; y++)
    {
        for (int x = 0; x < IMG_W; x++)
        {
            int v = 1024 + (x << 4) + (y << 3);
            if ((x > 20) && (x < 44) && (y > 12) && (y < 40))
                v += 1200;
            v += rng() & 0x7F;
            if (v > IMG_MAX)
                v = IMG_MAX;
            img_src[y * IMG_W + x] = (uint16_t)v;
        }
    }
}

void dct8(const int16_t *mat, const int16_t *vec, int16_t *out, uint16_t shift)
{
    int32_t round = (int32_t)1 << (shift - 1);

    for (int u = 0; u < 8; u++)
    {
        int32_t acc = round;
        for (int x = 0; x < 8; x++)
            acc += (int32_t)mat[8 * u + x] * vec[x];
        out[8 * u] = (int16_t)(acc >> shift);
    }
}

#ifdef TEAK
void dct8_asm(const int16_t *mat, const int16_t *vec, int16_t *out,
              uint16_t shift);
#else
#define dct8_asm dct8
#endif

static void run_dct8(dct8_impl impl, const int16_t *mat, const int16_t *vec,
                     int16_t *out, uint16_t shift)
{
    if (impl == DCT8_ASM)
        dct8_asm(mat, vec, out, shift);
    else
        dct8(mat, vec, out, shift);
}

// 2D transform of one block: rows of "in" (stride "in_stride"), stored
// transposed into tmp, then rows of tmp, stored transposed into out (stride 8)
static int16_t tmp[64];

static void block_2d(dct8_impl impl, const int16_t *mat, const int16_t *in,
                     int in_stride, int16_t *out)
{
    for (int r = 0; r < 8; r++)
        run_dct8(impl, mat, &in[r * in_stride], &tmp[r], DCT_SHIFT_1);
    for (int u = 0; u < 8; u++)
        run_dct8(impl, mat, &tmp[8 * u], &out[u], DCT_SHIFT_2);
}

static int16_t block_in[64];
static int16_t block_out[64];

void k_dct_image(dct8_impl impl)
{
    for (int by = 0; by < IMG_H; by += 8)
    {
        for (int bx = 0; bx < IMG_W; bx += 8)
        {
            // Level shift to [-2048, 2047]
            for (int y = 0; y < 8; y++)
            {
                for (int x = 0; x < 8; x++)
                {
                    int v = img_src[(by + y) * IMG_W + bx + x];
                    block_in[8 * y + x] = (int16_t)(v - 2048);
                }
            }

            int16_t *coef = &img_coef[((by >> 3) * 8 + (bx >> 3)) * 64];
            block_2d(impl, dct_matrix, block_in, 8, coef);
        }
    }
}

void k_idct_image(dct8_impl impl)
{
    for (int by = 0; by < IMG_H; by += 8)
    {
        for (int bx = 0; bx < IMG_W; bx += 8)
        {
            const int16_t *coef = &img_coef[((by >> 3) * 8 + (bx >> 3)) * 64];
            block_2d(impl, idct_matrix, coef, 8, block_out);

            for (int y = 0; y < 8; y++)
            {
                for (int x = 0; x < 8; x++)
                {
                    int v = block_out[8 * y + x] + 2048;
                    if (v < 0)
                        v = 0;
                    if (v > IMG_MAX)
                        v = IMG_MAX;
                    img_recon[(by + y) * IMG_W + bx + x] = (uint16_t)v;
                }
            }
        }
    }
}

uint32_t dct_checksum(dct_job job)
{
    proto_checksum c;
    checksum_init(&c);

    switch (job)
    {
        case JOB_DCT_C:
        case JOB_DCT_ASM:
        case JOB_DCT_IMAGE_ASM:
            for (int i = 0; i < IMG_SIZE; i++)
                checksum_add16(&c, (uint16_t)img_coef[i]);
            break;
        case JOB_IDCT_C:
        case JOB_IDCT_ASM:
        case JOB_IDCT_IMAGE_ASM:
            for (int i = 0; i < IMG_SIZE; i++)
                checksum_add16(&c, img_recon[i]);
            break;
        case JOB_INPUTS:
            for (int i = 0; i < IMG_SIZE; i++)
                checksum_add16(&c, img_src[i]);
            for (int i = 0; i < 64; i++)
                checksum_add16(&c, (uint16_t)idct_matrix[i]);
            break;
        default:
            break;
    }

    return checksum_get(&c);
}
