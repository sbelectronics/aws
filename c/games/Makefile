# The games, built with the library in the directory above
CTOS_C = $(abspath ..)
include $(CTOS_C)/ctos.mk

all: chess.run patrol.run
chess.run: chess.obj
patrol.run: patrol.obj
chess.obj patrol.obj: $(wildcard $(CTOS_C)/include/*.h)

# the search is worth the speed: about 16% more positions a second than -os
chess.obj: CFLAGS += -otexan

clean:
	rm -f *.obj *.bin *.map *.run

.PHONY: all clean
