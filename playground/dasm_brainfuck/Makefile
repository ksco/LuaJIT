all:
	gcc -lm -o minilua ../../src/host/minilua.c
	./minilua ../../dynasm/dynasm.lua -o bf.generated.c main.c
	gcc -g -o bf.bin bf.generated.c
	./bf.bin fib.bf

clean:
	rm -rf bf.*
	rm -rf minilua

.PHONY: all clean