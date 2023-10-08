/*
 * RISCV Co Pro Emulation
 *
 * (c) 2017 Ed Spittles after David Banks
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tube-client.h"
#include "tube-ula.h"
#include "tube.h"
#include "info.h"
#include "riscv/tuberom.h"

// Define the memory size of the RISCV Co Pro (must be a power of 2)
#define MINI_RV32_RAM_SIZE (16*1024*1024)

// Memory Map (assuming 16MB)
//
// 0x00000000 - RAM
// 0x00FFFFE0 - Tube R1 Status
// 0x00FFFFE4 - Tube R1 Data
// 0x00FFFFE8 - Tube R2 Status
// 0x00FFFFEC - Tube R2 Data
// 0x00FFFFF0 - Tube R3 Status
// 0x00FFFFF4 - Tube R3 Data
// 0x00FFFFF8 - Tube R4 Status
// 0x00FFFFFC - Tube R4 Data

#define ADDR_MASK (MINI_RV32_RAM_SIZE - 1)
#define TUBE_MASK (MINI_RV32_RAM_SIZE - 32)

// In our implementation RAM starts at address zero
#define MINIRV32_RAM_IMAGE_OFFSET 0

#define MINIRV32_IMPLEMENTATION

#define MINIRV32_CUSTOM_MEMORY_BUS

#define MINIRV32_STORE4( ofs, val )  copro_riscv_write_mem32(ofs, val)
#define MINIRV32_STORE2( ofs, val )  copro_riscv_write_mem16(ofs, val)
#define MINIRV32_STORE1( ofs, val )  copro_riscv_write_mem8(ofs, val)
#define MINIRV32_LOAD4( ofs )        copro_riscv_read_mem32(ofs)
#define MINIRV32_LOAD2( ofs )        copro_riscv_read_mem16(ofs)
#define MINIRV32_LOAD1( ofs )        copro_riscv_read_mem8(ofs)
#define MINIRV32_LOAD2_SIGNED( ofs ) copro_riscv_read_mem16_signed(ofs)
#define MINIRV32_LOAD1_SIGNED( ofs ) copro_riscv_read_mem8_signed(ofs)

#define MINIRV32_POSTEXEC( pc, ir, trap ) { if ((CSR(mstatus & (1<<3))) && ((CSR(mip) & CSR(mie) & (1<<11)))) { trap = 0x8000000B; pc -= 4; } }

#include "copro-riscv.h"

static struct MiniRV32IMAState state;
struct MiniRV32IMAState *riscv_state = &state;

#ifdef INCLUDE_DEBUGGER
#include "cpu_debug.h"
#include "riscv/riscv_debug.h"
#endif

static uint8_t *memory;

void copro_riscv_write_mem32(uint32_t addr, uint32_t data) {
   addr &= ADDR_MASK;
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memwrite(&riscv_cpu_debug, addr, data, 4);
   }
#endif
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      tube_parasite_write((addr >> 2) & 7, (uint8_t) data);
   } else {
      *(uint32_t *)(memory + addr) = data;
   }
}

void copro_riscv_write_mem16(uint32_t addr, uint32_t data) {
   addr &= ADDR_MASK;
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memwrite(&riscv_cpu_debug, addr, data, 2);
   }
#endif
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      tube_parasite_write((addr >> 2) & 7, (uint8_t) data);
   } else {
      *(uint16_t *)(memory + addr) = (uint16_t) data;
   }
}

void copro_riscv_write_mem8(uint32_t addr, uint32_t data) {
   addr &= ADDR_MASK;
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memwrite(&riscv_cpu_debug, addr, data, 1);
   }
#endif
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      tube_parasite_write((addr >> 2) & 7, (uint8_t) data);
   } else {
      *(uint8_t *)(memory + addr) = (uint8_t)data;
   }
}

uint32_t copro_riscv_read_mem32(uint32_t addr) {
   addr &= ADDR_MASK;
   uint32_t data;
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      data = tube_parasite_read((addr >> 2) & 7);
   } else {
      data = *(uint32_t *)(memory + addr);
   }
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, data, 4);
   }
#endif
   return data;
}

uint32_t copro_riscv_read_mem16(uint32_t addr) {
   addr &= ADDR_MASK;
   uint32_t data;
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      data = tube_parasite_read((addr >> 2) & 7);
   } else {
      data = *(uint16_t *)(memory + addr);
   }
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

uint32_t copro_riscv_read_mem16_signed(uint32_t addr) {
   addr &= ADDR_MASK;
   uint32_t data;
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      data = tube_parasite_read((addr >> 2) & 7);
   } else {
      data = *(uint16_t *)(memory + addr);
   }
   if (data & 0x8000) {
      data |= 0xffff0000;
   }
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

uint32_t copro_riscv_read_mem8(uint32_t addr) {
   addr &= ADDR_MASK;
   uint32_t data;
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      data = tube_parasite_read((addr >> 2) & 7);
   } else {
      data = *(uint8_t *)(memory + addr);
   }
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, data, 1);
   }
#endif
   return data;
}

uint32_t copro_riscv_read_mem8_signed(uint32_t addr) {
   addr &= ADDR_MASK;
   uint32_t data;
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      data = tube_parasite_read((addr >> 2) & 7);
   } else {
      data = *(uint8_t *)(memory + addr);
   }
   if (data & 0x80) {
      data |= 0xffffff00;
   }
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, data, 1);
   }
#endif
   return data;
}

static void copro_riscv_poweron_reset() {
   // Initialize memory pointer
   memory = copro_mem_reset(MINI_RV32_RAM_SIZE);

   // Copy over client ROM
   copro_memcpy((void *) (memory + RESET_ADDRESS), (const void *)tuberom_riscv_bin, (size_t) tuberom_riscv_bin_len);

   // Reset all of the riscv state to 0
   memset((void *)riscv_state, 0, sizeof(struct MiniRV32IMAState));
}

static void copro_riscv_reset() {
   // Log ARM performance counters
   tube_log_performance_counters();

   // Reset the processor...

   // Reset to ‘m’ mode.
   riscv_state->extraflags |= 3;
   // Disable all interrupts.
   riscv_state->mie = 0;
   // mstatus.mprv = 0, Select normal memory access privilege level.
   riscv_state->mstatus &= 0xFFFDFFF7;
   // reset the timer
   riscv_state->timerl = 0;
   riscv_state->timerh = 0;
   riscv_state->timermatchl = 0;
   riscv_state->timermatchh = 0;
   // mcause = 0 or Implementation defined RESET_MCAUSE_VALUES.
   riscv_state->mcause = 0;
   // PC = Implementation defined RESET_VECTOR.
   riscv_state->pc = RESET_ADDRESS;

   // Wait for rst become inactive before continuing to execute
   tube_wait_for_rst_release();

   // Reset ARM performance counters
   tube_reset_performance_counters();
}

static inline uint32_t get_arm_cycle_count ()
{
  uint32_t value;
  // Read CCNT Register
#if (__ARM_ARCH >= 7 )
  asm volatile ("MRC p15, 0, %0, c9, c13, 0\t\n": "=r"(value));
#else
  asm volatile ("MRC p15, 0, %0, c15, c12, 1\t\n": "=r"(value));
#endif
  return value;
}

static uint32_t init_arm_cycle_count()
{
   uint32_t value;
#if (__ARM_ARCH >= 7 )
   asm volatile ("MRC p15, 0, %0, c9, c12, 0\t\n" : "=r"(value));
   value &= 0xfffffff7;
   asm volatile ("MCR p15, 0, %0, c9, c12, 0\t\n" :: "r"(value));
#else
   asm volatile ("MRC p15, 0, %0, c15, c12, 0\t\n" : "=r"(value));
   value &= 0xfffffff7;
   asm volatile ("MCR p15, 0, %0, c15, c12, 0\t\n" :: "r"(value));
#endif
   return get_arm_cycle_count();
}


void copro_riscv_emulator()
{
   // Remember the current copro so we can exit if it changes
   unsigned int last_copro = copro;

   // ARM clock ticks in 1ms
   uint32_t arm_cycles_per_ms = get_clock_rates(ARM_CLK_ID)->rate / 1000;

   copro_riscv_poweron_reset();
   copro_riscv_reset();

   // Last ARM cycle count
   uint32_t last_arm_cycle_count = init_arm_cycle_count();

   while (1) {

      // Track the elapsed time usin the ARM cycle counter, tring to
      // minimize the maths done each RISC-V instruction.
      uint32_t elapsed_cycles = get_arm_cycle_count() - last_arm_cycle_count;
      uint32_t elapsed_us = 0;
      if (elapsed_cycles >= arm_cycles_per_ms) {
         elapsed_us = 1000;
         last_arm_cycle_count += arm_cycles_per_ms;
      }

#ifdef INCLUDE_DEBUGGER
      if (riscv_debug_enabled)
      {
         debug_preexec(&riscv_cpu_debug, riscv_state->pc);
      }
#endif

      MiniRV32IMAStep(
                      riscv_state,   // struct MiniRV32IMAState * state
                      memory,        // uint8_t * image
                      0,             // uint32_t vProcAddress
                      elapsed_us,    // uint32_t elapsedUs
                      1              // int count
                      );


      int tube_irq_copy = tube_irq & ( RESET_BIT + IRQ_BIT);
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_riscv_reset();
            last_arm_cycle_count = init_arm_cycle_count();
         }
      }

      // IRQ is level sensitive, so check between every instruction
      if (tube_irq_copy & IRQ_BIT) {
         riscv_state->mip |= 0x00000800; // set bit 11 (external interrupt)
      } else {
         riscv_state->mip &= 0xfffff7ff; // clear bit 11 (external interrupt)
      }
   }
}
