#-------------------------------------------------------------------------
# VideoCore IV implementation of tube handler
#-------------------------------------------------------------------------

# on entry
# GPIO pins setup by arm
# Addresses passed into vc are VC based
# gpfsel_data_idle setup

#  r0 - pointer to shared memory ( VC address) of tube registers
#  r1 - LEDTYPE
#  r2 - tube_delay
#  r3 - GPIO mapping of address lines - 0x00<A2><A1><A0> where A2, A1 and A0 are the numbers of the respective GPIOs
#  r4 - unused
#  r5 - debug pin mask (0 = no debug  xx= debug pin e.g 1<<21)

# Intenal register allocation
#  r0 - pointer to shared memory ( VC address) of tube registers
#  r1 - unused
#  r2 - unused
#  r3 - ARM_GPU_MAILBOX constant
#  r4 - scratch
#  r5 - debug pin mask (0 = no debug  xx= debug pin e.g 1<<21)
#  r6 - GPFSEL0 constant
#  r7 - scratch register
#  r8 - scratch register
#  r9 - (0xF<<D0D3_shift) + (0xF<<D4D7_shift) constant
# r10 - data bus driving values idle
# r11 - data bus driving values idle
# r12 - data bus driving values idle
# r13 - data bus driving values
# r14 - data bus driving values
# r15 - data bus driving values
# r16 - A0 GPIO bit number
# r17 - A1 GPIO bit number
# r18 - A2 GPIO bit number
# r19 - LEDTYPE
# r20 - led_state

# GPIO registers
.equ GPFSEL0,       0x7e200000
.equ GPSET0_offset, 0x1C
.equ GPSET1_offset, 0x20

.equ GPCLR0_offset, 0x28
.equ GPCLR1_offset, 0x2C

.equ GPLEV0_offset, 0x34
.equ GPEDS0_offset, 0x40

# LED type 0 is GPIO 16
.equ LED_TYPE0_BIT, 16

# LED type 1 is GPIO 47 (15 and use SEL1)
.equ LED_TYPE1_BIT, 15

# LED type 2 means no LED supported (Pi 3)

# LED type 3 is GPIO 29
.equ LED_TYPE3_BIT, 29

# LED type 4 is GPIO 42 (10 and use SEL1)
.equ LED_TYPE4_BIT, 10

.equ GPU_ARM_MBOX, 0x7E00B880

#.equ IC0_MASK,     0x7e002010
#.equ IC1_MASK,     0x7e002810

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

.equ MAGIC_C0,     ((1 << (D0_PIN * 3)) | (1 << (D1_PIN * 3)))
.equ MAGIC_C1,     ((1 << ((D2_PIN - 10) * 3)) | (1 << ((D3_PIN - 10) * 3)))
.equ MAGIC_C2,     ((1 << ((D4_PIN - 20) * 3)) | (1 << ((D5_PIN - 20) * 3)))
.equ MAGIC_C3,     ((1 << ((D6_PIN - 20) * 3)) | (1 << ((D7_PIN - 20) * 3)))

  # mail box flags
.equ ATTN_MASK,    31
.equ OVERRUN_MASK, 30

.equ RESET_MAILBOX_BIT, 12
.equ RW_MAILBOX_BIT, 11

.org 0

# code entry point
#  ld r0, (r0)
#  rts

# disable interrupts
  di

# Setup interrupt vector
# 0x1EC01E00 looks like the interrupt table
# Vector 113 = 49 + 64 = GPIO0

#  mov r8, (0x40000000 + 0x1EC01E00 + (113 << 2))
#  lea r9, irq_handler(pc)
#  st  r9, (r8)

# mask ARM interrupts
#  mov r8, IC0_MASK
#  mov r9, IC1_MASK
#  mov r10, 0x0
#  mov r11, 8
#mask_all:
#  st r10, (r8)
#  st r10, (r9)
#  add r8, 4
#  add r9, 4
#  sub r11, 1
#  cmp r11, 0
#  bne mask_all

# enable the interupt (49 + 64 = 113)
#  mov r8, IC0_MASK + 0x18
#  mov r9, 0x00000010
#  st  r9, (r8)

   mov    r19, r1      # save LEDTYPE
   mov    r20, 0       # clear led_state
   mov    r9, (0xF<<D0D3_shift) + (0xF<<D4D7_shift) # all the databus
   or     r9, r5       # add in test pin so that it is cleared at the end of the access
   lsr    r16, r3, 0   # Extract GPIO number of A0
   and    r16, 31
   lsr    r17, r3, 8   # Extract GPIO number of A1
   and    r17, 31
   lsr    r18, r3, 16  # Extract GPIO number of A2
   and    r18, 31



# r1, r3, r4 now free

   mov    r3, GPU_ARM_MBOX

   mov    r6, GPFSEL0

   # read GPIO registers to capture the bus idle state

   ld     r10,  (r6)
   ld     r11, 4(r6)
   ld     r12, 8(r6)
   # setup the databus drive states
   mov    r13, MAGIC_C0
   mov    r14, MAGIC_C1
   mov    r15, MAGIC_C2 | MAGIC_C3
   or     r13, r10
   or     r14, r11
   or     r15, r12

# enable interrupts
#  ei
   rsb    r2, 40
   nop           #nop to align loop for speed
# poll for nTube being low
Poll_loop:
   mov    r1, r2
   mov    r7, r0
Poll_tube_low:
   ld     r8, GPLEV0_offset(r6)
   btst   r8, nRST
   beq    post_reset
   btst   r8, nTUBE
   bne    Poll_tube_low

.rept 40
   addcmpbeq r1,1,41,delay_done
.endr

delay_done:
   ld     r8, GPLEV0_offset(r6)  # check ntube again to remove glitches

   # we now know nTube is low
 # Extra read with might possible help with rnw on slow machines
 #  ld     r8, GPLEV0_offset(r6)  # read bus again to ensure we have the correct address

   # sort out the address bus

   btst   r8, r16

   orne   r7, 4
   btst   r8, r17

   orne   r7, 8
   btst   r8, r18

   orne   r7, 16
   ld     r4, (r7)               # Read word from tube register
   btst   r8, nTUBE
   bne    Poll_loop
   btst   r8, RnW
   beq    wr_cycle

   # So we are in a read cycle

   st     r13, (r6)              # Drive data bus
   st     r14, 4(r6)             # Drive data bus
   st     r15, 8(r6)             # Drive data bus
   or     r4,r5
   st     r4, GPSET0_offset(r6)  # Write word to data bus

   #st     r5, GPSET0_offset(r6)  # DEBUG pin

  # spin waiting for clk high
rd_wait_for_clk_high1:
   ld     r8, GPLEV0_offset(r6)
   btst   r8, CLK
   beq    rd_wait_for_clk_high1
# we now have half a cycle to do post mail
   btst   r7, 2               # no need to post mail if A0 = 0
   beq    rd_wait_for_clk_low
   sub    r7, r0                # just get the address bits
   lsl    r7, 6                 # put address bits in correct place
   bset   r7, RW_MAILBOX_BIT    # set read bit
   st     r7, (r3)              # store in mail box
   bl     toggle_led

# spin waiting for clk low
rd_wait_for_clk_low:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   bne    rd_wait_for_clk_low

# stop driving databus
   st     r10, (r6)  # Stop Driving data bus
   st     r11, 4(r6)
   st     r12, 8(r6)
   st     r9, GPCLR0_offset(r6) #clear databus ready for next time and debug pin

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
   mov    r8, r7
   ld     r7, GPLEV0_offset(r6)
   btst   r7, CLK
   bne    wr_wait_for_clk_low

# Post a message to indicate a tube register write

#  move databus to correct position
   lsr    r7,r8, D0D3_shift
   lsr    r4,r8, D4D7_shift -4
   and    r7, 0x0F
   and    r4, 0xF0
   or     r7, r4

#  move address bit to correct position
   btst   r8, r16
   bsetne r7, 8
   btst   r8, r17
   bsetne r7, 9
   btst   r8, r18
   bsetne r7, 10
   st     r7, (r3)      # post mail
   bl     toggle_led
   b      Poll_loop

# Post a message to indicate a reset
post_reset:
   mov    r7, 1<<RESET_MAILBOX_BIT
   st     r7, (r3)
   bl     toggle_led
# Wait for reset to be released (so we don't overflow the mailbox)
post_reset_loop:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, nRST
   beq    post_reset_loop
   b      Poll_loop

#### Toggle LED
toggle_led:

   cmp    r19, 0
   beq    led_type0
   cmp    r19, 1
   beq    led_type1
   cmp    r19, 3
   beq    led_type3
   cmp    r19, 4
   beq    led_type4

   # must be led_type2
   # only on raspberry pi 3

   rts

# led_type 0 found on 26 pin raspberry pis
led_type0:
   mov    r7, (1<<LED_TYPE0_BIT)
   btst   r20, 0
   not    r20, r20
   bne    led_type0_set
   st     r7,GPCLR0_offset(r6)
   rts
led_type0_set:
   st     r7,GPSET0_offset(r6)
   rts

# led_type 1 found on most 40 pin raspberry pis
led_type1:
   mov    r7, (1<<LED_TYPE1_BIT)
   btst   r20, 0
   not    r20, r20
   bne    led_type1_set
   st     r7,GPCLR1_offset(r6)
   rts
led_type1_set:
   st     r7,GPSET1_offset(r6)
   rts

# led_type 3 found on rpi3b+ GPIO29
led_type3:
   mov    r7, (1<<LED_TYPE3_BIT)
   btst   r20, 0
   not    r20, r20
   bne    led_type1_set
   st     r7,GPCLR0_offset(r6)
   rts
led_type3_set:
   st     r7,GPSET0_offset(r6)
   rts

# led_type 4 found on the Pi 4
led_type4:
   mov    r7, (1<<LED_TYPE4_BIT)
   btst   r20, 0
   not    r20, r20
   bne    led_type4_set
   st     r7,GPCLR1_offset(r6)
   rts
led_type4_set:
   st     r7,GPSET1_offset(r6)
   rts

# This code is not currently used

.if 0

# The interrupt handler just deals with nRST being pressed
# This saves two instructions in Poll_loop
irq_handler:

   # Clear the interrupt condition
   mov    r8, 0xffffffff
   st     r8, GPEDS0_offset(r6)

   ld     r8, GPLEV0_offset(r6)
   btst   r8, nRST
   bne    irq_handler_exit

   bl     do_post_mailbox

irq_handler_exit:
   rti

demo_irq_handler:

   mov    r6, GPFSEL0

   mov    r5, 0xffffffff
   st     r5, GPEDS0_offset(r6)

   mov    r5, (1<<20)
   st     r5, GPSET0_offset(r6) #DEBUG pin
   st     r5, GPCLR0_offset(r6) #DEBUG pin

   rti
.endif
