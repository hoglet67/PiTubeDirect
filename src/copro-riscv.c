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
#include "riscv/tuberom.h"

// Define the memory size of the RISCV Co Pro (must be a power of 2)
#define MINI_RV32_RAM_SIZE (16*1024*1024)

// Memory Map (assuming 16MB)
//
// 0x00000000 - RAM
// 0x00FFFFE0 - Tube R1 Status
// 0x00FFFF4E - Tube R1 Data
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
      addr = (addr >> 2) & 7;
      tube_parasite_write(addr, (uint8_t) data);
   } else {
      *(uint32_t *)(memory + addr) = data;
   }
}

uint32_t copro_riscv_read_mem32(uint32_t addr) {
   addr &= ADDR_MASK;
   uint32_t data;
   if ((addr & TUBE_MASK) == TUBE_MASK) {
      addr = (addr >> 2) & 7;
      data = tube_parasite_read(addr);
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

void copro_riscv_write_mem16(uint32_t addr, uint16_t data) {
   addr &= ADDR_MASK;
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memwrite(&riscv_cpu_debug, addr, data, 4);
   }
#endif
   *(uint16_t *)(memory + addr) = data;
}

uint16_t copro_riscv_read_mem16(uint32_t addr) {
   addr &= ADDR_MASK;
   uint16_t data = *(uint16_t *)(memory + addr);
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, data, 2);
   }
#endif
   return data;
}

int16_t copro_riscv_read_mem16_signed(uint32_t addr) {
   addr &= ADDR_MASK;
   int16_t data = *(int16_t *)(memory + addr);
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, (uint16_t) data, 2);
   }
#endif
   return data;
}

void copro_riscv_write_mem8(uint32_t addr, uint8_t data) {
   addr &= ADDR_MASK;
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memwrite(&riscv_cpu_debug, addr, data, 1);
   }
#endif
   *(uint8_t *)(memory + addr) = data;
}

uint8_t copro_riscv_read_mem8(uint32_t addr) {
   addr &= ADDR_MASK;
   uint8_t data = *(uint8_t *)(memory + addr);
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, data, 1);
   }
#endif
   return data;
}

int8_t copro_riscv_read_mem8_signed(uint32_t addr) {
   addr &= ADDR_MASK;
   int8_t data = *(int8_t *)(memory + addr);
#ifdef INCLUDE_DEBUGGER
   if (riscv_debug_enabled) {
      debug_memread(&riscv_cpu_debug, addr, (uint8_t) data, 1);
   }
#endif
   return data;
}

static void copro_riscv_poweron_reset() {
   // Initialize memory pointer
   memory = copro_mem_reset(MINI_RV32_RAM_SIZE);

   // Copy over client ROM
   copro_memcpy((void *) (memory + 0x00000000), (const void *)tuberom_riscv_bin, (size_t) tuberom_riscv_bin_len);

   // Reset all of the riscv state to 0
   memset((void *)riscv_state, 0, sizeof(struct MiniRV32IMAState));
}

static void copro_riscv_reset() {
   // Log ARM performance counters
   tube_log_performance_counters();

   // Reset the processor...

   // Reset to ‘m’ mode.
   riscv_state->extraflags |= 3;
   // mstatus.mie = 0, Disable all interrupts.
   // mstatus.mprv = 0, Select normal memory access privilege level.
   riscv_state->mstatus &= 0xFFFDFFF7;
   // misa = DEFAULT_MISA, enable all extensions.

   // mcause = 0 or Implementation defined RESET_MCAUSE_VALUES.
   riscv_state->mcause = 0;
   // PC = Implementation defined RESET_VECTOR.
   riscv_state->pc = 0;

   // Wait for rst become inactive before continuing to execute
   tube_wait_for_rst_release();

   // Reset ARM performance counters
   tube_reset_performance_counters();
}

void copro_riscv_emulator()
{
   // Remember the current copro so we can exit if it changes
   unsigned int last_copro = copro;

   copro_riscv_poweron_reset();
   copro_riscv_reset();

   while (1) {

      MiniRV32IMAStep(
                      riscv_state,   // struct MiniRV32IMAState * state
                      memory,        // uint8_t * image
                      0,             // uint32_t vProcAddress
                      0,             // uint32_t elapsedUs
                      1              // int count
                      );


      int tube_irq_copy = tube_irq & ( RESET_BIT + NMI_BIT + IRQ_BIT);
      if (tube_irq_copy) {
         // Reset the processor on active edge of rst
         if ( tube_irq_copy & RESET_BIT ) {
            // Exit if the copro has changed
            if (copro != last_copro) {
               break;
            }
            copro_riscv_reset();
         }

         // TODO: Not sure how the MiniRV32 core implements interrupts yet

         // NMI is edge sensitive
         if (tube_irq_copy & NMI_BIT) {
            // arm2_execute_set_input(ARM_FIRQ_LINE, 1);
            tube_ack_nmi();
         }

         // IRQ is level sensitive, so check between every instruction
         //arm2_execute_set_input(ARM_IRQ_LINE, tube_irq_copy & IRQ_BIT);
      }
   }
}
