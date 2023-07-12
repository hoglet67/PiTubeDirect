#!/bin/bash

NAME=tuberom_riscv

riscv64-linux-gnu-as -march=rv32i ${NAME}.s -o ${NAME}.elf
riscv64-linux-gnu-objcopy -O binary ${NAME}.elf ${NAME}.bin

riscv64-linux-gnu-objdump -d ${NAME}.elf

xxd -i ${NAME}.bin > tuberom.c
rm -f ${NAME}.elf ${NAME}.bin
