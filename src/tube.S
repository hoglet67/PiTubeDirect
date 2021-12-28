#include "rpi-base.h"
#include "rpi-mailboxregs.h"
#include "tube-irqbits.h"
#include "copro-65tubeasm.h"

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
#else
      ldr      r12, =MBOX0_READ        // Read the GPU mailbox
#endif

      push     {r0-r3,r14}
      ldr      r10,=tube_irq
      ldr      r0, [r12]               // Get Mailbox data

#ifdef USE_DOORBELL
      ldr      r12, =DOORBELLDATA
      ldr      r0, [r12]               // Get Mailbox data
#else
      tst      r0, #0x0F               // Discard if the channel bits are non-zero
      popne    {r0-r3,r14}             // Clean up the stack
      subnes   pc, lr, #4              // And return without invoking the handler
#endif

      bl       tube_io_handler

      ldr      r12,[r10]
      pop      {r0-r3,r14}
      tst      r12, #FAST6502_BIT      // see if we should signal the 6502 Core
      tstne    r12,#RESET_BIT+NMI_BIT+IRQ_BIT
      orrne    copro6502asm_instruction_table, copro6502asm_instruction_table, #copro6502asm_instruction_size

// fall into return

// Default handlers for FIQ/IRQ do nothing

arm_fiq_handler:

arm_irq_handler:

      subs    pc, lr, #4
