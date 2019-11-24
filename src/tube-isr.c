#include <stdio.h>
#include <stdlib.h>

#include "copro-armnative.h"

#include "startup.h"
#include "swi.h"
#include "tube-lib.h"
#include "tube-env.h"
#include "tube-swi.h"
#include "tube-ula.h"
#include "tube-isr.h"

volatile unsigned char *address;
volatile unsigned int count;
volatile unsigned int signature;

ErrorBlock_type isrErrorBlock;

#ifdef TUBE_ISR_STATE_MACHINE

// Host-to-Client transfers
// ------------------------
// Escape   R1: flag, b7=1
// Event    R1: &00 Y X A
// Error    R4: &FF R2: &00 err string &00
// Transfer R4: action ID block sync R3: data
//
// idle
// event_r1_y
// event_r1_x
// event_r1_a
// error_r2_00
// error_r2_err
// error_r2_string
// transfer_r4_id
// transfer_r4_a3
// transfer_r4_a2
// transfer_r4_a1
// transfer_r4_a0
// transfer_r4_sync
// transfer_r3

typedef enum {
  IDLE,            // 0
  EVENT_R1_Y,      // 1
  EVENT_R1_X,      // 2
  EVENT_R1_A,      // 3
  ERROR_R2_00,     // 4
  ERROR_R2_ERR,    // 5
  ERROR_R2_STRING, // 6
  TRANSFER_R4_ID,  // 7
  TRANSFER_R4_A3,  // 8
  TRANSFER_R4_A2,  // 9
  TRANSFER_R4_A1,  // 10
  TRANSFER_R4_A0,  // 11
  TRANSFER_R4_SYNC,// 12
  TRANSFER_R3      // 13
} tih_state_type;

void copro_armnative_tube_interrupt_handler(uint32_t mail) {
  static tih_state_type state = IDLE;
  static unsigned char a;
  static unsigned char x;
  static unsigned char y;
  static unsigned char type;
  static ErrorBlock_type *eblk;
  static char *emsg;
  static unsigned char id;
  static unsigned char a3;
  static unsigned char a2;
  static unsigned char a1;
  static unsigned char a0;
  static int remaining;

  int addr;
  int rnw;
  int ntube;
  int nrst;
  int unexpected = 0;

  addr = (mail>>8) & 7;
  rnw   = ( (mail >>11 ) & 1);
  nrst  = !((mail >> 12) & 1) ;
  ntube =  ((mail >> 12) & 1) ;

  // Handle a reset
  if (nrst == 0) {
    state = IDLE;
    copro_armnative_reset();
    // This never returns as it uses longjmp
  }

  // State machine updates on tube write cycles

  if (nrst == 1 && ntube == 0 && rnw == 0) {

    switch (state) {

    case IDLE:
      if (addr == 1) {
        // R1 interrupt
        unsigned char flag = tubeRead(R1_DATA);
        if (flag & 0x80) {
          // Escape
          // The escape handler is called with the escape flag value in R11
          // That's with this wrapper achieves
          _escape_handler_wrapper(flag & 0x40, env->handler[ESCAPE_HANDLER].handler);
        } else {
          state = EVENT_R1_Y;
        }
      } else if (addr == 7) {
        // R4 interrupt
        type = tubeRead(R4_DATA);
        if (type == 0xff) {
          state = ERROR_R2_00;
        } else {
          state = TRANSFER_R4_ID;
        }
      }
      break;

    case EVENT_R1_Y:
      if (addr == 1) {
        y = tubeRead(R1_DATA);
        state = EVENT_R1_X;
      } else {
        unexpected = 1;
      }
      break;

    case EVENT_R1_X:
      if (addr == 1) {
        x = tubeRead(R1_DATA);
        state = EVENT_R1_A;
      } else {
        unexpected = 1;
      }
      break;

    case EVENT_R1_A:
      if (addr == 1) {
        a = tubeRead(R1_DATA);
        env->handler[EVENT_HANDLER].handler(a, x, y);
        state = IDLE;
      } else {
        unexpected = 1;
      }
      break;

    case ERROR_R2_00:
      if (addr == 3) {
        tubeRead(R2_DATA); // Always 0
        state = ERROR_R2_ERR;
      } else {
        unexpected = 1;
      }
      break;

    case ERROR_R2_ERR:
      if (addr == 3) {
        // Build the error block
        eblk = &isrErrorBlock;
        emsg = eblk->errorMsg;
        eblk->errorNum = tubeRead(R2_DATA);
        state = ERROR_R2_STRING;
      } else {
        unexpected = 1;
      }
      break;

    case ERROR_R2_STRING:
      if (addr == 3) {
        unsigned char c = tubeRead(R2_DATA);
        *emsg++ = c;
        if (c) {
          state = ERROR_R2_STRING;
        } else {
          state = IDLE;
          // SWI OS_GenerateError need the error block in R0
          OS_GenerateError(eblk);
          // OS_GenerateError() probably never returns
        }
      } else {
        unexpected = 1;
      }
      break;

    case TRANSFER_R4_ID:
      if (addr == 7) {
        id = tubeRead(R4_DATA);
        if (type == 5) {
          if (DEBUG_TRANSFER) {
            printf("Transfer = %02x %02x\r\n", type, id);
          }
          state = IDLE;
        } else {
          state = TRANSFER_R4_A3;
        }
      } else {
        unexpected = 1;
      }
      break;

    case TRANSFER_R4_A3:
      if (addr == 7) {
        a3 = tubeRead(R4_DATA);
        state = TRANSFER_R4_A2;
      } else {
        unexpected = 1;
      }
      break;

    case TRANSFER_R4_A2:
      if (addr == 7) {
        a2 = tubeRead(R4_DATA);
        state = TRANSFER_R4_A1;
      } else {
        unexpected = 1;
      }
      break;

    case TRANSFER_R4_A1:
      if (addr == 7) {
        a1 = tubeRead(R4_DATA);
        state = TRANSFER_R4_A0;
      } else {
        unexpected = 1;
      }
      break;

    case TRANSFER_R4_A0:
      if (addr == 7) {
        a0 = tubeRead(R4_DATA);
        address = (unsigned char *)((a3 << 24) + (a2 << 16) + (a1 << 8) + a0);
        if (DEBUG_TRANSFER) {
          printf("Transfer = %02x %02x %08x\r\n", type, id, (unsigned int)address);
        }
        state = TRANSFER_R4_SYNC;
      } else {
        unexpected = 1;
      }
      break;

    case TRANSFER_R4_SYNC:
      if (addr == 7) {
        tubeRead(R4_DATA);
        count = 0;
        signature = 0;
        switch (type) {
        case 6:
          remaining = 256;
          // fall though to...
        case 0:
        case 2:
          state = TRANSFER_R3;
          // For a copro->host transfer, send the first byte in response to the sync byte
          tubeWrite(R3_DATA, *address);
          count++;
          signature += *address++;
          signature *= 13;
          break;
        case 7:
          remaining = 256;
          // fall though to...
        case 1:
        case 3:
          state = TRANSFER_R3;
          break;
        case 4:
        case 5:
          state = IDLE;
          break;
        default:
          printf("Unexpected transfer type %d\r\n", type);
          state = IDLE;
        }
      } else if (addr == 0) {
        // A write to the tube control register is expected at this point
      } else {
        unexpected = 1;
      }
      break;

    case TRANSFER_R3:
      if (addr == 5) {
        if (type == 1 || type == 3 || (type == 7 && remaining > 0)) {
          // Read the R3 data register, which should also clear the NMI
          *address = tubeRead(R3_DATA);
          count++;
          signature += *address++;
          signature *= 13;
        }
        if (type == 7 && remaining > 0) {
          remaining--;
          if (remaining == 0) {
            state = IDLE;
          }
        }
      } else if (addr == 7) {
        // R4 interrupt
        if (DEBUG_TRANSFER_CRC) {
          printf("count = %0x signature = %0x\r\n", count, signature);
        }
        type = tubeRead(R4_DATA);
        if (type == 0xff) {
          state = ERROR_R2_00;
        } else {
          state = TRANSFER_R4_ID;
        }
      }
      break;
    }
  }

  // State machine updates on tube read cycles

  if (nrst == 1 && ntube == 0 && rnw == 1) {

    switch (state) {

    case TRANSFER_R3:
      if (addr == 5) {
        if (type == 0 || type == 2 || (type == 6 && remaining > 0)) {
          // Write the R3 data register, which should also clear the NMI
          tubeWrite(R3_DATA, *address);
          count++;
          signature += *address++;
          signature *= 13;
        }
        if (type == 6 && remaining > 0) {
          remaining--;
          if (remaining == 0) {
            state = IDLE;
          }
        }
      }
      break;
    default:
      break;

    }

  }

  if (unexpected) {
    if (rnw == 0) {
      printf("Unexpected write to %d in state %d\r\n", addr, state);
    } else {
      printf("Unexpected read from %d in state %d\r\n", addr, state);
    }
    state = IDLE;
  }
}


#else


// single byte parasite -> host (e.g. *SAVE)
void type_0_data_transfer(void) {
  uint32_t mailbox, intr;
  // Terminate the data transfer if IRQ falls (e.g. interrupt from tube release)
  while (1) {
    // Wait for a mailbox message
    if (((*(volatile uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(volatile uint32_t *)MBOX0_READ) >> 4;
      intr = tube_io_handler(mailbox);
      // If there is an IRQ condition, terminate the transfer
      if (intr & 1) {
        return;
      }
      // If there is an NMI condition, handle the byte
      if (intr & 2) {
        // Write the R3 data register, which should also clear the NMI
        tubeWrite(R3_DATA, *address);
        count++;
        signature += *address++;
        signature *= 13;
      }
    }
  }
}

// single byte host -> parasite (e.g. *LOAD)
void type_1_data_transfer(void) {
  uint32_t mailbox, intr;
  // Terminate the data transfer if IRQ falls (e.g. interrupt from tube release)
  while (1) {
    // Wait for a mailbox message
    if (((*(volatile uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(volatile uint32_t *)MBOX0_READ) >> 4;
      intr = tube_io_handler(mailbox);
      // If there is an IRQ condition, terminate the transfer
      if (intr & 1) {
        return;
      }
      // If there is an NMI condition, handle the byte
      if (intr & 2) {
        // Read the R3 data register, which should also clear the NMI
        *address = tubeRead(R3_DATA);
        count++;
        signature += *address++;
        signature *= 13;
      }
    }
  }
}

void type_2_data_transfer(void) {
  uint32_t mailbox, intr;
  // Terminate the data transfer if IRQ falls (e.g. interrupt from tube release)
  while (1) {
    // Wait for a mailbox message
    if (((*(volatile uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(volatile uint32_t *)MBOX0_READ) >> 4;
      intr = tube_io_handler(mailbox);
      // If there is an IRQ condition, terminate the transfer
      if (intr & 1) {
        return;
      }
      // If there is an NMI condition, handle the byte
      if (intr & 2) {
        // Write the R3 data register, which should also clear the NMI
        tubeWrite(R3_DATA, *address);
        count++;
        signature += *address++;
        signature *= 13;
        // Write the R3 data register, which should also clear the NMI
        tubeWrite(R3_DATA, *address);
        count++;
        signature += *address++;
        signature *= 13;
      }
    }
  }
}

void type_3_data_transfer(void) {
  uint32_t mailbox, intr;
  // Terminate the data transfer if IRQ falls (e.g. interrupt from tube release)
  while (1) {
    // Wait for a mailbox message
    if (((*(volatile uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(volatile uint32_t *)MBOX0_READ) >> 4;
      intr = tube_io_handler(mailbox);
      // If there is an IRQ condition, terminate the transfer
      if (intr & 1) {
        return;
      }
      // If there is an NMI condition, handle the byte
      if (intr & 2) {
        // Read the R3 data register, which should also clear the NMI
        *address = tubeRead(R3_DATA);
        count++;
        signature += *address++;
        signature *= 13;
        // Read the R3 data register, which should also clear the NMI
        *address = tubeRead(R3_DATA);
        count++;
        signature += *address++;
        signature *= 13;
      }
    }
  }
}

void type_6_data_transfer(void) {
  uint32_t mailbox, intr;
  int i;
  for (i = 0; i < 256; i++) {
    // Wait for a mailbox message
    if (((*(volatile uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(volatile uint32_t *)MBOX0_READ) >> 4;
      intr = tube_io_handler(mailbox);
      // If there is an NMI condition, handle the byte
      if (intr & 2) {
        // Read the R3 data register, which should also clear the NMI
        tubeWrite(R3_DATA, *address);
        count++;
        signature += *address++;
        signature *= 13;
      }
    }
  }
}

void type_7_data_transfer(void) {
  uint32_t mailbox, intr;
  int i;
  for (i = 0; i < 256; i++) {
    // Wait for a mailbox message
    if (((*(volatile uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(volatile uint32_t *)MBOX0_READ) >> 4;
      intr = tube_io_handler(mailbox);
      // If there is an NMI condition, handle the byte
      if (intr & 2) {
        // Read the R3 data register, which should also clear the NMI
        *address = tubeRead(R3_DATA);
        count++;
        signature += *address++;
        signature *= 13;
      }
    }
  }
}

void copro_armnative_tube_interrupt_handler(void) {

  // Check for R1 interrupt
  if (tubeRead(R1_STATUS) & A_BIT) {
    if (DEBUG_ARM) {
      printf("R1 irq, cpsr=%08x\r\n", _get_cpsr());
    }
    unsigned char flag = tubeRead(R1_DATA);
    if (flag & 0x80) {
      // Escape
      // The escape handler is called with the escape flag value in R11
      // That's with this wrapper achieves
      _escape_handler_wrapper(flag & 0x40, env->handler[ESCAPE_HANDLER].handler);
    } else {
      // Event
      unsigned char y = receiveByte(R1_ID);
      unsigned char x = receiveByte(R1_ID);
      unsigned char a = receiveByte(R1_ID);
      env->handler[EVENT_HANDLER].handler(a, x, y);
    }
  }

  // Check for R4 interrupt
  if (tubeRead(R4_STATUS) & A_BIT) {
    if (DEBUG_ARM) {
      printf("R4 irq, cpsr=%08x\r\n", _get_cpsr());
    }
    unsigned char type = tubeRead(R4_DATA);
    if (DEBUG_TRANSFER) {
      printf("R4 type = %02x\r\n",type);
    }
    if (type == 0xff) {
      receiveByte(R2_ID); // always 0
      // Build the error block
      ErrorBlock_type *eblk = &isrErrorBlock;
      eblk->errorNum = receiveByte(R2_ID);
      receiveString(R2_ID, 0x00, eblk->errorMsg);
      // SWI OS_GenerateError need the error block in R0
      OS_GenerateError(eblk);
    } else {
      unsigned char id = receiveByte(R4_ID);
      if (type <= 4 || type == 6 || type == 7) {
        unsigned char a3 = receiveByte(R4_ID);
        unsigned char a2 = receiveByte(R4_ID);
        unsigned char a1 = receiveByte(R4_ID);
        unsigned char a0 = receiveByte(R4_ID);
        address = (unsigned char *)((a3 << 24) + (a2 << 16) + (a1 << 8) + a0);
        if (DEBUG_TRANSFER) {
          printf("Transfer = %02x %02x %08x\r\n", type, id, (unsigned int)address);
        }
      } else {
        if (DEBUG_TRANSFER) {
          printf("Transfer = %02x %02x\r\n", type, id);
        }
      }
      if (type == 5) {
        // Type 5 : tube release
      } else {
        // Every thing else has a sync byte
        receiveByte(R4_ID);
      }
      // The data transfers are done by polling the mailbox directly
      // so disable interrupts to prevent the FIQ handler reading the mailbox
      count = 0;
      signature = 0;
      switch (type) {
      case 0:
        _disable_interrupts();
        type_0_data_transfer();
        _enable_interrupts();
        break;
      case 1:
        _disable_interrupts();
        type_1_data_transfer();
        _enable_interrupts();
        break;
      case 2:
        _disable_interrupts();
        type_2_data_transfer();
        _enable_interrupts();
        break;
      case 3:
        _disable_interrupts();
        type_3_data_transfer();
        _enable_interrupts();
        break;
      case 6:
        type_6_data_transfer();
        break;
      case 7:
        type_7_data_transfer();
        break;
      }
      if (DEBUG_TRANSFER_CRC) {
        printf("count = %0x signature = %0x\r\n", count, signature);
      }
      // type 0..3 data transfers will be terminated by an interrupt, so call
      // ourselves recursively or this will be not be processed
      if (type < 4) {
        copro_armnative_tube_interrupt_handler();
      }
    }
  }
}

#endif
