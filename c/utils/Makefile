# The tools, built with the library in the directory above
CTOS_C = $(abspath ..)
include $(CTOS_C)/ctos.mk

TOOLS = grep find diff sort wc du sum calc sysinfo xmodem

all: $(addsuffix .run,$(TOOLS)) mkcmd.run Install.sub

# installs them as commands: see tools/mksub.py
Install.sub: $(addsuffix .c,$(TOOLS)) $(CTOS_C)/tools/mksub.py
	$(PYTHON) $(CTOS_C)/tools/mksub.py $(addsuffix .c,$(TOOLS)) >$@

$(foreach t,$(TOOLS),$(eval $(t).run: $(t).obj util.obj))
mkcmd.run: mkcmd.obj
$(addsuffix .obj,$(TOOLS)) util.obj: util.h
$(addsuffix .obj,$(TOOLS) util mkcmd): $(wildcard $(CTOS_C)/include/*.h)

clean:
	rm -f *.obj *.bin *.map *.run Install.sub

.PHONY: all clean
