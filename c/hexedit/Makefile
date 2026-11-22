# hexedit.run, built with the library in the directory above
CTOS_C = $(abspath ..)
include $(CTOS_C)/ctos.mk

all: hexedit.run
hexedit.run: hexedit.obj
hexedit.obj: $(wildcard $(CTOS_C)/include/*.h)

clean:
	rm -f *.obj *.bin *.map *.run

.PHONY: all clean
