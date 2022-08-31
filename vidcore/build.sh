#!/bin/bash
../tools/vasm/vasmvidcore_std -DUSE_DOORBELL=0 -Fbin -L tubevc.lst -o tubevc_mailbox.asm tubevc.s
../tools/vasm/vasmvidcore_std -DUSE_DOORBELL=1 -Fbin -L tubevc.lst -o tubevc_doorbell.asm tubevc.s
rm -f ../src/tubevc.c
xxd -i tubevc_mailbox.asm  >> ../src/tubevc.c
xxd -i tubevc_doorbell.asm >> ../src/tubevc.c
