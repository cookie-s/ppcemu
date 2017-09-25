CC := /usr/local/ppc/bin/powerpcle-linux-gcc
OBJDUMP := /usr/local/ppc/bin/powerpcle-linux-objdump

.PHONY: all clean dis

all: test1 test2

clean:
	rm test1 test2

dis:
	$(OBJDUMP) -d test1

test1: test1.c
	$(CC) -nostdlib $< -o $@

test2: test2.c
	$(CC) -nostdlib $< /usr/local/ppc/lib/crt1.o -o $@
