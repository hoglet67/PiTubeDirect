	;;  pi program by Bruce Clark as seen on 6502.org
	;;  ported from 65816 to 65Org16 and then to OPC5ls
	;;  see http://forum.6502.org/viewtopic.php?p=15708#p15708

	;; r15 is pc
	;; r14 is stack pointer
	;; r13 is temporary used by JSR

	;; r1  will be used as a was in 65org16
	;; r2  will be used as x was in 65org16
	;; r3  will be used as y was in 65org16

	;; r4 is the stacked value of a
	;; r5 is the stacked value of x
	;; r6 is the stacked value of y

	;; r9  stands for s
	;; r10 stands for r
	;; r11 stands for q
	;; r12 stands for p (a pointer)

MACRO JSR( _address_)
   mov      r13, pc, 0x0005
   sto      r13, r14
   mov      r14, r14, 0xffff
   mov      pc,  r0, _address_
ENDMACRO

MACRO RTS()
    mov     r14, r14, 0x0001
    ld      pc, r14
ENDMACRO

# preamble for a bootable program
# remove this for a monitor-friendly loadable program
	ORG 0
	mov r14, r0, 0x2000
	mov pc, r0, start

# 3 digits in 10431 instructions, 26559 cycles
# 4 digits in 16590 instructions, 42229 cycles
# 5 digits in 23644 instructions, 60186 cycles
# 6 digits in 33412 instructions, 85040 cycles
# 7 digits in 42589 instructions, 108368 cycles
# 8 digits in 52659 instructions, 133981 cycles

	ORG 359 # Original target 359 digits
ndigits:

	ORG 1193 # 1193  should be 1+ndigits*10/3
psize:

	ORG 0x1000
start:
	;; trivial banner
	mov r1, r0, 0x4f
	JSR(oswrch)
	mov r1, r0, 0x6b
	JSR(oswrch)
	mov r1, r0, 0x20
	JSR(oswrch)
	

	;; mov r14, r0, stack	; initialise stack pointer
	JSR( init)
	mov r2, r0, ndigits	# ldx #359
	mov r3, r0, psize	# ldy #1193
l1:
	mov r6, r3 		# phy
	mov r4, r1 		# pha
	mov r5, r2 		# phx
	mov r11, r0		# stz q
	mov r1, r3		# tya
	mov r2, r1		# tax

l2:	mov r1, r2		# txa
	JSR (mul)
	mov r9, r1		# sta s
	mov r1, r0, 10		# lda #10
	mov r11, r1		# sta q
	ld  r1, r2, p-1		# lda p-1,x
	JSR (mul)
				# clc
	add r1, r9		# adc s
	mov r11, r1		# sta q
	mov r1, r2		# txa
	add r1, r1		# asl
	mov r1, r1, -1		# dec
	JSR (div)
	sto r1, r2, p-1		# sta p-1,x
	mov r2, r2, -1		# dex
	nz.mov pc, r0, l2	# bne l2

	mov r1, r0, 10		# lda #10
	JSR (div)
	sto r1, r0, p		# sta p
	mov r2, r5 		# plx
	mov r1, r4 		# pla
	mov r3, r11		# ldy q
	cmp r3, r0, 10		# cpy #10
	nc.mov pc, r0, l3	# bcc l3
	mov r3, r0		# ldy #0
	mov r1, r1, 1		# inc
l3:
	cmp r2, r0, ndigits-1	# cpx #358
	nc.mov pc, r0, l4	# bcc l4
	nz.mov pc, r0, l5	# bne l5
	JSR (oswrch)
	mov r1, r0, 46		# lda #46
l4:
	JSR (oswrch)
l5:	mov r1, r3		# tya
	xor r1, r0, 48		# eor #48
	mov r3, r6 		# ply
	cmp r2, r0, ndigits-1	# cpx #358
	c.mov pc, r0, l6	# bcs l6
				# dey
				# dey
	mov r3, r3, -3		# dey by 3
l6:	mov r2, r2, -1		# dex
	nz.mov pc, r0, l1	# bne l1
	JSR (oswrch)
 	mov r0, r0, 3142	# RTS()
	RTS()
done:	mov pc, r0, done

init:
	mov r1, r0, 2		# lda #2
	mov r2, r0, psize	# was ldx #1192
i1:	sto r1, r2, p-1		# was sta p,x
	mov r2, r2, -1		# dex
	nz.mov pc, r0, i1	# bne instead of bpl i1
	RTS()

mul:				# uses y as loop counter
	mov r10, r1		# sta r
	mov r3, r0, 16		# ldy #16
m1:	add r1, r1		# asl
	add r11, r11		# asl q
	nc.mov pc, r0, m2	# bcc m2
				# clc
	add r1, r10		# adc r
m2:	mov r3, r3, -1		# dey
	nz.mov pc, r0, m1	# bne m1
	RTS()

div:				# uses y as loop counter
	mov r10, r1		# sta r
	mov r3, r0, 16		# ldy #16
	mov r1, r0, 0		# lda #0
	add r11, r11		# asl q
d1:	adc r1, r1		# rol
	cmp r1, r10		# cmp r
	nc.mov pc, r0, d2	# bcc d2
	sbc r1, r10		# sbc r
d2:	adc r11, r11		# rol q
	mov r3, r3, -1		# dey
	nz.mov pc, r0, d1	# bne d1
	RTS()

# --------------------------------------------------------------
#
# oswrch
#
# output a single ascii character to the uart
#
# entry:
# - r1 is the character to output
#
# exit:
# - r13 used as temporary

oswrch:
oswrch_loop:
    ld      r13, r0, 0xfe08
    and     r13, r0, 0x8000
    nz.mov  pc, r0, oswrch_loop
    sto     r1, r0, 0xfe09
    RTS     ()

base:   WORD 0,0,0,0,0,0,0,0,0  # reserve some stack space
stack:	WORD 0
p:	WORD 0  # needs 1193 words but there's nothing beyond

