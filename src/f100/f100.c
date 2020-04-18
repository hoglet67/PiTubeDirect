#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

#include "f100.h"

// Global Variables
static bool     verbose = false;
static cpu_t    cpu;
static uint16_t operand_address, target;
static uint16_t reset_vec;
static uint16_t irq0_vec;
static uint16_t irq1_vec;

// CPU Functions
void f100_init(uint16_t *memory, uint16_t pc_rst, uint16_t pc_irq0, uint16_t pc_irq1) {
  //cpu.mem = (uint16_t *) malloc(F100MEMSZ * sizeof(uint16_t));
  cpu.mem = memory;
  reset_vec = pc_rst;
  irq0_vec = pc_irq0;
  irq1_vec = pc_irq1;  
}

void f100_irq(int id) {
}

void f100_reset() {
  cpu.pc = reset_vec;
}


static void decode( uint16_t word) {
  cpu.ir.WORD = word;
  cpu.ir.F = (word>>12) & 0x000F ;
  cpu.ir.I = (word>>11) & 0x0001 ;
  cpu.ir.T = (word>>10) & 0x0003 ;
  cpu.ir.R = (word>> 8) & 0x0003 ;
  cpu.ir.S = (word>> 6) & 0x0003 ;
  cpu.ir.J = (word>> 4) & 0x0003 ;
  cpu.ir.B = word       & 0x000F ;
  cpu.ir.P = word       & 0x00FF ;
  cpu.ir.N = word       & 0x07FF ;
}

void f100_execute() {
  uint32_t result;
  uint16_t stack_pointer;
  uint16_t pointer;
  uint16_t operand_address;
  uint16_t operand1_address;
  uint16_t operand;


  do {
    // Fetch and decode operand
    decode(F100_READ_MEM(cpu.pc));
    INC_ADDR(cpu.pc,1);

    // Fetch additional Operands if required
    if ( cpu.ir.F==OP_F0 && cpu.ir.T==1) { // HALT
      break;
    } else if ( cpu.ir.F==1) {  // SJM - ignore all addressing bits
    } else if ( cpu.ir.F!=0 && cpu.ir.I==0 && cpu.ir.N!=0) { // Direct data (single word)
      operand_address = cpu.ir.N;
    } else if ( cpu.ir.F!=0 && cpu.ir.I==0 && cpu.ir.N==0) { // Immediate data (double word)
      operand_address = cpu.pc;
      INC_ADDR(cpu.pc,1);
    } else if ( cpu.ir.F!=0 && cpu.ir.I==1 && cpu.ir.P!=0) { // Pointer indirect (single word)
      pointer = TRUNC15(F100_READ_MEM(cpu.ir.P));
      if ( cpu.ir.R==1 ) INC_ADDR(pointer,1);
      operand_address = pointer;
      if ( cpu.ir.R==3 ) INC_ADDR(pointer,-1);
      F100_WRITE_MEM(cpu.ir.P, pointer);
    } else if (cpu.ir.F!=0 && cpu.ir.I==1 && cpu.ir.P==0) { // Immediate Indirect address (double word)
      FETCH15(cpu.mem, operand_address, cpu.pc);
    } else if (cpu.ir.F==0 && cpu.ir.T==0 && (cpu.ir.R==3||cpu.ir.S==2)) { // Shifts, bit manipulation and jumps
      FETCH15(cpu.mem, operand_address, cpu.pc);
      if (cpu.ir.S==2 && cpu.ir.R==3) { // Jumps (triple word)
        FETCH15(cpu.mem, operand1_address, cpu.pc);
      }
    }
    if (cpu.ir.F==OP_ICZ) { // ICZ takes an additional operand
      FETCH15(cpu.mem, operand1_address, cpu.pc);
    }

    // F100_Execute instruction
    switch ( cpu.ir.F ) {
    case OP_F0:
      if (cpu.ir.T==0 && cpu.ir.S<2) { // Shifts
        int i;
        if (cpu.ir.R==1) cpu.or = PACK_FLAGS;
        else if (cpu.ir.R==3) cpu.or = F100_READ_MEM(operand_address);
        if ( cpu.M==0 ) { // Single Length
          uint8_t  places = cpu.ir.B;
          if (cpu.ir.R==1 || cpu.ir.R==3) operand = cpu.or;
          else operand = cpu.acc;
          // S=Direction, J=0,1:Arith, 2:Logical, 3:Rotate, B=Num Places
          if (cpu.ir.S) {
            result = TRUNC16(operand<<places);
          } else if (cpu.ir.J<2) {
            result = TRUNC16((int16_t)operand>>places);
          } else {
            result = TRUNC16(operand>>places);
          }
          COMPUTE_SV(result,operand,operand);
          cpu.acc = TRUNC16(result);          
          if (cpu.ir.R==1) {
            // Overwrite flags with the shifted value
            UNPACK_FLAGS(cpu.acc);
          } 
        } else { // Double Length
          uint8_t  places = (((cpu.ir.J&1)<<4) + cpu.ir.B);
          // Assemble acc+OR into 32bit result register to start shifting
          result = (cpu.acc<<16) | cpu.or;
          // S=Direction, J=0,1:Arith, 2:Logical, B=Num Places
          if (cpu.ir.S) {
            result = (result << places) & 0xFFFFFFFF;
          } else if ( cpu.ir.J<2 ) {
            // Shift as a signed value to get arithmetic shift
            result =  ((int32_t) result>>places) & 0xFFFFFFFF;
          } else {
            result = result>>places;
          }
          COMPUTE_SV(TRUNC16(result>>16), cpu.acc, cpu.acc);
          cpu.acc = TRUNC16((result>>16));
          cpu.or = TRUNC16(result);
          if (cpu.ir.R==1) {
            // Overwrite flags with the shifted value, low word (in OR)
            UNPACK_FLAGS(cpu.or);
          } 
        }
      } else if ( cpu.ir.T==0 && cpu.ir.S>1) { //  Bit conditional jumps and Bit manipulation
        uint16_t bmask;
        uint16_t jump_address;
        if (cpu.ir.R==3) {
          operand = (cpu.or=F100_READ_MEM(operand_address));
          jump_address = operand1_address;
        } else if (cpu.ir.R==1) {
          operand = PACK_FLAGS;
          jump_address = operand_address;
        } else {
          operand=cpu.acc;
          jump_address=operand_address;
        }
        bmask = (1<<cpu.ir.B);
        if ( cpu.ir.S==2 ) { // Jumps only
          if ( cpu.ir.J==0 || cpu.ir.J==2) { // Jump on bit clear
            cpu.pc = (!(operand & bmask))? jump_address: cpu.pc;
          } else { // Jump on bit set
            cpu.pc = (operand & bmask)? jump_address: cpu.pc;
          }
        }
        if (cpu.ir.J>1) { // Set or clear bit
          result = ( cpu.ir.J==2) ? (operand | bmask) : (operand & ~bmask);
          if (cpu.ir.R==3) {
            cpu.or = TRUNC16(result);
            F100_WRITE_MEM(operand_address, cpu.or);
          } else if ( cpu.ir.R==1) {
            UNPACK_FLAGS (result);
          } else {
            cpu.acc=result;
          }
        }
      }
      break;
    case OP_SJM:
      INC_ADDR( cpu.pc, cpu.acc ) ;
      break;
    case OP_CAL:
      // Special case for immed. addr - push imm data addr onto stack and set it to be PC also
      stack_pointer = F100_READ_MEM( LSP);
      if ( cpu.ir.I==0 && cpu.ir.N==0) cpu.pc = TRUNC15(operand_address);
      F100_WRITE_MEM(TRUNC15(stack_pointer+1), cpu.pc);
      F100_WRITE_MEM(TRUNC15(stack_pointer+2), PACK_FLAGS);
      F100_WRITE_MEM(LSP, TRUNC15(stack_pointer+2));
      cpu.pc = TRUNC15(operand_address);
      CLEAR_MULTI ;
      break;
    case OP_ICZ:
      cpu.acc = 1+F100_READ_MEM(operand_address);
      F100_WRITE_MEM(operand_address, cpu.acc);
      if (cpu.acc!=0) cpu.pc=operand1_address ;
      break;
    case OP_JMP:
      cpu.pc = TRUNC15(operand_address);
      break;
    case OP_RTN_RTC:
      stack_pointer = F100_READ_MEM( LSP);
      cpu.pc = TRUNC15(F100_READ_MEM( stack_pointer-1));
      // restore bits 0-5 from stack, retain others
      if (cpu.ir.I == 0) {
        uint16_t flags, new_flags;
        flags = PACK_FLAGS;
        new_flags = F100_READ_MEM(stack_pointer);
        flags = (new_flags & 0x3F) | (flags & 0xC0) ;
        UNPACK_FLAGS(flags);
      }
      F100_WRITE_MEM(LSP, TRUNC15(stack_pointer-2));
      break;
    case OP_LDA:
      cpu.acc = F100_READ_MEM(operand_address);
      COMPUTE_SZ(cpu.acc) ;
      CLEAR_OVERFLOW ;
      break;
    case OP_STO:
      F100_WRITE_MEM(operand_address, cpu.acc);
      COMPUTE_SZ(cpu.acc) ;
      CLEAR_OVERFLOW ;
      break;
    case OP_SBS:
    case OP_SUB:
    case OP_CMP:
      cpu.or = F100_READ_MEM(operand_address);
      result = cpu.or - cpu.acc;
      if (cpu.M) result += cpu.C-1;
      COMPUTE_BORROW(result) ;
      COMPUTE_SVZ_SUB(result, cpu.or, cpu.acc) ;        
      if (cpu.ir.F==OP_SBS) F100_WRITE_MEM(operand_address, TRUNC16(result));
      if (cpu.ir.F!=OP_CMP) cpu.acc=TRUNC16(result);
      break;
    case OP_ADS:
    case OP_ADD:
      cpu.or = F100_READ_MEM(operand_address);
      result = cpu.acc + cpu.or;
      if (cpu.M) result += cpu.C;
      COMPUTE_CARRY(result) ;
      COMPUTE_SVZ_ADD(result, cpu.acc, cpu.or) ;
      cpu.acc=TRUNC16(result);      
      if (cpu.ir.F==OP_ADS) F100_WRITE_MEM(operand_address, TRUNC16(result));
      break;
    case OP_AND:
      cpu.or = F100_READ_MEM(operand_address);
      cpu.acc = cpu.acc&cpu.or;
      SET_CARRY ;
      COMPUTE_SZ(cpu.acc);
      break;
    case OP_NEQ:
      cpu.or = F100_READ_MEM(operand_address);
      cpu.acc = cpu.acc^cpu.or;
      CLEAR_CARRY;
      COMPUTE_SZ(cpu.acc);
      break;  
   default: break;
    }
  } while  (tubeContinueRunning());
//  if (HALT(cpu.ir)) printf("CPU Halted with halt number 0x%04X\n", cpu.ir.WORD & 0x03FF);
}
