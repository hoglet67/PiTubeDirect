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

// single byte parasite -> host (e.g. *SAVE)
void type_0_data_transfer(void) {
  uint32_t mailbox, intr;
  // Terminate the data transfer if IRQ falls (e.g. interrupt from tube release)
  while (1) {
    // Wait for a mailbox message
    if (((*(uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(uint32_t *)MBOX0_READ) >> 4;
      intr = tube_io_handler(mailbox);
      // If there is an IRQ condition, terminate the transfer
      if (intr & 1) {
        return;
      }
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

// single byte host -> parasite (e.g. *LOAD)
void type_1_data_transfer(void) {
  uint32_t mailbox, intr;
  // Terminate the data transfer if IRQ falls (e.g. interrupt from tube release)
  while (1) {
    // Wait for a mailbox message
    if (((*(uint32_t *)MBOX0_STATUS) & MBOX0_EMPTY) == 0) {
      // Forward the message to the tube handler
      mailbox = (*(uint32_t *)MBOX0_READ) >> 4;
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
}

void type_3_data_transfer(void) {
}

void type_6_data_transfer(void) {
}

void type_7_data_transfer(void) {
}

void copro_armnative_tube_interrupt_handler(void) {

  // Check for R1 interrupt
  if (tubeRead(R1_STATUS) & A_BIT) {
    if (DEBUG_ARM) {
      printf("R1 irq\r\n");
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
      printf("R4 irq\r\n");
    }
    unsigned char type = tubeRead(R4_DATA);
    if (DEBUG_ARM) {
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
        if (DEBUG_ARM) {
          printf("Transfer = %02x %02x %08x\r\n", type, id, (unsigned int)address);
        }
      } else {
        if (DEBUG_ARM) {
          printf("Transfer = %02x %02x\r\n", type, id);
        }
      }
      if (type == 5) {
        // Type 5 : tube release
      } else {
        // Every thing else has a sync byte
        receiveByte(R4_ID);
      }
      // The data transfers are done by polling the GPIO bits for IRQ and NMI
      count = 0;
      signature = 0;
      switch (type) {
      case 0:
        type_0_data_transfer();
        break;
      case 1:
        type_1_data_transfer();
        break;
      case 2:
        type_2_data_transfer();
        break;
      case 3:
        type_3_data_transfer();
        break;
      case 6:
        type_6_data_transfer();
        break;
      case 7:
        type_7_data_transfer();
        break;
      }
      if (DEBUG_ARM) {
        printf("count = %0x signature = %0x\r\n", count, signature);
      }
    }
  }
}
