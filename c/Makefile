# Cross-compile C for the Convergent AWS and NGEN with Open Watcom.
#
#   make                  build/libc.lib and build/X.run for each examples/X.c
#   make lib              just the library and crt0
#   make test             run the examples on the simulators (see tools/simtest.py)
#   make WATCOM=/path     Open Watcom's install directory (default /opt/watcom)
#
# The programs are 8086 code in one 64 KB segment, linked as a raw binary and
# wrapped as a CTOS version 4 run file by tools/mkrun.py. The compile and link
# rules are in ctos.mk, which other directories can include.

CTOS_C   := $(CURDIR)
CTOS_SELF = 1
include ctos.mk

B        = $(CTOS_C)/build
LIBOBJ   = $(addprefix $(B)/,start.obj console.obj file.obj printf.obj scanf.obj \
           screen.obj string.obj stdlib.obj qsort.obj time.obj assert.obj \
           farmem.obj strerror.obj dir.obj stat.obj exec.obj font.obj \
           request.obj serial.obj serio.obj ctos.obj kernel.obj i4.obj setjmp.obj)
EXAMPLES = $(patsubst examples/%.c,$(B)/%.run,$(wildcard examples/*.c))

all: $(EXAMPLES)

lib: $(CTOS_LIB) $(CTOS_CRT0)

$(B):
	mkdir -p $(B)

$(B)/%.obj: lib/%.c lib/libc.h $(wildcard include/*.h) | $(B)
	$(CTOS_CC) $(CTOS_CFLAGS) -fo=$@ $<

$(B)/%.obj: lib/%.asm | $(B)
	$(CTOS_AS) $(CTOS_AFLAGS) -fo=$@ $<

$(B)/%.obj: examples/%.c $(wildcard include/*.h) | $(B)
	$(CTOS_CC) $(CTOS_CFLAGS) -fo=$@ $<

$(CTOS_LIB): $(LIBOBJ)
	rm -f $@
	$(CTOS_AR) -q -n $@ $(addprefix +,$^)

$(B)/%.run: $(B)/%.obj $(CTOS_CRT0) $(CTOS_LIB) tools/mkrun.py
	$(call ctos-link,$@,$<)

test: all
	$(PYTHON) tools/simtest.py

clean:
	rm -rf $(B)

.PRECIOUS: $(B)/%.obj $(B)/%.bin
.PHONY: all lib test clean
