# libteak

## 1. Introduction

This is a library to help develop applications for the DSP that comes with the
DSi and 3DS consoles. It is provided as part of
[BlocksDS](https://blocksds.skylyrac.net), but it can also be used as a
standalone library.

- [Documentation](https://blocksds.skylyrac.net/libteak/index.html)
- [Examples](https://codeberg.org/blocksds/sdk/src/branch/master/examples/dsp)

Please, report issues [here](https://codeberg.org/blocksds/sdk/issues).

## 2. Setup

This fork is built with the Teak LLVM toolchain (clang + lld) from the
teak-llvm fork, not the one from Wonderful Toolchain. Everything is installed
in `$BLOCKSDSEXT` (default: `/opt/blocksds/external`):

```
$BLOCKSDSEXT/
├── llvm-teak/          clang, lld and binutils for the Teak
└── libteak/
    ├── bin/teaktool    ELF to TLF converter
    ├── include/        libteak headers
    ├── lib/            libteak.a, libteakd.a
    ├── mk/Makefile.teak  Makefile for DSP programs
    └── teak.ld         Linker script for DSP programs
```

The toolchain is currently only available on Linux.

Build, test and install the toolchain:

```bash
cd teak-llvm
./build-teak.sh all
```

Build libteak and teaktool, link-test the library and install everything:

```bash
make install
```

- `make` builds `lib/libteak.a`, `lib/libteakd.a` and teaktool.
- `make check` links `tests/smoke` against both libraries and checks the result.
- `make examples` builds the examples in `examples` (after `make install`).
- `LLVM_TEAK_PATH` selects another toolchain (default:
  `$BLOCKSDSEXT/llvm-teak/bin/`), `INSTALLDIR` another install path.

## 3. Using it from a BlocksDS project

Projects keep using the BlocksDS default makefiles for the ARM side. The
`Makefile.teak` of the project includes the one installed with libteak instead
of the one from BlocksDS:

```make
BLOCKSDSEXT ?= /opt/blocksds/external

include $(BLOCKSDSEXT)/libteak/mk/Makefile.teak
```

It accepts the same variables as the BlocksDS one (`SOURCEDIRS`, `INCLUDEDIRS`,
`BINDIRS`, `DEFINES`, `LIBS`, `LIBDIRS`...). See `examples` for complete
projects.

## 4. Assembly files

The assembler of this toolchain evaluates expressions, `.macro`s and symbols
in instruction operands, so the assembly files in `source` are built directly
with the C preprocessor (`-x assembler-with-cpp`) and can use the defines of
`include/teak`. No preprocessing script is needed.

## 5. C code on the DSP

Memory is addressed in 16-bit words. `short` is 16 bits and `int`/`long` are 32
bits (held in the 40-bit accumulators). The multiplier takes 16-bit operands:
a multiplication of two 16-bit values uses one `mpy`, a full 32-bit
multiplication uses three. Division by a constant is done with a
multiplication.

Current limitations of the compiler:

- Functions with more than 4 arguments aren't supported.
- Local arrays and taking the address of local variables aren't supported
  ("Cannot select: FrameIndex"). Use static variables.
- Calls through function pointers aren't supported.
- There is no division by a variable and no 64-bit integer (see
  `examples/dsp-bench/*/common/fixed.c` for 32-bit helpers).

The data memory is 0x0000-0x7FFF words (stack, then .rodata/.data/.bss). The
link fails if the data goes past word 0x8000, where the MMIO registers are.

Hand-written assembly must follow the calling convention of the compiler:
16-bit arguments in `a0l`, `a1l`, `b0l`, `b1l`, 32-bit results in `a0`, and
`y0`, `y1` and `r0`-`r7` preserved. A 32-bit value is stored with its high
half at the lower address. On DSi hardware, an `[r7 + offset]` operand just after
an instruction that computes r7 (`addv`, `modr`...) reads the old value of r7:
put another instruction between them (the compiler does it for C code). Note that the memory forms of `mpy` and `mac`
write `y0`.

Instructions with two memory operands (for example `mac [r4++], [r0++], a0`)
don't work on the DSi: both operands are read from the address of the second
pointer (r0-r3). The MIU starts in "Z single access" mode (MISC = 0x0014,
PAGE0CFG = 0x1E20: all data memory is X memory) and there is no separate Y
memory (see `examples/dsp-bench/00-miu-probe`). Emulators don't model this. Load
one of the operands with a normal `mov` instead (see `examples/benchmark`).

## 6. Tools for testing DSP code without hardware

- `tools/teaksim`: runs a DSP binary (`teak.elf`) in the teakra emulator and
  talks to it with the command protocol of `examples/dsp-bench`. It needs the
  teakra sources (`make -C tools/teaksim TEAKRA_DIR=<teakra>`, by default
  `../teak-llvm-1/helpSrc/teakra-master` next to this repository) and CMake.
- `tools/hostcheck/hostcheck.sh <example> <number of jobs> [stubs.c]
  [setup.txt]`: builds the DSP C code of a dsp-bench example for the host, runs
  every job on the host and in teaksim, and compares the checksums. The stubs
  file gives host versions of the assembly functions, the setup file uploads
  tables and sets parameters first (see the comments of the script).
- `examples/dsp-bench/00-asm-probe/host/gen_expected.sh`: computes the
  results of the assembly snippets in teaksim, which the probe ROM compares with
  the hardware.

The emulator doesn't model everything (DMA between the DSP and ARM9 memory in
melonDS, the MIU, and some differences found by the probe ROMs), so code
should still be tested on hardware.
