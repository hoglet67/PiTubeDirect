#!/bin/bash
../tools/vasm/vasmvidcore_std  -Fbin -L tubevc.lst -o tubevc.asm tubevc.s
rm -f ../src/tubevc.c
xxd -i tubevc.asm >> ../src/tubevc.c
