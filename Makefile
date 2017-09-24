.PHONY: all clean

all: emu

clean:
	rm emu

emu: emu.c
	gcc $< -g -o $@
