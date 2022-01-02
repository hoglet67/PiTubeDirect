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

volatile unsigned char *tube_address;
static unsigned int count;
static unsigned int signature;

static ErrorBlock_type isrErrorBlock;

#ifdef TUBE_ISR_STATE_MACHINE

static int ignore_transfer = 0;

void set_ignore_transfer(int on) {
   ignore_transfer = on;
}


// Host-to-Client transfers
// ------------------------
// Escape   R1: flag, b7=1
// Event    R1: &00 Y X A
// Error    R4: &FF R2: &00 err string &00
// Transfer R4: action ID block sync R3: data
//

typedef enum {
  IDLE_R1,         // 0
  EVENT_R1_Y,      // 1
  EVENT_R1_X,      // 2
  EVENT_R1_A,      // 3
} r1_state_type;

typedef enum {
  IDLE_R4,         // 0
  ERROR_R2_00,     // 1
  ERROR_R2_ERR,    // 2
  ERROR_R2_STRING, // 3
  TRANSFER_R4_ID,  // 4
  TRANSFER_R4_A3,  // 5
  TRANSFER_R4_A2,  // 6
  TRANSFER_R4_A1,  // 7
  TRANSFER_R4_A0,  // 8
  TRANSFER_R4_SYNC,// 9
  TRANSFER_R3      // 10
} r4_state_type;


void copro_armnative_tube_interrupt_handler(uint32_t mail) {
  static r1_state_type r1_state = IDLE_R1;
  static r4_state_type r4_state = IDLE_R4;

  static unsigned char type;
  static ErrorBlock_type *eblk;

  static int remaining;

  int addr;
  int rnw;
  int ntube;
  int nrst;

  addr = (mail>>8) & 7;
  rnw   = ( (mail >>11 ) & 1);
  nrst  = !((mail >> 12) & 1) ;
  ntube =  ((mail >> 12) & 1) ;

  // Handle a reset
  if (nrst == 0) {
    r1_state = IDLE_R1;
    r4_state = IDLE_R4;
    copro_armnative_reset();
    // This never returns as it uses longjmp
  }

  // State machine updates on tube write cycles

  if (nrst == 1 && ntube == 0 && rnw == 0 && addr == 1) {
    static unsigned char x;
    static unsigned char y;
    switch (r1_state) {

    case IDLE_R1: {
      // R1 interrupt
      unsigned char flag = tubeRead(R1_DATA);
      if (flag & 0x80) {
        // Escape
        // The escape handler is called with the escape flag value in R11
        // That's with this wrapper achieves
        unsigned int r11 = flag & 0x40;
        unsigned int r12 = env->handler[ESCAPE_HANDLER].r12;
        _escape_handler_wrapper(r11, r12, env->handler[ESCAPE_HANDLER].handler);
      } else {
        r1_state = EVENT_R1_Y;
      }
      break;
    }
    case EVENT_R1_Y:
      y = tubeRead(R1_DATA);
      r1_state = EVENT_R1_X;
      break;

    case EVENT_R1_X:
      x = tubeRead(R1_DATA);
      r1_state = EVENT_R1_A;
      break;

    case EVENT_R1_A:
      env->handler[EVENT_HANDLER].handler(tubeRead(R1_DATA), x, y);
      r1_state = IDLE_R1;
      break;
    }
  }

  if (nrst == 1 && ntube == 0 && rnw == 0) {
    static unsigned char id;
    static unsigned char a3;
    static unsigned char a2;
    static unsigned char a1;
    static char *emsg;

    switch (r4_state) {

    case IDLE_R4:
      if (addr == 7) {
        // R4 interrupt
        type = tubeRead(R4_DATA);
        if (type == 0xff) {
          r4_state = ERROR_R2_00;
        } else {
          r4_state = TRANSFER_R4_ID;
        }
      }
      break;

    case ERROR_R2_00:
      if (addr == 3) {
        tubeRead(R2_DATA); // Always 0
        r4_state = ERROR_R2_ERR;
      }
      break;

    case ERROR_R2_ERR:
      if (addr == 3) {
        // Build the error block
        eblk = &isrErrorBlock;
        emsg = eblk->errorMsg;
        eblk->errorNum = tubeRead(R2_DATA);
        r4_state = ERROR_R2_STRING;
      }
      break;

    case ERROR_R2_STRING:
      if (addr == 3) {
        unsigned char c = tubeRead(R2_DATA);
        *emsg++ = c;
        if (c) {
          r4_state = ERROR_R2_STRING;
        } else {
          r4_state = IDLE_R4;
          generate_error(0, eblk->errorNum, eblk->errorMsg);
          // generate_error() never returns
        }
      }
      break;

    case TRANSFER_R4_ID:
      if (addr == 7) {
        id = tubeRead(R4_DATA);
        if (type == 5) {
          if (DEBUG_TRANSFER) {
            printf("Transfer = %02x %02x\r\n", type, id);
          }
          r4_state = IDLE_R4;
        } else {
          r4_state = TRANSFER_R4_A3;
        }
      }
      break;

    case TRANSFER_R4_A3:
      if (addr == 7) {
        a3 = tubeRead(R4_DATA);
        r4_state = TRANSFER_R4_A2;
      }
      break;

    case TRANSFER_R4_A2:
      if (addr == 7) {
        a2 = tubeRead(R4_DATA);
        r4_state = TRANSFER_R4_A1;
      }
      break;

    case TRANSFER_R4_A1:
      if (addr == 7) {
        a1 = tubeRead(R4_DATA);
        r4_state = TRANSFER_R4_A0;
      }
      break;

    case TRANSFER_R4_A0:
      if (addr == 7) {
        unsigned char a0 = tubeRead(R4_DATA);
        tube_address = (unsigned char *)((a3 << 24) + (a2 << 16) + (a1 << 8) + a0);
        if (DEBUG_TRANSFER) {
          printf("Transfer = %02x %02x %08x\r\n", type, id, (unsigned int)tube_address);
        }
        r4_state = TRANSFER_R4_SYNC;
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
          // fall through
        case 0:
        case 2:
          r4_state = TRANSFER_R3;
          // For a copro->host transfer, send the first byte in response to the sync byte
          tubeWrite(R3_DATA, *tube_address);
          count++;
          signature += *tube_address++;
          signature *= 13;
          break;
        case 7:
          remaining = 256;
          // fall through
        case 1:
        case 3:
          r4_state = TRANSFER_R3;
          break;
        case 4:
        case 5:
          r4_state = IDLE_R4;
          break;
        default:
          printf("Unexpected transfer type %d\r\n", type);
          r4_state = IDLE_R4;
        }
      }
      break;

    case TRANSFER_R3:
      if (addr == 5) {
        if (type == 1 || type == 3 || (type == 7 && remaining > 0)) {
          // Read the R3 data register, which should also clear the NMI
          if (ignore_transfer) {
            signature += tubeRead(R3_DATA);
          } else {
            *tube_address = tubeRead(R3_DATA);
            signature += *tube_address;
          }
          tube_address++;
          count++;
          signature *= 13;
        }
        if (type == 7 && remaining > 0) {
          remaining--;
          if (remaining == 0) {
            r4_state = IDLE_R4;
          }
        }
      } else if (addr == 7) {
        // R4 interrupt
        if (DEBUG_TRANSFER_CRC) {
          printf("count = %0x signature = %0x\r\n", count, signature);
        }
        type = tubeRead(R4_DATA);
        if (type == 0xff) {
          r4_state = ERROR_R2_00;
        } else {
          r4_state = TRANSFER_R4_ID;
        }
      }
      break;
    }
  }

  // R4_State machine updates on tube read cycles

  if (nrst == 1 && ntube == 0 && rnw == 1) {
    if (r4_state == TRANSFER_R3) {
      if (addr == 5) {
        if (type == 0 || type == 2 || (type == 6 && remaining > 0)) {
          // Write the R3 data register, which should also clear the NMI
          tubeWrite(R3_DATA, *tube_address);
          count++;
          signature += *tube_address++;
          signature *= 13;
        }
        if (type == 6 && remaining > 0) {
          remaining--;
          if (remaining == 0) {
            r4_state = IDLE_R4;
          }
        }
      }
    }
  }
}


#else


// single byte parasite -> host (e.g. *SAVE)
static void type_0_data_transfer(void) {
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
        tubeWrite(R3_DATA, *tube_address);
        count++;
        signature += *tube_address++;
        signature *= 13;
      }
    }
  }
}

// single byte host -> parasite (e.g. *LOAD)
static void type_1_data_transfer(void) {
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
        *tube_address = tubeRead(R3_DATA);
        count++;
        signature += *tube_address++;
        signature *= 13;
      }
    }
  }
}

static void type_2_data_transfer(void) {
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
        tubeWrite(R3_DATA, *tube_address);
        count++;
        signature += *tube_address++;
        signature *= 13;
        // Write the R3 data register, which should also clear the NMI
        tubeWrite(R3_DATA, *tube_address);
        count++;
        signature += *tube_address++;
        signature *= 13;
      }
    }
  }
}

static void type_3_data_transfer(void) {
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
        *tube_address = tubeRead(R3_DATA);
        count++;
        signature += *tube_address++;
        signature *= 13;
        // Read the R3 data register, which should also clear the NMI
        *tube_address = tubeRead(R3_DATA);
        count++;
        signature += *tube_address++;
        signature *= 13;
      }
    }
  }
}

static void type_6_data_transfer(void) {
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
        tubeWrite(R3_DATA, *tube_address);
        count++;
        signature += *tube_address++;
        signature *= 13;
      }
    }
  }
}

static void type_7_data_transfer(void) {
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
        *tube_address = tubeRead(R3_DATA);
        count++;
        signature += *tube_address++;
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
       unsigned int r11 = flag & 0x40;
       unsigned int r12 = env->handler[ESCAPE_HANDLER].r12;
       _escape_handler_wrapper(r11, r12, env->handler[ESCAPE_HANDLER].handler);
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
        tube_address = (unsigned char *)((a3 << 24) + (a2 << 16) + (a1 << 8) + a0);
        if (DEBUG_TRANSFER) {
          printf("Transfer = %02x %02x %08x\r\n", type, id, (unsigned int)tube_address);
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
