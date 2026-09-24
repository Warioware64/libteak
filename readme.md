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
a multiplication of two 16-bit values (for example `(int32_t)a * b` with
`int16_t a, b`) uses one `mpy`, a full 32-bit multiplication uses three.

Current limitations of the compiler:

- Local arrays and taking the address of local variables aren't supported
  (the compiler stops with "Cannot select: FrameIndex"). Use static variables.
- There is no division.

Hand-written assembly must follow the calling convention of the compiler:
16-bit arguments in `a0l`, `a1l`, `b0l`, `b1l`, 32-bit results in `a0`, and
`y0`, `y1` and `r0`-`r7` preserved. Note that the memory forms of `mpy` and
`mac` write `y0`.

Instructions with two memory operands (for example `mac [r4++], [r0++], a0`)
read the second operand through the Y data bus. On DSi hardware that doesn't
read the same memory as normal accesses (it depends on the X/Y memory setup of
the MIU, which emulators don't model), so load one of the operands with a
normal `mov` instead (see `examples/benchmark`).
