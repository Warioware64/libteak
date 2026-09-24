# DSP workload benchmarks

Each example runs a realistic workload on the ARM9 and on the DSP (Teak) of the
DSi, checks that both give the same result and compares their speed. The same C
source is built for both CPUs; some kernels also have a hand-written version in
DSP assembly (and, in 01, in ARM assembly).

| Example | Workload | DSP assembly |
| --- | --- | --- |
| `00-asm-probe` | Small snippets of DSP assembly, compared with the results of the teakra emulator (`host/gen_expected.sh`). Run it on hardware before trusting a new instruction form. | all |
| `00-miu-probe` | Reads the MIU registers and tests which addresses the two operands of `mpy [r4], [r0]` really read, with several MIU settings. | all |
| `01-collision` | 512 pairs: dot3, cross3, sphere vs sphere and AABB tests (bitmasks), in Q3.12 and Q16.16. The ARM9 also runs `dsp16.s` (SMULxy/SMLAxy). | all kernels |
| `02-skinning` | 256 vertices, 2 of 16 bones (4x3 matrices) each, in Q3.12 and Q16.16 | skin, skin2 (faster), skinQ |
| `03-blur` | 64x64 image: Gaussian [1 4 6 4 1]/16, generic 3x3 convolution. Shown on the top screen. | Gaussian (2 versions), 3x3 convolution |
| `04-fft` | 256-point complex FFT in Q15. Twiddles computed by the ARM9 and sent once. SNR against a float FFT. | butterflies, whole FFT (fft2) |
| `05-dct` | 8x8 DCT + IDCT of a 64x64 image, round-trip error. Shown on the top screen. | 8-point transform, whole image (dct2/idct2) |
| `06-texture` | Animated 64x64 plasma, written by the DSP directly into BG VRAM. Shows the ARM9 time left free. | plasma |
| `07-perlin` | 64x64 gradient noise, 1 octave and 4-octave fBm, in Q8 and Q16.16. Tables sent once. | all kernels |
| `08-math32` | 1024 values: 32-bit add/shift, multiplication, Q16.16 multiplication, division, square root. | all kernels |

## Results on a DSi

ARM9 at 134 MHz, speedup of the DSP assembly version over the ARM9 C version
(Thumb). Results of the DSP C versions are correct but mostly slower than the
ARM9 (0.03x to 1.2x).

| Example | DSP assembly speedup |
| --- | --- |
| 01 | dot3 6.7x, cross3 5.5x, sphere 5.4x, aabb 7.1x, dot3 Q16 6.3x, sphere Q16 5.7x |
| 02 | skin 2.0x, skin2 3.2x, skin Q16 3.4x |
| 03 | 3x3 convolution 3.6x, Gaussian 3.1x |
| 05 | DCT 2.3x, IDCT 1.9x, whole image: DCT 5.9x, IDCT 5.7x |
| 06 | plasma 2.6x (699 us instead of 1840 us per frame, and the ARM9 is free) |
| 07 | noise 3.8x, fBm 3.0x, noise Q16 10.6x, fBm Q16 9.7x |
| 08 | alu 4.3x, mul 5.3x, Q16.16 mul 5.0x, division 2.9x, square root 1.2x |

The whole FFT of 04 (fft2) is correct on hardware. The butterflies of 04
(fft) still give wrong results on hardware only, although the same function
passes the 00-asm-probe snippets: page 5 of 04 compares it with the C version
stage by stage.

The first Gaussian of 03 gave wrong results on hardware because of the r7 +
offset rule below (fixed with a `nop`; page 5 of 03 compares both Gaussians
with the ARM9 pass by pass).

## How they work

Every example is standalone. They share the same small pieces of code, copied
into each one:

- `common/proto.h`, `teak/source/proto_teak.c`, `arm9/source/proto_arm9.c`:
  commands sent through the APBP registers. Every command and reply has a
  sequence number, because on real hardware the DSP can see a command twice and
  the ARM9 can read a reply that isn't new. The argument of a command is
  written before the command itself.
- `arm9/source/ui.c`: pages (Left/Right), and the tables of times, speedups,
  round trips and results.
- `common/fixed.c` (01, 02, 07, 08): Q16.16 multiplication, division and
  square root built from 16-bit operations, with the same results on both
  CPUs.

The inputs are generated on both CPUs with the same generator, or computed by
the ARM9 and uploaded through the command registers (tables of 04, 06, 07). Each
result is checked with a checksum computed by both CPUs. Times are the minimum
of 4 runs: ARM9 time with the ARM9 timers, DSP time counted by the DSP itself
(DSP cycles, 134 MHz).

## Emulators

melonDS doesn't emulate DMA transfers between ARM9 memory and the DSP. The
images of 03, 05 and 07 are then taken from the ARM9, and the DSP texture of 06
stays black. Everything else, including the checks, works in melonDS.

## Writing code for the DSP

See the "C code on the DSP" section of the libteak readme. Rules followed by
these examples:

- At most 4 arguments per function.
- No local arrays, and no address of local variables.
- No calls through function pointers (use a `switch`).
- No division by a variable (use `fixed.c`). Division by a constant works.
- Hand-written assembly: see below.

## DSP assembly

All kernels follow the calling convention of the Teak LLVM toolchain:
16-bit arguments in a0l, a1l, b0l, b1l (at most 4), 32-bit result in a0; y0,
y1 and r0-r7 must be preserved. A 32-bit value is stored in memory with the
high half at the lower address. Symbols are word addresses (`sym + 1` is the
next word), `.space` counts bytes.

Forms that work on hardware (used by kernels that give correct results on a
DSi):

- Multiplications with one memory operand: `mov [rN++], y0` then
  `mpy/mac y0, [rM++], aX`, or with a register as second operand
  (`mpy y0, a1l, a0`, `mac y0, r5, a1`).
- The mixed products `mpysu`, `macsu`, `macus`, `macuu`, and `maa`/`maasu`
  (they add the previous product shifted right by 16). With them, a Q16.16
  multiplication is 4 products and a 32x32 multiplication 3.
- The carry: `add`/`sub` set it, `rol`/`ror` shift it in and out, `copy aX, c`
  and `clr aX, lt/gt` are conditional. Division and square root use it (08).
- `max_ge a0, ^r0` (a0 = max(a0, a1)), `neg aX, lt`, `tstb`, `subh`/`subl`/
  `addh`/`addl` (32-bit values from memory), `shfc` (shift by sv).
- Nested `bkrep` loops, `rep`, a 2-word instruction at the end of a `bkrep`
  block, an address register used just after it is written (`[rN]`,
  `[rN++]`...).
- `[r7 + offset]` operands, after at least one other instruction when r7 was
  computed by `addv` (see below). After `mov imm, r7` they can follow
  directly.

Don't use:

- An `[r7 + offset]` operand just after an instruction that computes r7
  (`addv 8, r7` then `mov [r7-0x3], a0`): on hardware, it reads the old r7.
  Put another instruction (or a `nop`) between them. The compiler does it
  itself (TeakR7HazardPass in the LLVM fork). Found with 00-asm-probe
  (`r7off_after_addv`, shown as "HW": the value seen on hardware).

- `mpy`/`mac` with two memory operands (`[r4], [r0]`): on the DSi, both
  operands are read from the r0 address (00-miu-probe: the MIU is in "Z single
  access" mode, MISC = 0x0014, and there is no separate Y memory: with a
  smaller X size, everything above it reads 0). Clearing the Z single access
  bit or changing the X/Y sizes doesn't change it.

Tools (see the libteak readme): `tools/teaksim` runs a DSP binary in the
teakra emulator, `tools/hostcheck/hostcheck.sh` compares every job of an
example with the same C code built for the host. Every assembly kernel is
checked with it before it runs on hardware (`host/stubs.c` maps the assembly
functions to the C versions for the host build).
