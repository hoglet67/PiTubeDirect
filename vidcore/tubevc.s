#-------------------------------------------------------------------------
# VideoCore IV implementation of tube handler
#-------------------------------------------------------------------------

# on entry
# GPIO pins setup by arm
# Addresses passed into vc are VC based
# gpfsel_data_idle setup

#  r0 - pointer to shared memory ( VC address) of tube registers
#  r1 - LED_PIN
#  r2 - tube_delay
#  r3 - GPIO mapping of address lines - 0x00<A2><A1><A0> where A2, A1 and A0 are the numbers of the respective GPIOs
#  r4 - unused
#  r5 - debug pin mask (0 = no debug  xx= debug pin e.g 1<<21)

# Internal register allocation
#  r0 - pointer to shared memory ( VC address) of tube registers
#  r1 - scratch
#  r2 - tube_delay
#  r3 - ARM_GPU_MAILBOX constant
#  r4 - scratch
#  r5 - debug pin mask (0 = no debug  xx= debug pin e.g 1<<21)
#  r6 - GPFSEL0 constant
#  r7 - scratch register
#  r8 - A1 GPIO bit number
#  r9 - A2 GPIO bit number
# r10 - LED BIT
# r11 - LED address
# r12 - LED Toggle number
# r13 - A0 GPIO bit number
# r14 - data bus driving values
# r15 - data bus driving values
# r16 - data bus driving values
# r17 - data bus driving values idle
# r18 - data bus driving values idle
# r19 - data bus driving values idle

# r22 - doorbell

# r25 - (SP)
# r26 - (LR)

# GPIO registers
.equ GPFSEL0,       0x7e200000
.equ GPSET0_offset, 0x1C
.equ GPSET1_offset, 0x20

.equ GPCLR0_offset, 0x28
.equ GPCLR1_offset, 0x2C

.equ GPLEV0_offset, 0x34
.equ GPEDS0_offset, 0x40

.equ GPU_ARM_MBOX, 0x7E00B880
.equ GPU_ARM_DBELL, 0x7E00B844       # Doorbell1
.equ GPU_ARM_DBELLDATA, 0x7E20C014   # Hijack PWM_DAT1 for Doorbel1 Data

# fixed pin bit positions (A2..0, TEST passed in dynamically)
.equ nRST,          4
.equ nTUBE,        17
.equ RnW,          18
.equ CLK,           7
.equ D0D3_shift,    8
.equ D4D7_shift,   22

.equ D7_PIN,       (25)     # C2
.equ D6_PIN,       (24)     # C2
.equ D5_PIN,       (23)     # C2
.equ D4_PIN,       (22)     # C2
.equ D3_PIN,       (11)     # C1
.equ D2_PIN,       (10)     # C1
.equ D1_PIN,       (9)      # C0
.equ D0_PIN,       (8)      # C0

.equ MAGIC_C0,     ((1 << ( D0_PIN       * 3)) | (1 << ( D1_PIN       * 3)))
.equ MAGIC_C1,     ((1 << ((D2_PIN - 10) * 3)) | (1 << ((D3_PIN - 10) * 3)))
.equ MAGIC_C2,     ((1 << ((D4_PIN - 20) * 3)) | (1 << ((D5_PIN - 20) * 3)))
.equ MAGIC_C3,     ((1 << ((D6_PIN - 20) * 3)) | (1 << ((D7_PIN - 20) * 3)))

  # mail box flags
.equ ATTN_MASK,    31
.equ OVERRUN_MASK, 30

.equ RESET_MAILBOX_BIT, 12
.equ RW_MAILBOX_BIT, 11


.macro TOGGLE_LED
   st     r10,(r11)
   eor    r11,r12
.endm

.org 0
   di
   mov    r6, GPFSEL0

   mov    r7, (0xF<<D0D3_shift) + (0xF<<D4D7_shift) # all the databus
   st     r7, GPCLR0_offset(r6) # clear databus

   lsr    r13, r3, 0  # Extract GPIO number of A0
   and    r13, 31
   lsr    r8, r3, 8   # Extract GPIO number of A1
   and    r8, 31
   lsr    r9, r3, 16  # Extract GPIO number of A2
   and    r9, 31

.if USE_DOORBELL
   mov    r3, GPU_ARM_DBELL
   mov    r22, GPU_ARM_DBELLDATA
.else
   mov    r3, GPU_ARM_MBOX
.endif

   # read GPIO function registers to capture the bus idle state

   ld     r17,  (r6)
   ld     r18, 4(r6)
   ld     r19, 8(r6)
   # setup the databus drive states
   mov    r14, MAGIC_C0
   mov    r15, MAGIC_C1
   mov    r16, MAGIC_C2 | MAGIC_C3
   or     r14, r17
   or     r15, r18
   or     r16, r19

# setup the LED toggling

   add    r11,r6,GPSET0_offset

# R1 LED pin 0-53
# 0 - 31 GPIO bank 0
# 32 - 63 GPIO bank 1
# >63 no led defined.

   cmp   r1,31
   ble   led_bank0
   sub   r1,32
# led bank 1 or LED_PIN to to great and R10 becomes 0 and so has not effect
   add   r11,GPSET1_offset-GPSET0_offset  # 4

led_bank0:
   mov   r10,1
   lsl   r10,r1
no_led:
   add   r12,r11,GPCLR0_offset-GPSET0_offset  # 12
   eor   r12,r11

   rsb   r2, 40
   B     Poll_loop

# Post a message to indicate a reset
post_reset:
   mov    r7, 1<<RESET_MAILBOX_BIT
.if USE_DOORBELL
   st     r7, (r22)             # store in register we are using for doorbell data
.endif
   st     r7, (r3)
   TOGGLE_LED
# Wait for reset to be released (so we don't overflow the mailbox)
post_reset_loop:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, nRST
   beq    post_reset_loop
   b      Poll_loop

# poll for nTube being low ( aligned to word boundary
.align 2
Poll_loop:

Poll_tube_low:
   ld     r1, GPLEV0_offset(r6)
# hide some code in the stall
   mov    r4, r2
   mov    r7, r0
   btst   r1, nRST
   beq    post_reset
   btst   r1, nTUBE
   bne    Poll_tube_low

.rept 40
   addcmpbeq r4,1,41,delay_done
.endr

delay_done:
   ld     r1, GPLEV0_offset(r6)  # check ntube again to remove glitches

   # sort out the address bus

   btst   r1, r13
   orne   r7, 4
   btst   r1, r8
   orne   r7, 8
   btst   r1, r9
   orne   r7, 16

   ld     r4, (r7)               # Read word from tube register
   btst   r1, nTUBE
   bne    Poll_loop
   btst   r1, RnW
   beq    wr_cycle

   # So we are in a read cycle

   st     r14, (r6)              # Drive data bus
   st     r15, 4(r6)             # Drive data bus
   st     r16, 8(r6)             # Drive data bus
   or     r4, r5				      # add debug pin
   st     r4, GPSET0_offset(r6)  # Write word to data bus

  # spin waiting for clk high
rd_wait_for_clk_high1:
   ld     r1, GPLEV0_offset(r6)
   btst   r1, CLK
   beq    rd_wait_for_clk_high1
# we now have half a cycle to do post mail

   btst   r7, 2                 # no need to post mail if A0 = 0
   beq    rd_wait_for_clk_low
   sub    r7, r0                # just get the address bits
   lsl    r7, 6                 # put address bits in correct place

   bset   r7, RW_MAILBOX_BIT    # set read bit

.if USE_DOORBELL
   st     r7, (r22)             # store in register we are using for doorbell data
.endif
   st     r7, (r3)              # store in mail box
   TOGGLE_LED

# spin waiting for clk low
rd_wait_for_clk_low:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   bne    rd_wait_for_clk_low

# stop driving databus
   st     r17, (r6)  # Stop Driving data bus
   st     r18, 4(r6)
   st     r19, 8(r6)
   st     r4, GPCLR0_offset(r6) # clear databus ready for next time and debug pin

# detect dummy read
# spin waiting for clk high
rd_wait_for_clk_high2:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   beq    rd_wait_for_clk_high2
   btst   r7, nTUBE
   bne    Poll_loop
   btst   r7, RnW
   bne    Poll_loop

wr_cycle:

# spin waiting for clk high
wr_wait_for_clk_high1:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   beq    wr_wait_for_clk_high1

# spin waiting for clk low
wr_wait_for_clk_low:
   mov    r1, r7
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   bne    wr_wait_for_clk_low

# Post a message to indicate a tube register write

#  move databus to correct position to D23..D16
   lsl    r7,r1, 16 - D0D3_shift
   lsr    r4,r1, D4D7_shift - 20
   and    r7, 0x000F0000
   and    r4, 0x00F00000
   or     r7, r4

#  move address bit to correct position
   btst   r1, r13
   bsetne r7, 8
   btst   r1, r8
   bsetne r7, 9
   btst   r1, r9
   bsetne r7, 10

# check clock is still low this filters out clock glitches
   ld     r1, GPLEV0_offset(r6)
   btst   r1, CLK
   bne    wr_wait_for_clk_low

.if USE_DOORBELL
   st     r7, (r22)             # store in register we are using for doorbell data
.endif
   st     r7, (r3)               # post mail
   TOGGLE_LED
   b      Poll_loop
