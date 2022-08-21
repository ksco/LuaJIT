#!/bin/sh

gcc -o minilua ../../src/host/minilua.c
./minilua ../../dynasm/dynasm.lua -o bf.generated.c bf.c
gcc -o bf bf.generated.c
./bf fib.bf
