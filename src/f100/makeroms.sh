# Need to setup your path and PYTHONPATH to the F100Asm

F100Asm.py -f tuberom.S -g hex16 -o tuberom.raw -g hex16 | tee tuberom.lst

awk '\
    /^0x0780:/{print("#include \"tuberom.h\"\nuint16_t tuberom_f100[0x800] = {")}\
    /^0x08..:|^0x09..:|^0x0A..:|^0x0B..:|^0x0C..:|^0x0D..:|^0x0E..:|^0x0F..:/{print($2)}\
    /^0x7E80:/{print("};\nuint16_t tuberom_f100_high[0x100] = {")}\
    /^0x7F..:/{print($2)} \
    /^0x8000:/{print("};")}' tuberom.raw  > tuberom.c
