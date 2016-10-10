#-------------------------------------------------------------------------
# VideoCore IV implementation of tube handler
#-------------------------------------------------------------------------

# on entry
# GPIO pins setup by arm
# Addresses passed into vc are VC based
# gpfsel_data_idle setup

# r0 pointer to shared memory ( VC address) of tube registers
# r1 pointer to shared memory ( VC address) of gpfsel_data_idle
# r2 pointer to shared memory ( VC address) of tube_mailbox
# r3 pinmap pointer ** to be done)
# r4
# r5 debug pin (0 = no debug  xx= debug pin e.g 1<<21)

# Intenal register allocation
# r7
# r8
# r9
# r10
# r11
# r12
# r13
# r14
# r15

# GPIO registers
.equ GPFSEL0,       0x7e200000
.equ GPSET0_offset, 0x1C
.equ GPCLR0_offset, 0x28
.equ GPLEV0_offset, 0x34

  # pin bit positions
.equ nRST,          4
.equ nTUBE,        17
.equ RnW,          18
.equ A0,           27
.equ A1,            2
.equ A2,            3
.equ CLK,           7

.equ D0D3_shift,    8
.equ D4D7_shift,   22

  # mail box flags
.equ ATTN_MASK,    31
.equ OVERRUN_MASK, 30

.org 0
# code entry point
#  ld r0, (r0)
#  rts
   di
   mov    r9, (0xF<<D0D3_shift) + (0xF<<D4D7_shift) # all the databus
   ld     r10, (r1)    # databus driving signals
   ld     r11, 4(r1)   # databus driving signals
   ld     r12, 8(r1)   # databus driving signals
   ld     r13, 12(r1)  # databus driving signals
   ld     r14, 16(r1)  # databus driving signals
   ld     r15, 20(r1)  # databus driving signals
# r1 is now free
   mov    r6, GPFSEL0

#temploop:
#  st     r5, GPSET0_offset(r6) #DEBUG pin
#  mov    r1, 1000
#delay1:
#  sub    r1, 1
#  cmp    r1, 0
#  bne    delay1
#  st     r5, GPCLR0_offset(r6) #DEBUG pin
#  mov    r1, 1000
#delay2:
#  sub    r1, 1
#  cmp    r1, 0
#  bne    delay2
#  b      temploop

# poll for nTube or RST being low
Poll_loop:
   # use r8 here, as post_mail expects GPIO read data in r8
   ld     r8, GPLEV0_offset(r6)
   btst   r8, nRST
   beq    post_mail
   btst   r8, nTUBE
   bne    Poll_loop
   ld     r7, GPLEV0_offset(r6)  # check ntube again to remove glitches
   btst   r7, nTUBE
   bne    Poll_loop
   # we now know nTube is low
   btst   r7, RnW
   beq    wr_cycle

   # So we are in a ready cycle
   # sort out the address bus
   btst   r7, A0
   and    r7, (1<<A1)+(1<<A2)

   st     r13, (r6)  # Drive data bus

   lsr    r7, 1       # A1 shift
   orne   r7, 1

   st     r14, 4(r6) # Drive data bus

   or     r7, r0
   ldb    r8, (r7)  # Read byte from tube register

   st     r15, 8(r6)  # Drive data bus

   # Sort out the databus
   mov    r7, r8
   lsl    r7, 28      # put lower nibble at the top ( clear upper nibble)
   lsr    r7, 28-D0D3_shift
   lsr    r8, 4
   lsl    r8, D4D7_shift
   or     r8, r7

   st     r8, GPSET0_offset(r6)

   st     r5, GPSET0_offset(r6) #DEBUG pin

#  # spin waiting for clk high
#rd_wait_for_clk_high1:
#  ld     r7, GPLEV0_offset(r6)
#  btst   r7, CLK
#  beq    rd_wait_for_clk_high1

# spin waiting for clk low
rd_wait_for_clk_low:
   ld     r7, GPLEV0_offset(r6)
   mov    r8, r7
   btst   r7, CLK
   bne    rd_wait_for_clk_low

   st     r5, GPCLR0_offset(r6) #DEBUG pin

# stop driving databus
   st     r9, GPCLR0_offset(r6)
   st     r10, (r6)  # Drive data bus
   st     r11, 4(r6)
   st     r12, 8(r6)

# detect dummy read
# spin waiting for clk high
rd_wait_for_clk_high2:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   beq    rd_wait_for_clk_high2
   btst   r7, nTUBE
   bne    rd_not_going_to_write
   btst   r7, RnW
   beq    wr_wait_for_clk_high1
rd_not_going_to_write:
   btst   r8, A0
   bne    post_mail
   b      Poll_loop

wr_cycle:

# spin waiting for clk high
wr_wait_for_clk_high1:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   beq    wr_wait_for_clk_high1

# spin waiting for clk low
wr_wait_for_clk_low:
   mov    r8, r7
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   bne    wr_wait_for_clk_low

#
post_mail:
   and    r8,( 1<<nTUBE+1<<nRST+1<<RnW+1<<A2+1<<A1+1<<A0+(0xF<<D0D3_shift) + (0xF<<D4D7_shift))
   bset   r8, ATTN_MASK
   ld     r7, (r2)  # get mailbox
   btst   r7, ATTN_MASK
   bsetne r8, OVERRUN_MASK
   st     r8, (r2)
   b      Poll_loop


