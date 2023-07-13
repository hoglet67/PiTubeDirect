   .equ        TUBE, 0x00FFFFE0
   .equ       STACK, 0x000FFFFC

   .equ    r1status, 0
   .equ      r1data, 4
   .equ    r2status, 8
   .equ      r2data, 12
   .equ    r3status, 16
   .equ      r3data, 20
   .equ    r4status, 24
   .equ      r4data, 28

   .globl _start

   .section .text

_start:

Reset:
   li    sp, STACK
   li    gp, TUBE

   la    a1, BannerMessage
BannerLoop:
   lb    a0, (a1)
   jal   ra, do_wrch
   addi  a1, a1, 1
   bne   a0, zero, BannerLoop

CliLoop:

   j     CliLoop


do_wrch:
   lw    t0, r1status(gp)
   andi  t0, t0, 0x40
   beq   t0, zero, do_wrch
   sw    a0, r1data(gp)
   ret

# -----------------------------------------------------------------------------
# Messages
# -----------------------------------------------------------------------------

BannerMessage:
   .string "\nRISCV Co Processor\n\n\r"
