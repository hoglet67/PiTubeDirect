#-------------------------------------------------------------------------
# VideoCore IV implementation of tube handler
#-------------------------------------------------------------------------

# on entry
# GPIO pins setup by arm
# Addresses passed into vc are VC based
# gpfsel_data_idle setup

#  r0 - pointer to shared memory ( VC address) of tube registers
#  r1 - pointer to shared memory ( VC address) of gpfsel_data_idle
#  r2 - pointer to shared memory ( VC address) of tube_mailbox
#  r3 - GPIO mapping of address lines - 0x00<A2><A1><A0> where A2, A1 and A0 are the numbers of the respective GPIOs
#  r4 - unused
#  r5 - debug pin mask (0 = no debug  xx= debug pin e.g 1<<21)

# Intenal register allocation
#  r0 - pointer to shared memory ( VC address) of tube registers
#  r1 - ( 1<<nTUBE+1<<nRST+1<<RnW+1<<A2+1<<A1+1<<A0+(0xF<<D0D3_shift) + (0xF<<D4D7_shift))
#  r2 - pointer to shared memory ( VC address) of tube_mailbox
#  r3 - ARM_GPU_MAILBOX constant
#  r4 - 4-bit sequence number for detecting overruns
#  r5 - debug pin mask (0 = no debug  xx= debug pin e.g 1<<21)
#  r6 - GPFSEL0 constant
#  r7 - scratch register 
#  r8 - scratch register
#  r9 - (0xF<<D0D3_shift) + (0xF<<D4D7_shift) constant
# r10 - data bus driving values
# r11 - data bus driving values
# r12 - data bus driving values
# r13 - data bus driving values
# r14 - data bus driving values
# r15 - data bus driving values
# r16 - A0 GPIO bit number
# r17 - A1 GPIO bit number
# r18 - A2 GPIO bit number        

# GPIO registers
.equ GPFSEL0,       0x7e200000
.equ GPSET0_offset, 0x1C
.equ GPCLR0_offset, 0x28
.equ GPLEV0_offset, 0x34
.equ GPEDS0_offset, 0x40

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
        
   mov    r9, (0xF<<D0D3_shift) + (0xF<<D4D7_shift) # all the databus
   ld     r10, (r1)    # databus driving signals
   ld     r11, 4(r1)   # databus driving signals
   ld     r12, 8(r1)   # databus driving signals
   ld     r13, 12(r1)  # databus driving signals
   ld     r14, 16(r1)  # databus driving signals
   ld     r15, 20(r1)  # databus driving signals

   lsr    r16, r3, 0   # Extract GPIO number of A0
   and    r16, 31
   lsr    r17, r3, 8   # Extract GPIO number of A1
   and    r17, 31
   lsr    r18, r3, 16  # Extract GPIO number of A2
   and    r18, 31

# r1, r3, r4 now free

   mov    r1, (1<<nTUBE+1<<nRST+1<<RnW+(0xF<<D0D3_shift) + (0xF<<D4D7_shift))
   bset   r1, r16
   bset   r1, r17
   bset   r1, r18

   mov    r3, GPU_ARM_MBOX
        
   mov    r4, 0        # 4-bit sequence number for detecting overun

   mov    r6, GPFSEL0

# enable interrupts
#  ei
                
# poll for nTube being low
Poll_loop:
   mov    r7, r0
Poll_tube_low:
   ld     r8, GPLEV0_offset(r6)
   btst   r8, nRST
   beq    post_reset
   btst   r8, nTUBE
   bne    Poll_tube_low
   ld     r8, GPLEV0_offset(r6)  # check ntube again to remove glitches
   btst   r8, nTUBE
   bne    Poll_tube_low
   # we now know nTube is low
   btst   r8, RnW
   beq    wr_cycle

   # So we are in a read cycle
   # sort out the address bus
   
   btst   r8, r16
   orne   r7, 4
   btst   r8, r17
   orne   r7, 8
   btst   r8, r18
   orne   r7, 16
   ld     r8, (r7)               # Read word from tube register
   st     r13, (r6)              # Drive data bus
   st     r14, 4(r6)             # Drive data bus        
   st     r15, 8(r6)             # Drive data bus
   st     r8, GPSET0_offset(r6)  # Write word to data bus

   st     r5, GPSET0_offset(r6)  # DEBUG pin

  # spin waiting for clk high
rd_wait_for_clk_high1:
   ld     r8, GPLEV0_offset(r6)
   btst   r8, CLK
   beq    rd_wait_for_clk_high1
# we now have half a cycle to do post mail
   btst   r8, r16               # no need to post mail if A0 = 0
   beq    rd_wait_for_clk_low
   sub    r7, r0                # just get the address bits
   lsl    r7, 6                 # put address bits in correct place
   bset   r7, RW_MAILBOX_BIT    # set read bit
   st     r7, (r3)              # store in mail box
   
# spin waiting for clk low
rd_wait_for_clk_low:
   ld     r7, GPLEV0_offset(r6)
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
   b      Poll_loop
        
# Post a message to indicate a reset
post_reset:
   mov    r7, 1<<RESET_MAILBOX_BIT
   st     r7, (r3) 
# Wait for reset to be released (so we don't overflow the mailbox)
post_reset_loop:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, nRST
   beq    post_reset_loop        
   b      Poll_loop
.if 0        
# Subroutine to post r8 to the mailbox
# if r2 is zero then the hardware mailbox is used
# if r2 is non zero then a software mailbox at address r2 is used
        
do_post_mailbox:
# Send to GPU->ARM mailbox (channel 10)
   and    r8, r1

   cmp    r2, 0
   beq    use_hw_mailbox

# Use the software mailbox
   bset   r8, ATTN_MASK
   ld     r7, (r2)  # get mailbox
   btst   r7, ATTN_MASK
   bsetne r8, OVERRUN_MASK
   st     r8, (r2)
   rts

# Use the hardware mailbox
use_hw_mailbox:
# Add 4-bit seqence number into bits 12-15 (unused GPIO bits), and increment
   or     r8, r4
   add    r4, 0x1000
   and    r4, 0xF000
   lsl    r8, 4
   or     r8, 0x0000000A
   st     r8, (r3)
   rts        
.endif        
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
