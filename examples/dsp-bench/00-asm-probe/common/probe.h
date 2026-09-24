// SPDX-License-Identifier: CC0-1.0
//
// List of the snippets of the assembly probe. The job id of a snippet is its
// position in this list. PROBE_LIST(X) calls X(name) for each one.

#ifndef PROBE_H__
#define PROBE_H__

#include "proto.h"

#define PROBE_LIST(X)           \
    X(mpy_zero_step)            \
    X(mpy_postinc)              \
    X(mpy_postdec)              \
    X(mac_zero_step)            \
    X(mac_chain)                \
    X(y0_load_use)              \
    X(mpy_add_p_next)           \
    X(mac_into_a1)              \
    X(fft_twiddle)              \
    X(sub_ab_bx)                \
    X(add_ab_bx)                \
    X(sub_mem_bx)               \
    X(shfi_right15)             \
    X(shfi_negative)            \
    X(store_b0l)                \
    X(add_mem_abs)              \
    X(ptr_add_mem)              \
    X(bkrep_last_ptr)           \
    X(bkrep_last_addv)          \
    X(nested_bkrep)             \
    X(rep_mac)                  \
    X(mpysu)                    \
    X(macsu)                    \
    X(macus)                    \
    X(macuu)                    \
    X(maa)                      \
    X(divs)                     \
    X(exp)                      \
    X(clr_cond)                 \
    X(sqr)                      \
    X(shfc_sv)                  \
    X(fft_h1_g1)                \
    X(fft_h1_g2)                \
    X(fft_h2_g1)                \
    X(fft_h1_g4)                \
    X(fft_h4_g2)                \
    X(alu_r7_offset)            \
    X(long_count)               \
    X(modr_after_mov)           \
    X(addv_before_bkrep)        \
    X(r7off_after_addv)         \
    X(gauss_loop)               \
    X(r7off_after_nop)

// Snippets known to give another result on DSi hardware than in teakra, with
// the value seen on hardware. r7off_after_addv: an [r7 + offset] operand just
// after "addv ..., r7" reads the old r7 on hardware.
#define PROBE_KNOWN_HW_LIST(X)                  \
    X(r7off_after_addv, 0x0000369F)

#define PROBE_ENUM(name) PROBE_##name,

typedef enum {
    PROBE_LIST(PROBE_ENUM)
    PROBE_COUNT
} probe_id;

#endif // PROBE_H__
