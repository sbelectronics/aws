# Cross-compile C for the Convergent AWS and NGEN with Open Watcom.
#
#   make                  build/libc.lib and build/*.run for each examples/*.c
#   make WATCOM=/path     Open Watcom's install directory (default /opt/watcom)
#
# The programs are 8086 code in one 64 KB segment, linked as a raw binary and
# wrapped as a CTOS version 4 run file by tools/mkrun.py.

WATCOM ?= /opt/watcom
export WATCOM
BIN    ?= $(WATCOM)/binl64
CC      = $(BIN)/wcc
AS      = $(BIN)/wasm
LD      = $(BIN)/wlink
AR      = $(BIN)/wlib
PYTHON ?= python3

# -0 8086 code, -ms small model, -s no stack checks, -zl no default library
# records, -x ignore Watcom's own headers, -os optimise for size
CFLAGS  = -q -0 -ms -s -zl -x -os -wx -we -Iinclude
AFLAGS  = -q -0 -ms

B       = build
LIBOBJ  = $(B)/console.obj $(B)/stdio.obj $(B)/string.obj $(B)/stdlib.obj \
          $(B)/ctos.obj $(B)/i4.obj
EXAMPLES = $(patsubst examples/%.c,$(B)/%.run,$(wildcard examples/*.c))

all: $(EXAMPLES)

$(B):
	mkdir -p $(B)

$(B)/%.obj: lib/%.c lib/console.h $(wildcard include/*.h) | $(B)
	$(CC) $(CFLAGS) -fo=$@ $<

$(B)/%.obj: lib/%.asm | $(B)
	$(AS) $(AFLAGS) -fo=$@ $<

$(B)/%.obj: examples/%.c $(wildcard include/*.h) | $(B)
	$(CC) $(CFLAGS) -fo=$@ $<

$(B)/libc.lib: $(LIBOBJ)
	rm -f $@
	$(AR) -q -n $@ $(addprefix +,$^)

# crt0 must be first: the run file is entered at offset 0
$(B)/%.bin: $(B)/%.obj $(B)/crt0.obj $(B)/libc.lib
	$(LD) option quiet format raw bin option offset=0 option nodefaultlibs \
	    option map=$(B)/$*.map name $@ \
	    order clname CODE clname DATA clname BSS clname STACK \
	    file $(B)/crt0.obj file $< library $(B)/libc.lib

$(B)/%.run: $(B)/%.bin tools/mkrun.py
	$(PYTHON) tools/mkrun.py $< $@

clean:
	rm -rf $(B)

.PRECIOUS: $(B)/%.obj $(B)/%.bin
.PHONY: all clean
