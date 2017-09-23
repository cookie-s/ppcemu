
.PHONY: all dis
all: test

dis:
	/usr/local/ppc/bin/powerpcle-linux-objdump -d test

test: test.c
	/usr/local/ppc/bin/powerpcle-linux-gcc -nostdlib -e main test.c -o test
