#!/bin/bash

NAME=tuberom_riscv

riscv64-linux-gnu-gcc -march=rv32i -mabi=ilp32 -static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles -T${NAME}.ld ${NAME}.s -o ${NAME}.elf
riscv64-linux-gnu-objcopy -O binary ${NAME}.elf ${NAME}.bin

od -Ax -tx4 -w4 ${NAME}.bin

riscv64-linux-gnu-objdump -d ${NAME}.elf

xxd -i ${NAME}.bin > tuberom.c
rm -f ${NAME}.elf ${NAME}.bin
