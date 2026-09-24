// SPDX-License-Identifier: CC0-1.0
//
// Collision kernels. Q3.12 values are kept in [-2, 2) so that no intermediate
// result overflows 32 bits. The Q16.16 versions use the same geometry with 4
// more fractional bits.

#include "collision.h"
#include "fixed.h"

kvec3 vec_a[N_PAIRS], vec_b[N_PAIRS];
ksphere sph_a[N_PAIRS], sph_b[N_PAIRS];
ksphere_q16 sph_a_q16[N_PAIRS], sph_b_q16[N_PAIRS];

int32_t out_dot[N_PAIRS];
int32_t out_cross[N_PAIRS * 3];
uint16_t out_sphere[MASK_WORDS];
uint16_t out_aabb[MASK_WORDS];
int32_t out_dot_q16[N_PAIRS];
uint16_t out_sphere_q16[MASK_WORDS];

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

// Q3.12 value in [-2, 2)
static int16_t rng_coord(void)
{
    return (int16_t)((int16_t)(rng() & 0x3FFF) - 0x2000);
}

// Q3.12 radius in [0, 0.5)
static int16_t rng_radius(void)
{
    return (int16_t)(rng() & 0x7FF);
}

// Q3.12 -> Q16.16, with 4 extra random fractional bits
static int32_t to_q16(int16_t v)
{
    return ((int32_t)v << 4) | (rng() & 0xF);
}

void collision_generate(void)
{
    rng_state = 0x1234;

    for (int i = 0; i < N_PAIRS; i++)
    {
        vec_a[i].x = rng_coord();
        vec_a[i].y = rng_coord();
        vec_a[i].z = rng_coord();

        // Put "b" close to "a" half of the time, so that there are hits
        if (rng() & 1)
        {
            vec_b[i].x = (int16_t)(vec_a[i].x + (int16_t)(rng() & 0x7FF) - 0x400);
            vec_b[i].y = (int16_t)(vec_a[i].y + (int16_t)(rng() & 0x7FF) - 0x400);
            vec_b[i].z = (int16_t)(vec_a[i].z + (int16_t)(rng() & 0x7FF) - 0x400);
        }
        else
        {
            vec_b[i].x = rng_coord();
            vec_b[i].y = rng_coord();
            vec_b[i].z = rng_coord();
        }

        sph_a[i].c = vec_a[i];
        sph_a[i].r = rng_radius();
        sph_b[i].c = vec_b[i];
        sph_b[i].r = rng_radius();

        sph_a_q16[i].x = to_q16(sph_a[i].c.x);
        sph_a_q16[i].y = to_q16(sph_a[i].c.y);
        sph_a_q16[i].z = to_q16(sph_a[i].c.z);
        sph_a_q16[i].r = to_q16(sph_a[i].r);
        sph_b_q16[i].x = to_q16(sph_b[i].c.x);
        sph_b_q16[i].y = to_q16(sph_b[i].c.y);
        sph_b_q16[i].z = to_q16(sph_b[i].c.z);
        sph_b_q16[i].r = to_q16(sph_b[i].r);
    }
}

void k_dot3(const kvec3 *a, const kvec3 *b, int32_t *out, int n)
{
    for (int i = 0; i < n; i++)
    {
        out[i] = (int32_t)a[i].x * b[i].x
               + (int32_t)a[i].y * b[i].y
               + (int32_t)a[i].z * b[i].z;
    }
}

void k_cross3(const kvec3 *a, const kvec3 *b, int32_t *out, int n)
{
    for (int i = 0; i < n; i++)
    {
        out[0] = (int32_t)a[i].y * b[i].z - (int32_t)a[i].z * b[i].y;
        out[1] = (int32_t)a[i].z * b[i].x - (int32_t)a[i].x * b[i].z;
        out[2] = (int32_t)a[i].x * b[i].y - (int32_t)a[i].y * b[i].x;
        out += 3;
    }
}

// Adds one test result to a bitmask (bit i of word i / 16), without variable
// shifts or divisions.
#define MASK_PUSH(hit, word, bit, out)      \
    do {                                    \
        if (hit)                            \
            (word) |= (bit);                \
        (bit) = (uint16_t)((bit) << 1);     \
        if ((bit) == 0)                     \
        {                                   \
            *(out)++ = (word);              \
            (word) = 0;                     \
            (bit) = 1;                      \
        }                                   \
    } while (0)

// Bit i = 1 if |ca - cb|^2 <= (ra + rb)^2
void k_sphere(const ksphere *a, const ksphere *b, uint16_t *mask, int n)
{
    uint16_t word = 0;
    uint16_t bit = 1;

    for (int i = 0; i < n; i++)
    {
        int16_t dx = (int16_t)(a[i].c.x - b[i].c.x);
        int16_t dy = (int16_t)(a[i].c.y - b[i].c.y);
        int16_t dz = (int16_t)(a[i].c.z - b[i].c.z);
        int16_t rs = (int16_t)(a[i].r + b[i].r);

        int32_t d2 = (int32_t)dx * dx + (int32_t)dy * dy + (int32_t)dz * dz;
        int32_t r2 = (int32_t)rs * rs;

        int hit = 0;
        if (d2 <= r2)
            hit = 1;

        MASK_PUSH(hit, word, bit, mask);
    }
}

// Absolute value of the difference of two coordinates
static inline int16_t abs_diff(int16_t a, int16_t b)
{
    int16_t d = (int16_t)(a - b);
    if (d < 0)
        d = (int16_t)-d;
    return d;
}

// Bit i = 1 if the boxes (center and half size) overlap on the three axes
void k_aabb(const ksphere *a, const ksphere *b, uint16_t *mask, int n)
{
    uint16_t word = 0;
    uint16_t bit = 1;

    for (int i = 0; i < n; i++)
    {
        int16_t size = (int16_t)(a[i].r + b[i].r);
        int hit = 1;

        if (abs_diff(a[i].c.x, b[i].c.x) > size)
            hit = 0;
        if (abs_diff(a[i].c.y, b[i].c.y) > size)
            hit = 0;
        if (abs_diff(a[i].c.z, b[i].c.z) > size)
            hit = 0;

        MASK_PUSH(hit, word, bit, mask);
    }
}

void k_dot3_q16(const ksphere_q16 *a, const ksphere_q16 *b, int32_t *out,
                int n)
{
    for (int i = 0; i < n; i++)
    {
        out[i] = q16_mul(a[i].x, b[i].x)
               + q16_mul(a[i].y, b[i].y)
               + q16_mul(a[i].z, b[i].z);
    }
}

void k_sphere_q16(const ksphere_q16 *a, const ksphere_q16 *b, uint16_t *mask,
                  int n)
{
    uint16_t word = 0;
    uint16_t bit = 1;

    for (int i = 0; i < n; i++)
    {
        int32_t dx = a[i].x - b[i].x;
        int32_t dy = a[i].y - b[i].y;
        int32_t dz = a[i].z - b[i].z;
        int32_t rs = a[i].r + b[i].r;

        int32_t d2 = q16_mul(dx, dx) + q16_mul(dy, dy) + q16_mul(dz, dz);
        int32_t r2 = q16_mul(rs, rs);

        int hit = 0;
        if (d2 <= r2)
            hit = 1;

        MASK_PUSH(hit, word, bit, mask);
    }
}

static void add_i32(proto_checksum *c, const int32_t *v, int n)
{
    for (int i = 0; i < n; i++)
        checksum_add32(c, (uint32_t)v[i]);
}

static void add_u16(proto_checksum *c, const uint16_t *v, int n)
{
    for (int i = 0; i < n; i++)
        checksum_add16(c, v[i]);
}

uint32_t collision_checksum(collision_job job)
{
    proto_checksum c;
    checksum_init(&c);

    switch (job)
    {
        case JOB_DOT3_C:
        case JOB_DOT3_ASM:
            add_i32(&c, out_dot, N_PAIRS);
            break;
        case JOB_CROSS3_C:
        case JOB_CROSS3_ASM:
            add_i32(&c, out_cross, N_PAIRS * 3);
            break;
        case JOB_SPHERE_C:
        case JOB_SPHERE_ASM:
            add_u16(&c, out_sphere, MASK_WORDS);
            break;
        case JOB_AABB_C:
        case JOB_AABB_ASM:
            add_u16(&c, out_aabb, MASK_WORDS);
            break;
        case JOB_DOT3_Q16_C:
        case JOB_DOT3_Q16_ASM:
            add_i32(&c, out_dot_q16, N_PAIRS);
            break;
        case JOB_SPHERE_Q16_C:
        case JOB_SPHERE_Q16_ASM:
            add_u16(&c, out_sphere_q16, MASK_WORDS);
            break;
        case JOB_INPUTS:
            for (int i = 0; i < N_PAIRS; i++)
            {
                checksum_add16(&c, (uint16_t)vec_a[i].x);
                checksum_add16(&c, (uint16_t)vec_a[i].y);
                checksum_add16(&c, (uint16_t)vec_a[i].z);
                checksum_add16(&c, (uint16_t)vec_b[i].x);
                checksum_add16(&c, (uint16_t)vec_b[i].y);
                checksum_add16(&c, (uint16_t)vec_b[i].z);
                checksum_add16(&c, (uint16_t)sph_a[i].r);
                checksum_add16(&c, (uint16_t)sph_b[i].r);
                checksum_add32(&c, (uint32_t)sph_a_q16[i].x);
                checksum_add32(&c, (uint32_t)sph_b_q16[i].r);
            }
            break;
        default:
            break;
    }

    return checksum_get(&c);
}
