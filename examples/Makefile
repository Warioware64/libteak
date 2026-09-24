# SPDX-License-Identifier: CC0-1.0
#
# Builds all examples. They use the Teak LLVM toolchain, libteak and teaktool
# installed in $(BLOCKSDSEXT) (run "make install" in the root of libteak first),
# and BlocksDS from $(BLOCKSDS) for the ARM side.

BLOCKSDSEXT	?= /opt/blocksds/external
LLVM_TEAK_PATH	?= $(BLOCKSDSEXT)/llvm-teak/bin/

MAKE		:= make

EXAMPLES	:= $(dir $(wildcard */Makefile */*/Makefile))

.PHONY: all clean check-toolchain $(EXAMPLES)

all: $(EXAMPLES)

check-toolchain:
	@test -x $(LLVM_TEAK_PATH)clang || \
		{ echo "Teak clang not found in $(LLVM_TEAK_PATH)"; exit 1; }
	@test -f $(BLOCKSDSEXT)/libteak/mk/Makefile.teak || \
		{ echo "libteak not installed in $(BLOCKSDSEXT)/libteak"; exit 1; }

$(EXAMPLES): check-toolchain
	@+$(MAKE) -C $@ --no-print-directory

clean:
	@for i in $(EXAMPLES); do \
		$(MAKE) -C $$i clean --no-print-directory || exit 1; \
	done
