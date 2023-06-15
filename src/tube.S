#include "rpi-base-asm.h"
#include "rpi-mailboxregs.h"
#include "tube-irqbits.h"
#include "tube-defs.h"

.text

.global arm_irq_handler
.global arm_fiq_handler
.global arm_fiq_handler_flag1

// =================================================
// ISR CODE
// =================================================
CACHELINE_ALIGN = 5

.align CACHELINE_ALIGN

// ARM FIQ handler
arm_fiq_handler_flag1:

#ifdef USE_DOORBELL
      ldrd     r10, doorbell_const      // Read the GPU doorbell
      push     {r0-r3,r14}
      ldr      r1, [r10]               // Get Mailbox data
      ldr      r0, [r11]               // Get Mailbox data
      bl       tube_io_handler
      pop      {r0-r3,r14}
#else
      ldr      r12, =MBOX0_READ        // Read the GPU mailbox

      push     {r0-r3,r14}
      ldr      r0, [r12]               // Get Mailbox data

      tst      r0, #0x0F               // Discard if the channel bits are non-zero
      bleq     tube_io_handler
      pop      {r0-r3,r14}
#endif

// fall into return

// Default handlers for FIQ/IRQ do nothing

arm_fiq_handler:

      subs    pc, lr, #4
#ifdef USE_DOORBELL
.align 3
doorbell_const:
      .word DOORBELL
      .word DOORBELLDATA
#endif