# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023

# Tools
# -----

CP		:= cp
INSTALL		:= install
MAKE		:= make
RM		:= rm -rf

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Targets
# -------

.PHONY: all check clean docs examples install teak teaktool

all: teak teaktool

teak:
	@+$(MAKE) -f Makefile.teak --no-print-directory
	@+$(MAKE) -f Makefile.teak --no-print-directory DEBUG=1

teaktool:
	@+$(MAKE) -C tools/teaktool --no-print-directory

check: teak
	@+$(MAKE) -f Makefile.teak --no-print-directory check
	@+$(MAKE) -f Makefile.teak --no-print-directory DEBUG=1 check

clean:
	@echo "  CLEAN"
	@$(RM) lib build
	@+$(MAKE) -C tools/teaktool clean --no-print-directory

docs:
	@echo "  DOXYGEN"
	@doxygen Doxyfile

BLOCKSDSEXT	?= /opt/blocksds/external
INSTALLDIR	?= $(BLOCKSDSEXT)/libteak
INSTALLDIR_ABS	:= $(abspath $(INSTALLDIR))

install: all check
	@echo "  INSTALL $(INSTALLDIR_ABS)"
	@test $(INSTALLDIR_ABS)
	$(V)$(RM) $(INSTALLDIR_ABS)
	$(V)$(INSTALL) -d $(INSTALLDIR_ABS)
	$(V)$(CP) -r include lib licenses mk teak.ld $(INSTALLDIR_ABS)
	$(V)$(INSTALL) -d $(INSTALLDIR_ABS)/bin
	$(V)$(INSTALL) -s -m 755 tools/teaktool/teaktool $(INSTALLDIR_ABS)/bin
	$(V)$(CP) tools/teaktool/COPYING.mit $(INSTALLDIR_ABS)/licenses/teaktool-COPYING.mit
	$(V)$(CP) tools/teaktool/COPYING.zlib $(INSTALLDIR_ABS)/licenses/teaktool-COPYING.zlib

# Examples are built against the installed library, toolchain and teaktool
examples:
	@+$(MAKE) -C examples --no-print-directory
