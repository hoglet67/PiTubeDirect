#!/bin/bash

NAME=tuberom_riscv
PI=tuberom_pi

riscv64-linux-gnu-gcc -O1 -march=rv32im -mabi=ilp32 -static -nostdlib -nostartfiles -T${NAME}.ld ${NAME}.s ${PI}.c -o ${NAME}.elf
riscv64-linux-gnu-objcopy -O binary ${NAME}.elf ${NAME}.bin

od -Ax -tx4 -w4 ${NAME}.bin

riscv64-linux-gnu-objdump -d ${NAME}.elf

xxd -i ${NAME}.bin > tuberom.c
rm -f ${NAME}.elf ${NAME}.bin
