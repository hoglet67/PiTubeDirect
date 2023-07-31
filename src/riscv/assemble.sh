#!/bin/bash

NAME=tuberom_riscv
PILIB=tuberom_pi

BASE=riscv32-unknown-elf-
ARCH=rv32im_zicsr
ABI=ilp32

# Assemble the Tube ROM


${BASE}gcc -O1 -march=${ARCH} -mabi=${ABI} -static -nostdlib -nostartfiles -T${NAME}.ld ${NAME}.s ${PILIB}.c -o ${NAME}.elf
${BASE}objcopy -O binary ${NAME}.elf ${NAME}.bin

od -Ax -tx4 -w4 ${NAME}.bin

${BASE}objdump -d ${NAME}.elf

xxd -i ${NAME}.bin > tuberom.c
rm -f ${NAME}.elf ${NAME}.bin

# Assemble the Standalone Pi Program

NAME=piapp

${BASE}gcc -O1 -march=${ARCH} -mabi=${ABI} -static -nostartfiles -T${NAME}.ld ${NAME}.c ${PILIB}.c -o ${NAME}.elf
${BASE}objcopy -O binary ${NAME}.elf ${NAME^^}
#${BASE}objdump -d ${NAME}.elf
rm -f ${NAME}.ssd
beeb blank_ssd ${NAME}.ssd
beeb putfile ${NAME}.ssd ${NAME^^}
beeb title ${NAME}.ssd ${NAME^^}
beeb info ${NAME}.ssd
rm -f ${NAME}.elf
rm -f ${NAME^^}


# Copy bbcbasic
# ${BASE}objcopy -O binary ../../../BBCSDL/console/riscv/bbcbasic bbcbasic_riscv.bin
#xxd -i bbcbasic_riscv.bin > bbcbasic.c
