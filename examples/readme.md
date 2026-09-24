# libteak examples

Build them with `make examples` from the root of libteak, after `make install`.
The DSP side uses the Teak LLVM toolchain, libteak and teaktool installed in
`$BLOCKSDSEXT`; the ARM side uses BlocksDS from `$BLOCKSDS`.

- `benchmark`: Runs the same calculations on the ARM9 and on the DSP (in C and
  in hand-written assembly), checks that the results match and compares their
  speed. See below.
- `codegen_check`: Runs ~45 small tests of the C compiler (shifts, 32-bit
  arithmetic, multiplication, loops, calls, spills...) on the ARM9 and on the
  DSP, and shows which ones give different results. Run it on real hardware
  to find code generation problems that emulators don't show.
- `dsp-bench`: 8 realistic workloads (collision, skinning, blur, FFT, DCT,
  texture, Perlin noise, 32-bit math) on the ARM9 and on the DSP, in C and in
  DSP assembly, in 16-bit and 32-bit fixed point. See `dsp-bench/readme.md`.
- `dsp`: Examples imported from BlocksDS (see `dsp/readme.md`). Only their
  `Makefile.teak` has been changed, to use this toolchain and library.
  `multiple_binaries` isn't included because it's built with ArchitectDS.

## Benchmark

Kernels, each one run 16 times over 1024 elements:

- `dot16`: dot product of two `int16_t` arrays (multiply-accumulate).
- `sum16`: sum of an `uint16_t` array.
- `xorsh`: 1024 steps of a 16-bit xorshift generator (shifts and xors).

Both CPUs generate the same input data. The results are split in pages (use
Left/Right to change page); the header of every page says if all DSP results
match the ARM9 results:

1. Time measured by each CPU (the DSP counts its own cycles).
2. Speedup of the DSP over the ARM9.
3. Round trip time seen by the ARM9 (including communication).
4. DSP results compared with the ARM9 results, and the DMA check.

Press A to run the benchmark again, and B to switch the ARM9 between 67 MHz and
134 MHz (it also runs it again).

It also checks that the DSP can read the input buffers from ARM9 memory with
DMA. melonDS doesn't emulate those transfers, so this check fails there (the
kernels don't depend on it).

## ARM9 <-> DSP communication

On real hardware the ARM9 can read a reply that isn't new, and the DSP can see
the same command twice, when commands and replies follow each other quickly.
`benchmark` and `codegen_check` add a sequence number to every command and
reply, and discard stale ones. The "Ignored" line shows how many replies and
commands were discarded.

## Emulator notes

- melonDS doesn't emulate ARM9 to DSP DMA transfers: `dma_arm_to_dsp` shows a
  sum of 0 there, even when built with the original toolchain.
- melonDS only supports DSP timers with a prescaler of 1, so `timers` stops the
  emulator with an assertion.
- Timings in emulators aren't accurate. Use real hardware for real numbers.
