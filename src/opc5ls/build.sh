#!/bin/bash
python opc5lsasm.py tuberom.s tuberom.hex

cat > tuberom.c <<EOF
#include "tuberom.h"
uint16_t tuberom_opc5ls[0x800] = {
EOF
tr " " "\n" < tuberom.hex | tail -2048 | awk '{print "0x" $1 ","}' >> tuberom.c
cat >> tuberom.c <<EOF
};
EOF

rm -f tuberom.hex
