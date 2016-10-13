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
.equ GPEDS0_offset, 0x40

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

.equ vector,       49

.equ IC0_MASK,     0x7e002010
.equ IC1_MASK,     0x7e002810

.org 0

# code entry point
#  ld r0, (r0)
#  rts
        
# disable interrupts
   di
        
# Setup interrupt vector
# 0x1EC01E00 looks like the interrupt table
# Vector 113 = 49 + 64 = GPIO0      

  mov r8, (0x40000000 + 0x1EC01E00 + (113 << 2))
  lea r9, irq_handler(pc)
  st  r9, (r8)
 
# mask ARM interrupts
   mov r8, IC0_MASK
   mov r9, IC1_MASK
   mov r10, 0x0
   mov r11, 8
mask_all:
   st r10, (r8)
   st r10, (r9)
   add r8, 4
   add r9, 4
   sub r11, 1
   cmp r11, 0
   bne mask_all   
   
# enable the interupt (49 + 64 = 113)
   mov r8, IC0_MASK + 0x18
   mov r9, 0x00000010
   st  r9, (r8)
        
   mov    r9, (0xF<<D0D3_shift) + (0xF<<D4D7_shift) # all the databus
   ld     r10, (r1)    # databus driving signals
   ld     r11, 4(r1)   # databus driving signals
   ld     r12, 8(r1)   # databus driving signals
   ld     r13, 12(r1)  # databus driving signals
   ld     r14, 16(r1)  # databus driving signals
   ld     r15, 20(r1)  # databus driving signals
# r1 is now free
   mov    r6, GPFSEL0

# enable interrupts
   ei
                
# poll for nTube being low
Poll_loop:
   ld     r7, GPLEV0_offset(r6)
   btst   r7, nTUBE
   bne    Poll_loop
   ld     r7, GPLEV0_offset(r6)  # check ntube again to remove glitches
   btst   r7, nTUBE
   bne    Poll_loop
   # we now know nTube is low
   btst   r7, RnW
   beq    wr_cycle

   # So we are in a read cycle
   # sort out the address bus
   btst   r7, A0
   and    r7, (1<<A1)+(1<<A2)
   lsr    r7, 1                  # A1 shift
   orne   r7, 1
   ld     r8, (r0, r7)           # Read word from tube register
   st     r13, (r6)              # Drive data bus
   st     r14, 4(r6)             # Drive data bus        
   st     r15, 8(r6)             # Drive data bus
   st     r8, GPSET0_offset(r6)  # Write word to data bus

   st     r5, GPSET0_offset(r6)  # DEBUG pin

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
        
# The interrupt handler just deals with nRST being pressed
# This saves two instructions in Poll_loop
        
irq_handler:

   # Clear the interrupt condition
   mov    r8, 0xffffffff
   st     r8, GPEDS0_offset(r6)

   ld     r8, GPLEV0_offset(r6)
   btst   r8, nRST
   bne    irq_handler_exit
        
   and    r8,( 1<<nTUBE+1<<nRST+1<<RnW+1<<A2+1<<A1+1<<A0+(0xF<<D0D3_shift) + (0xF<<D4D7_shift))
   bset   r8, ATTN_MASK
   ld     r7, (r2)  # get mailbox
   btst   r7, ATTN_MASK
   bsetne r8, OVERRUN_MASK
   st     r8, (r2)

irq_handler_exit:
   rti

# This code is not used
        
demo_irq_handler:

   mov    r6, GPFSEL0
        
   mov    r5, 0xffffffff
   st     r5, GPEDS0_offset(r6)

   mov    r5, (1<<20)
   st     r5, GPSET0_offset(r6) #DEBUG pin
   st     r5, GPCLR0_offset(r6) #DEBUG pin
        
   rti
