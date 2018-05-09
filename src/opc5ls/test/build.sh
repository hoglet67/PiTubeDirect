#!/bin/bash

PATH=${PATH}:~/atom/MMFS/tools/mmb_utils

mkdir -p tmp
python ../opc5lsasm.py pi-spigot-bruce.s pi-spigot-bruce.hex
cat pi-spigot-bruce.hex | tr " " "\n" | tail -n +4097 | head -256 | cut -c1,2 > c2
cat pi-spigot-bruce.hex | tr " " "\n" | tail -n +4097 | head -256 | cut -c3,4 > c1
paste c1 c2 | tr -d "\t" | xxd -r -p > tmp/PI
cat >> tmp/PI.inf <<EOF
$.PI       1000  1000 CRC=F884
EOF
rm -f opc5ls.ssd
blank_ssd.pl opc5ls.ssd
putfile.pl opc5ls.ssd tmp/*
dkill.pl -f /media/dmb/EBF2-1C46/BEEB.MMB 255
dput_ssd.pl -f /media/dmb/EBF2-1C46/BEEB.MMB 255 opc5ls.ssd

rm -f c1 c2 pi-spigot-bruce.hex tmp/*
rmdir tmp
