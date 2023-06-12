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
      ldr      r12, =DOORBELL          // Read the GPU doorbell
      ldr      r11, =DOORBELLDATA
      push     {r0-r3,r14}
      ldr      r10,=tube_irq
      ldr      r0, [r12]               // Get Mailbox data
      ldr      r0, [r11]               // Get Mailbox data

#else
      ldr      r12, =MBOX0_READ        // Read the GPU mailbox

      push     {r0-r3,r14}
      ldr      r10,=tube_irq
      ldr      r0, [r12]               // Get Mailbox data

      tst      r0, #0x0F               // Discard if the channel bits are non-zero
      popne    {r0-r3,r14}             // Clean up the stack
      subnes   pc, lr, #4              // And return without invoking the handler
#endif

      bl       tube_io_handler

      ldr      r12,[r10]
      pop      {r0-r3,r14}

// fall into return

// Default handlers for FIQ/IRQ do nothing

arm_fiq_handler:

      subs    pc, lr, #4
