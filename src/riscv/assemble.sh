#!/bin/bash

NAME=tuberom_riscv
PILIB=tuberom_pi


# Assemble the Tube ROM

riscv64-linux-gnu-gcc -O1 -march=rv32im -mabi=ilp32 -static -nostdlib -nostartfiles -T${NAME}.ld ${NAME}.s ${PILIB}.c -o ${NAME}.elf
riscv64-linux-gnu-objcopy -O binary ${NAME}.elf ${NAME}.bin

od -Ax -tx4 -w4 ${NAME}.bin

riscv64-linux-gnu-objdump -d ${NAME}.elf

xxd -i ${NAME}.bin > tuberom.c
rm -f ${NAME}.elf ${NAME}.bin

# Assemble the Standalone Pi Program

NAME=piapp

riscv64-linux-gnu-gcc -O1 -march=rv32im -mabi=ilp32 -static -nostdlib -T${NAME}.ld ${NAME}.c ${PILIB}.c -o ${NAME}.elf
riscv64-linux-gnu-objcopy -O binary ${NAME}.elf ${NAME^^}
#riscv64-linux-gnu-objdump -d ${NAME}.elf
rm -f ${NAME}.ssd
beeb blank_ssd ${NAME}.ssd
beeb putfile ${NAME}.ssd ${NAME^^}
beeb title ${NAME}.ssd ${NAME^^}
beeb info ${NAME}.ssd
rm -f ${NAME}.elf
rm -f ${NAME^^}
