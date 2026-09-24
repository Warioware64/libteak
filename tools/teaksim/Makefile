# SPDX-License-Identifier: CC0-1.0
#
# teaksim: runs a DSP binary (teak.elf) in the teakra emulator and talks to it
# with the protocol of the dsp-bench examples. It is a host tool used to test
# DSP code without hardware (see ../hostcheck/hostcheck.sh).
#
# It needs the sources of teakra (https://github.com/wwylele/teakra), given by
# TEAKRA_DIR, and CMake to build them.

TEAKRA_DIR	?= ../../../teak-llvm-1/helpSrc/teakra-master
BUILDDIR	:= build

CXX		?= g++
CMAKE		?= cmake

TEAKRA_LIB	:= $(BUILDDIR)/teakra/src/libteakra.a

.PHONY: all clean

all: teaksim

$(TEAKRA_LIB):
	$(CMAKE) -S $(TEAKRA_DIR) -B $(BUILDDIR)/teakra -DCMAKE_BUILD_TYPE=Release \
		-DTEAKRA_BUILD_TOOLS=OFF -DTEAKRA_BUILD_UNIT_TESTS=OFF
	$(CMAKE) --build $(BUILDDIR)/teakra --target teakra

teaksim: teaksim.cpp $(TEAKRA_LIB)
	$(CXX) -O2 -std=c++17 -I$(TEAKRA_DIR)/include -o $@ teaksim.cpp $(TEAKRA_LIB)

clean:
	rm -rf $(BUILDDIR) teaksim
