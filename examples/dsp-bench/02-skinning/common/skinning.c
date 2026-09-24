// SPDX-License-Identifier: CC0-1.0
//
// Skinning kernels. Rotation values are in [-1, 1), positions in [-2, 2) and
// translations in [-0.5, 0.5), so a transformed coordinate stays in (-6.5, 6.5)
// and fits in Q3.12 without saturation.

#include "fixed.h"
#include "skinning.h"

kvert verts[N_VERTS];
kbone bones[N_BONES];
kvec3 out_verts[N_VERTS];

kvert_q16 verts_q16[N_VERTS];
kbone_q16 bones_q16[N_BONES];
kvec3_q16 out_verts_q16[N_VERTS];

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

// Q3.12 -> Q16.16, with 4 extra random fractional bits
static int32_t to_q16(int16_t v)
{
    return ((int32_t)v << 4) | (rng() & 0xF);
}

void skinning_generate(void)
{
    rng_state = 0x5EED;

    for (int b = 0; b < N_BONES; b++)
    {
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 3; c++)
                bones[b].m[4 * r + c] = (int16_t)((int16_t)(rng() & 0x1FFF) - 0x1000);
            bones[b].m[4 * r + 3] = (int16_t)((int16_t)(rng() & 0xFFF) - 0x800);
        }

        for (int i = 0; i < 12; i++)
            bones_q16[b].m[i] = to_q16(bones[b].m[i]);
    }

    for (int i = 0; i < N_VERTS; i++)
    {
        verts[i].x = (int16_t)((int16_t)(rng() & 0x3FFF) - 0x2000);
        verts[i].y = (int16_t)((int16_t)(rng() & 0x3FFF) - 0x2000);
        verts[i].z = (int16_t)((int16_t)(rng() & 0x3FFF) - 0x2000);
        verts[i].bones = (uint16_t)((rng() & 0xF) | ((rng() & 0xF) << 8));
        verts[i].w0 = (int16_t)(rng() & 0xFFF);

        verts_q16[i].x = to_q16(verts[i].x);
        verts_q16[i].y = to_q16(verts[i].y);
        verts_q16[i].z = to_q16(verts[i].z);
        verts_q16[i].w0 = (int32_t)verts[i].w0 << 4;
        verts_q16[i].bones = verts[i].bones;
    }
}

// Row of a bone matrix applied to a vertex, Q3.12
static inline int16_t transform_row(const int16_t *row, const kvert *v)
{
    int32_t acc = (int32_t)row[0] * v->x
                + (int32_t)row[1] * v->y
                + (int32_t)row[2] * v->z
                + ((int32_t)row[3] << 12);
    return (int16_t)(acc >> 12);
}

static inline int16_t blend(int16_t w0, int16_t w1, int16_t t0, int16_t t1)
{
    return (int16_t)(((int32_t)w0 * t0 + (int32_t)w1 * t1) >> 12);
}

void k_skin(const kvert *v, const kbone *b, kvec3 *out, int n)
{
    for (int i = 0; i < n; i++)
    {
        const int16_t *m0 = b[v[i].bones & 0xFF].m;
        const int16_t *m1 = b[v[i].bones >> 8].m;
        int16_t w0 = v[i].w0;
        int16_t w1 = (int16_t)(4096 - w0);

        out[i].x = blend(w0, w1, transform_row(m0 + 0, &v[i]), transform_row(m1 + 0, &v[i]));
        out[i].y = blend(w0, w1, transform_row(m0 + 4, &v[i]), transform_row(m1 + 4, &v[i]));
        out[i].z = blend(w0, w1, transform_row(m0 + 8, &v[i]), transform_row(m1 + 8, &v[i]));
    }
}

static inline int32_t transform_row_q16(const int32_t *row, const kvert_q16 *v)
{
    return q16_mul(row[0], v->x) + q16_mul(row[1], v->y)
         + q16_mul(row[2], v->z) + row[3];
}

static inline int32_t blend_q16(int32_t w0, int32_t w1, int32_t t0, int32_t t1)
{
    return q16_mul(w0, t0) + q16_mul(w1, t1);
}

void k_skin_q16(const kvert_q16 *v, const kbone_q16 *b, kvec3_q16 *out, int n)
{
    for (int i = 0; i < n; i++)
    {
        const int32_t *m0 = b[v[i].bones & 0xFF].m;
        const int32_t *m1 = b[v[i].bones >> 8].m;
        int32_t w0 = v[i].w0;
        int32_t w1 = 65536 - w0;

        out[i].x = blend_q16(w0, w1, transform_row_q16(m0 + 0, &v[i]), transform_row_q16(m1 + 0, &v[i]));
        out[i].y = blend_q16(w0, w1, transform_row_q16(m0 + 4, &v[i]), transform_row_q16(m1 + 4, &v[i]));
        out[i].z = blend_q16(w0, w1, transform_row_q16(m0 + 8, &v[i]), transform_row_q16(m1 + 8, &v[i]));
    }
}

uint32_t skinning_checksum(skinning_job job)
{
    proto_checksum c;
    checksum_init(&c);

    switch (job)
    {
        case JOB_SKIN_C:
        case JOB_SKIN_ASM:
        case JOB_SKIN2_ASM:
            for (int i = 0; i < N_VERTS; i++)
            {
                checksum_add16(&c, (uint16_t)out_verts[i].x);
                checksum_add16(&c, (uint16_t)out_verts[i].y);
                checksum_add16(&c, (uint16_t)out_verts[i].z);
            }
            break;
        case JOB_SKIN_Q16_C:
        case JOB_SKIN_Q16_ASM:
            for (int i = 0; i < N_VERTS; i++)
            {
                checksum_add32(&c, (uint32_t)out_verts_q16[i].x);
                checksum_add32(&c, (uint32_t)out_verts_q16[i].y);
                checksum_add32(&c, (uint32_t)out_verts_q16[i].z);
            }
            break;
        case JOB_INPUTS:
            for (int i = 0; i < N_VERTS; i++)
            {
                checksum_add16(&c, (uint16_t)verts[i].x);
                checksum_add16(&c, verts[i].bones);
                checksum_add16(&c, (uint16_t)verts[i].w0);
                checksum_add32(&c, (uint32_t)verts_q16[i].z);
            }
            for (int b = 0; b < N_BONES; b++)
            {
                for (int i = 0; i < 12; i++)
                {
                    checksum_add16(&c, (uint16_t)bones[b].m[i]);
                    checksum_add32(&c, (uint32_t)bones_q16[b].m[i]);
                }
            }
            break;
        default:
            break;
    }

    return checksum_get(&c);
}
