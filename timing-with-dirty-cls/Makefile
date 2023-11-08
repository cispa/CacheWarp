CC ?= gcc
all: wbinvd_bench invd_bench

wbinvd_bench: wbinvd_bench.c r0e.h
	$(CC) wbinvd_bench.c -o wbinvd_bench -g

invd_bench: invd_bench.c
	$(CC) invd_bench.c -o invd_bench -g

clean:
	rm -f wbinvd_bench invd_bench