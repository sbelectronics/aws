# ctos.mk - build CTOS programs with this library from another directory.
#
#   CTOS_C = /path/to/aws/c
#   include $(CTOS_C)/ctos.mk
#
#   all: eliza.run
#   eliza.run: eliza.obj words.obj
#
# Each .run is linked from the .obj files listed for it, and each .obj is
# compiled from the .c of the same name. The library is built if needed.
# Add your own options with CFLAGS.

WATCOM      ?= /opt/watcom
export WATCOM
OWBIN       ?= $(WATCOM)/binl64
PYTHON      ?= python3

CTOS_CC      = $(OWBIN)/wcc
CTOS_AS      = $(OWBIN)/wasm
CTOS_LD      = $(OWBIN)/wlink
CTOS_AR      = $(OWBIN)/wlib

# -0 8086 code, -ms small model, -s no stack checks, -zl no default library
# records, -x ignore Watcom's own headers, -os optimise for size
CTOS_CFLAGS  = -q -0 -ms -s -zl -x -os -wx -we -I$(CTOS_C)/include
CTOS_AFLAGS  = -q -0 -ms
CTOS_LIB     = $(CTOS_C)/build/libc.lib
CTOS_CRT0    = $(CTOS_C)/build/crt0.obj

# $(call ctos-link,out.run,objects): one segment, linked as a raw binary with
# crt0 first (the run file is entered at offset 0), then made a run file
define ctos-link
$(CTOS_LD) option quiet format raw bin option offset=0 option nodefaultlibs \
    option map=$(basename $1).map name $(basename $1).bin \
    order clname CODE clname DATA clname BSS clname STACK \
    file $(CTOS_CRT0) $(addprefix file ,$2) library $(CTOS_LIB)
$(PYTHON) $(CTOS_C)/tools/mkrun.py $(basename $1).bin $1
endef

# (the library's own Makefile sets CTOS_SELF and has rules of its own)
ifndef CTOS_SELF
%.obj: %.c
	$(CTOS_CC) $(CTOS_CFLAGS) $(CFLAGS) -fo=$@ $<

%.run: $(CTOS_CRT0) $(CTOS_LIB)
	$(call ctos-link,$@,$(filter-out $(CTOS_CRT0),$(filter %.obj,$^)))

# keep the including Makefile's first target as the default
ctos_goal := $(.DEFAULT_GOAL)
$(CTOS_LIB) $(CTOS_CRT0):
	$(MAKE) -C $(CTOS_C) lib
.DEFAULT_GOAL := $(ctos_goal)
endif
