#include "rpi-base.h"
#include "tube-defs.h"

.text
.global arm_irq_handler
.global arm_firq_handler
.global copro_65tube_tube_read
.global copro_65tube_tube_write
        
// ARM IRQ handler

// This is the original IRQ handler from PiTubeClient's copro-65tube
//
// Retained for reference so we can see how events (IRQ, NMI, RST)
// are signalled to the 6502 code.
//
// If we keep this same "interface" then we should be able to re-use that code without change
//
//arm_irq_handler:
//        push    {r0, r1, r2}
//        ldr     r0, =(PERIPHERAL_BASE + 0x200040) // any falling events?
//        ldr     r2, [r0]
//        mov     r1, #1<<7               // mask our events
//        orr     r1, r1, #1<<18 + 1<<17
//        and     r2, r2, r1
//        str     r2, [r0]                // clear Pin events
//        ldr     r1, events
//        orr     r1, r1, r2              // add our events in
//        str     r1, events
//        orr     ip, ip, #256<<I_ALIGN   // signal event to 6502 instruction flow
//        pop     {r0, r1, r2}
//        subs    pc, lr, #4


arm_firq_handler:
        subs    pc, lr, #4
        
arm_irq_handler:
        
        push    {r0-r3,ip,lr}
        
        ldr     r0, =(PERIPHERAL_BASE + 0x200040) // address of GPEDS0
        ldr     r1, [r0]
        mov     r2, #NTUBE_MASK         // test if it's a falling edge of nNTUBE
        ands    r1, r1, r2              // set the Z if there was a falling edge detected
        str     r1, [r0]                // clear Pin events


        ldr     r0, =(PERIPHERAL_BASE + 0x200034) // address of GPLEV0
        ldr     r1, [r0]

        bl      temp_tube_io_handler
        
        pop     {r0-r3,ip,lr}
        subs    pc, lr, #4


// parasite reading the tube
copro_65tube_tube_read:
    mov      pc, lr

// parasite writing the tube
copro_65tube_tube_write:
    mov      pc, lr
        
