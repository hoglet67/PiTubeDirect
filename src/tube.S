#include "rpi-base.h"
#include "tube-defs.h"
#include "cache.h"

#define DETECT_DUMMY_READ
// #define DEBUG_OVERRUN
// #define DEBUG_LATE
// #define ON_READS_SPIN_FOR_PHI2_HIGH

#define GPFBASE (GPFSEL0)
#define GPFSEL0_offset (GPFSEL0 - GPFBASE )  // controls GPIOs 0..9
#define GPFSEL1_offset (GPFSEL1 - GPFBASE )  // controls GPIOs 10..19
#define GPFSEL2_offset (GPFSEL2 - GPFBASE )  // controls GPIOs 20..29
#define GPSET0_offset  (GPSET0 - GPFBASE)
#define GPCLR0_offset  (GPCLR0 - GPFBASE)
#define GPLEV0_offset  (GPLEV0 - GPFBASE)
#define GPEDS0_offset  (GPEDS0 - GPFBASE)     
.text
.global tube_regs
.global tube_mailbox
.global arm_irq_handler
.global arm_fiq_handler
.global arm_fiq_handler_flag0
#ifndef USE_MULTICORE
.global arm_fiq_handler_flag1
#endif        
.global lock_isr
.global gpfsel_data_idle
.global gpfsel_data_driving

.macro SET_TEST_PIN_HIGH
#ifdef HAS_40PINS
        mov     r12, #TEST_MASK
        str     r12, [r13,#GPSET0_offset]
#endif
.endm

.macro SET_TEST_PIN_LOW
 #ifdef HAS_40PINS
        mov     r12, #TEST_MASK
        str     r12, [r13,#GPCLR0_offset]
#endif
.endm

.macro FIQ_HANDLER flag_using_ip

        //r8-r11 are shadowed in FIQ mode, so no need to push
	// 	R13 ( sp ) is defiend to point to GPFBASE
	//LDR	r13,=GPFBASE
	SET_TEST_PIN_HIGH
	
        mov     r9, #-1
        str     r9, [r13,#GPEDS0_offset]        // clear all Pin events

        ldr     r9, [r13,#GPLEV0_offset]        // *** very expensive but necessary ***

	adr     r8, gpfsel_data_driving		// free cycle get ready incase of a read cycle
	adr     r11, tube_regs			// free cycle	
	ldmia	r8,{r8,r10,r12}			// free cycle
	
        tst     r9, #NRST_MASK          	// test for reset
        beq     post_mail\@

        tst     r9, #NTUBE_MASK         	// test for glitches
        bne     exit\@

        tst     r9, #RNW_MASK
        beq     wr_cycle\@

// READ_CYCLE
        //stmia	r13,{r8,r10,r12}		// Doesn't work
	str     R8, [r13,#GPFSEL0_offset]
	str     r10, [r13, #GPFSEL1_offset]            // *** expensive but necessary ***

	tst     r9, #A0_MASK
	and	r9,r9,#A1_MASK+A2_MASK
        orrne   r11, #1

        ldrb    r9, [r11,R9,A1_SHIFT]
	str     r12, [r13, #GPFSEL2_offset]            // *** expensive but necessary ***

        and     r11, r9, #0x0F 				// get lower nibble
	and 	r9,  r9, #0xF0 				// get upper nible
	mov	r9,  r9,LSL #D4_BASE-4 			// move upper nibble to the correct place
        orr     r9, r9, r11,LSL #D0_BASE 		// move lower nibble to correct place and orr it it
	
        str     r9, [r13,#GPSET0_offset]                // *** expensive but necessary ***
  
 	SET_TEST_PIN_LOW

 	
#ifdef ON_READS_SPIN_FOR_PHI2_HIGH
rd_wait_for_phi2_high1\@:
        ldr     r10, [r13,#GPLEV0_offset]                // ** very expensive but necessary ***
        tst     r10, #PHI2_MASK
        beq     rd_wait_for_phi2_high1\@
#endif
	adr	r8,gpfsel_data_idle
	LDMIA	R8,{r8,R11,R12}	

rd_wait_for_phi2_low\@:
        ldr     r10, [r13,#GPLEV0_offset]                // ** very expensive but necessary ***
        tst     r10, #PHI2_MASK
        movne   r9, r10
        bne     rd_wait_for_phi2_low\@

	//STMIA	r13,{r8,R11,R12}		// Doesn't work
        str     r8, [r13,#GPFSEL0_offset]              	// *** expensive but necessary ***
        str     r11, [r13, #GPFSEL1_offset]            	// *** expensive but necessary ***
        str     r12, [r13, #GPFSEL2_offset]            	// *** expensive but necessary ***

        mov     r10, #D30_MASK				// Clear databus
        orr     r10, r10, #D74_MASK
        str     r10, [r13,#GPCLR0_offset]               // *** expensive but necessary ***

// In some rare cases, a read may be immediately followed by a write
// A concrete case of this is *FX 151,230,N which uses STA &FEE0,X
// To detect this case, on reads we wait for one more edge of Phi2
// and the re-check the nTUBE and RNW lines for a possible write

#ifdef DETECT_DUMMY_READ        
rd_wait_for_phi2_high2\@:
        ldr     r10, [r13,#GPLEV0_offset]                // ** very expensive but necessary ***
        tst     r10, #PHI2_MASK
        beq     rd_wait_for_phi2_high2\@
        tst     r10, #(NTUBE_MASK | RNW_MASK)
        beq     wr_wait_for_phi2_low\@
#endif

        tst     r9, #A0_MASK             // don't bother emulator with status reads
        bne     post_mail\@

	SET_TEST_PIN_LOW
        subs    pc, lr, #4

// WRITE_CYCLE
wr_cycle\@:

wr_wait_for_phi2_high\@:
        ldr     r9, [r13,#GPLEV0_offset]
        tst     r9, #PHI2_MASK
        beq     wr_wait_for_phi2_high\@

wr_wait_for_phi2_low\@:
        ldr     r10, [r13,#GPLEV0_offset]
        tst     r10, #PHI2_MASK
        movne   r9, r10
        bne     wr_wait_for_phi2_low\@

// At this point, cache misses will no longer disrupt the 6502 bus timings
post_mail\@:
        ldr     r10, =PINS_MASK
        and     r9, r9, r10

post_mail2\@:
        orr     r9, r9, #ATTN_MASK

        ldr     r10, tube_mailbox
        tst     r10, #ATTN_MASK         // if previous message not acknowledged, then flag overrun
        orrne   r9, r9, #OVERRUN_MASK
        str     r9, tube_mailbox

// Update TEST2 Pin to reflect overun state      
#if defined(DEBUG_OVERRUN) && defined(HAS_40PINS)
        mov     r10, #TEST2_MASK
        strne     r10, [r13,#GPSET0_offset]
        streq     r10, [r13,#GPCLR0_offset]

#endif

// Update TEST3 Pin to reflect late state      
#if defined(DEBUG_LATE) && defined(HAS_40PINS)
        mov     r10, #TEST3_MASK
        tst     r9, #NTUBE_MASK
        strne     r10, [r13,#GPSET0_offset]
        streq     r10, [r13,#GPCLR0_offset]
#endif
        
.if \flag_using_ip
        // Switch back to irq mode so ip is not shadowed
        mrs     r9, cpsr
        bic     r9, r9, #0x1F
        orr     r9, r9, #0x12
        msr     cpsr_c, r9
        
        orr     ip, ip, #1024   // signal event to 6502 instruction flow

        // Switch back to fiq mode, so we return correctly
        // lr is used here because its shadowed in IRQ mode, so doesn't corrupt the normal lr
        mrs     lr, cpsr
        bic     lr, lr, #0x1F
        orr     lr, lr, #0x11
        msr     cpsr_c, lr

        // Note, the above mode switching could be avoided if we reworked the register assignment
        // in the 6502 emulator to use a register r0..r7 for signalling.
        // I did do this as an experiment, and it's parked in a branch.
        // I'd like to avoid making big changes to that code for now.
.endif

exit\@:
        

        SET_TEST_PIN_LOW
        //r8-r11 are shadowed in FIQ mode, so no need to pop
        //pop   {r8-r11}
        subs    pc, lr, #4

glitch\@:
        ldr     r10, =PINS_MASK
        and     r9, r9, r10
        orr     r9, r9, #GLITCH_MASK
        b       post_mail2\@

.endm
   
// =================================================
// LOCK the ISR CODE into cache
// =================================================
CACHELINE_ALIGN = 5
        
lock_isr:
    push    {lr}

    // Copy locking code into un-cached memory
    ldr     r0, =lock_isr_into_cache_start
    ldr     r1, =UNCACHED_MEM_BASE
    mov     r2, #(lock_isr_into_cache_end - lock_isr_into_cache_start)
copy_loop:
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    subs    r2, r2, #4
    bne     copy_loop

    // disable interrupts (so handler doesn't get accidentally executed
    bl      _disable_interrupts

    // execute locking code
    ldr     r0, =UNCACHED_MEM_BASE
    blx     r0

    // enable interrupts
    bl      _enable_interrupts

    pop     {lr}
    mov     pc, lr

// This code must only be executed from uncachable memory
// and must be position independant

// Cache ways are 4KB, and contain 128 x 32 byte lines
        
// I Cache Locked Way
        
// 0x00000000-0x0000001f - cache line  0     - (vectors)
// 0x01f01180-0x01f012ff - cache lines 12-23 - (ISR code)
        
// D Cache Locked Way

// 0x?????000-0x?????01f - cache line  0       - (vector TLBs)
// 0x00000020-0x0000003f - cache line  1       - (vectors)
// 0x?????060-0x?????07f - cache line  3       - (code/data/stack TLBs)
// 0x01f01300-0x01f01380 - cache lines 24-27   - (ISR data)
// 0x?????800-0x?????81f - cache line  64      - (IO TLB)
// 0x01edff00-0x01edffff - cache lines 120-127 - (FIQ stack, not currently used)

.align CACHELINE_ALIGN
lock_isr_into_cache_start:

    // invalidate entire instruction cache (page 3-74)
    mov     r0, #0
    mcr     p15, 0, r0, c7, c5, 0

    // clean and invalidate the entire data cache (page 3-74)
    mov      r0, #0
    mcr      p15, 0, r0, c7, c14, 0

    // data synchronization barrier to ensure all outstanding memory transactions are complete
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
        
    // Set the fast interrupt mode bit
    // This is commented out because
    // 1. It costs in overall performance
    // 2. It did not make an observable different to tube read latency

    // mrc     p15, 0, r0, c1, c0, 0
    // orr     r0, r0, #(1<<21)
    // mcr     p15, 0, r0, c1, c0, 0

    // data synchronization barrier to ensure all outstanding memory transactions are complete
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
        
    // enable d-cache way 0, lock ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffffe
    mcr     p15, 0, r0, c9, c0, 0
        
    // enable i-cache way 0, lock ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffffe
    mcr     p15, 0, r0, c9, c0, 1

    // bring the isr code into i-cache
    // these loads are relative references, so should relocate
    ldr     r0, isr_code_start_ptr
    ldr     r1, isr_code_end_ptr
lock_code_loop:
    // prefetch cache line into instruction cache (page 3-76)
    // r0 is the virtual address, with bits 4..0 set to zero
    mcr     p15, 0, r0, c7, c13, 1
    // move to the next cache line
    add     r0, r0, #32
    cmp     r0, r1
    bne     lock_code_loop

    // prefetch the vector instructions (8 words starting at 0x00000000) into the i-cache
    mov     r0, #0x00
    mcr     p15, 0, r0, c7, c13, 1

    // any data prefetched is actually used by accumulating into r3
    // don't think this is actually necessary, but lets be really
    // sure the ARM doesn't abort the transactions
    mov     r3, #0x00
        
    // bring the isr data into d-cache
    // these loads are relative references, so should relocate
    ldr     r0, isr_data_start_ptr
    ldr     r1, isr_data_end_ptr
lock_data_loop:
    // load from the cache line to prefetch it
    ldr     r2, [r0]
    add     r3, r3, r2
    // move to the next cache line
    add     r0, r0, #32
    cmp     r0, r1
    bne     lock_data_loop

    // prefetch the vector data (8 words starting at 0x00000020) into the d-cache
    mov     r0, #0x20
    ldr     r2, [r0]
    add     r3, r3, r2

    // prefetch the used page table (TLB) entries
    // The complete table is 4K words (16K bytes) with one work per 1MB of virtual memory
    // It is aligned on a 4KB boundary
    // Each cache line contains 8 words representing 8MB of memory
    ldr     r0, page_table_ptr
    ldr     r2, [r0, #(0x000 * 4)]  // 0x00000000-0x000fffff (vectors)
    add     r3, r3, r2
    ldr     r2, [r0, #(0x018 * 4)]  // 0x01800000-0x01ffffff (stacks, code and data)
    add     r3, r3, r2
    ldr     r2, [r0, #(0x200 * 4)]  // 0x20000000-0x027fffff (IO - todo should use PERIPHERAL_BASE here)
    add     r3, r3, r2

    // bring the isr stack into d-cache
    // these loads are relative references, so should relocate
    ldr     r0, isr_stack_start_ptr
    ldr     r1, isr_stack_end_ptr
lock_stack_loop:
    // load from the cache line to prefetch it
    ldr     r2, [r0]
    add     r3, r3, r2
    // move to the next cache line
    add     r0, r0, #32
    cmp     r0, r1
    bne     lock_stack_loop

    // data synchronization barrier to ensure all outstanding memory transactions are complete
    mov     r0, #0
    mcr     p15, 0, r0, c7, c10, 4
        
    // lock d-cache way 0, enable ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffff1
    mcr     p15, 0, r0, c9, c0, 0

    // lock i-cache way 0, enable ways 1, 2, 3 (page 3-89)
    mov     r0, #0xfffffff1
    mcr     p15, 0, r0, c9, c0, 1

    mov     pc, lr

isr_code_start_ptr:
.word isr_code_start

isr_code_end_ptr:
.word isr_code_end

isr_data_start_ptr:
.word isr_data_start

isr_data_end_ptr:
.word isr_data_end

page_table_ptr:
.word PageTable
        
// TODO these should be somehow imported from arm-startup.S
        
isr_stack_start_ptr:
.word 0x01EE0000 - 0x100

isr_stack_end_ptr:
.word 0x01EE0000


.align CACHELINE_ALIGN
lock_isr_into_cache_end:

// =================================================
// ISR CODE
// =================================================

.align CACHELINE_ALIGN
isr_code_start:

// Default handlers for FIQ/IRQ do nothing

arm_fiq_handler:
arm_irq_handler:
        subs    pc, lr, #4

// ARM FIQ handler (two versions)

#ifndef USE_MULTICORE
arm_fiq_handler_flag1:
FIQ_HANDLER 1
#endif

arm_fiq_handler_flag0:
FIQ_HANDLER 0

.align CACHELINE_ALIGN
isr_code_end:

// =================================================
// ISR DATA
// =================================================

.align CACHELINE_ALIGN
isr_data_start:

// 8 BYTES OF STATE  Muts be align to an 8 byte boundary 
tube_regs:
        .byte 0xfe
        .byte 0xed
        .byte 0xdc
        .byte 0xcb
        .byte 0xba
        .byte 0xa9
        .byte 0x98
        .byte 0x87
// Insert the current literal pool, so these get locked in the cache
.ltorg

// Some constants for controlling the data bus        
gpfsel_data_idle:
        .word 0
        .word 0
        .word 0

gpfsel_data_driving:
        .word 0
        .word 0
        .word 0

// Mailbox between tube isr and events handler (in C)
tube_mailbox:
        .word 0


.align CACHELINE_ALIGN
isr_data_end:
