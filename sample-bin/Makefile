CC := /usr/local/ppc/bin/powerpcle-linux-gcc
OBJDUMP := /usr/local/ppc/bin/powerpcle-linux-objdump

.PHONY: all clean dis

all: test1 test2

clean:
	rm test1 test2

dis:
	$(OBJDUMP) -d test1

test1: test1.c
	$(CC) -nostdlib -e main $< -o $@

test2: test2.c
	$(CC) -nostdlib -e main $< -o $@
