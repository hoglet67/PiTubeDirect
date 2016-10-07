#!/bin/bash
../tools/vasm/vasmvidcore_std  -Fbin -o tubevc.asm tubevc.s
rm -f ../src/tubevc.c
xxd -i tubevc.asm >> ../src/tubevc.c
