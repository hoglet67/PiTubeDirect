#include <stdio.h>
#include "rpi-aux.h"
#include "rpi-base.h"
#include "rpi-gpio.h"
#include "info.h"
#include "startup.h"
#include "stdlib.h"
#include "framebuffer/framebuffer.h"

#ifdef INCLUDE_DEBUGGER
#include "debugger/debugger.h"
#endif

/* Define the system clock frequency in MHz for the baud rate calculation.
 This is clearly defined on the BCM2835 datasheet errata page:
 http://elinux.org/BCM2835_datasheet_errata */
#define FALLBACK_SYS_FREQ    250000000

#define USE_IRQ

#define TX_BUFFER_SIZE 65536  // Must be a power of 2

static aux_t* auxiliary = (aux_t*) AUX_BASE;

#include "rpi-interrupts.h"

#ifdef USE_IRQ
static char *tx_buffer;
static volatile int tx_head;
static volatile int tx_tail;
#endif // USE_IRQ

// There is a GCC bug with __attribute__((interrupt("IRQ"))) in that it
// does not respect registers reserved with -ffixed-reg.
//
// So instead, we wrap the C handler in a few lines of assembler:
//
//_main_irq_handler:
//        sub     lr, lr, #4
//        push    {r0, r1, r2, r3, ip, lr}
//        bl      RPI_AuxMiniUartIRQHandler
//        ldm     sp!, {r0, r1, r2, r3, ip, pc}^

void RPI_AuxMiniUartIRQHandler() {

  _data_memory_barrier();
  RPI_SetGpioHi(TEST3_PIN);

#ifdef USE_IRQ
  _data_memory_barrier();

  while (1) {

    uint32_t iir = auxiliary->MU_IIR;

    if (iir & AUX_MUIIR_INT_NOT_PENDING) {
      /* No more interrupts */
      break;
    }

    /* Handle RxReady interrupt */
    if (iir & AUX_MUIIR_INT_IS_RX) {
#ifdef INCLUDE_DEBUGGER
      /* Forward all received characters to the debugger */
      debugger_rx_char(auxiliary->MU_IO & 0xFF);
#else
      /* Else just exho characters */
      RPI_AuxMiniUartWrite(auxiliary->MU_IO & 0xFF);
#endif
    }

    /* Handle TxEmpty interrupt */
    if (iir & AUX_MUIIR_INT_IS_TX) {
      if (tx_tail != tx_head) {
        /* Transmit the character */
        tx_tail = (tx_tail + 1) & (TX_BUFFER_SIZE - 1);
        auxiliary->MU_IO = tx_buffer[tx_tail];
      } else {
        /* Disable TxEmpty interrupt */
        auxiliary->MU_IER &= (uint32_t)~AUX_MUIER_TX_INT;
      }
    }
  }
#endif // USE_IRQ

  // Periodically also process the VDU Queue
  fb_process_vdu_queue();

  _data_memory_barrier();

  RPI_SetGpioLo(TEST3_PIN);

  _data_memory_barrier();
}

void RPI_AuxMiniUartInit(uint32_t baud, uint32_t bits)
{
  // Data memory barrier need to be places between accesses to different peripherals
  //
  // See page 7 of the BCM2853 manual

  _data_memory_barrier();

  clock_info_t *sys_clock_info = get_clock_rates(CORE_CLK_ID);

  uint32_t sys_freq = sys_clock_info->rate;

  // Sanity-check against zero
  if (!sys_freq) {
     sys_freq = FALLBACK_SYS_FREQ;
  }

  _data_memory_barrier();

  /* Setup GPIO 14 and 15 as alternative function 5 which is
   UART 1 TXD/RXD. These need to be set before enabling the UART */
  RPI_SetGpioPinFunction(RPI_GPIO14, FS_ALT5);
  RPI_SetGpioPinFunction(RPI_GPIO15, FS_ALT5);

  RPI_SetGpioPull(RPI_GPIO14, PULL_UP);
  RPI_SetGpioPull(RPI_GPIO15, PULL_UP);

  _data_memory_barrier();

  /* As this is a mini uart the configuration is complete! Now just
   enable the uart. Note from the documentation in section 2.1.1 of
   the ARM peripherals manual:

   If the enable bits are clear you will have no access to a
   peripheral. You can not even read or write the registers */
  auxiliary->ENABLES = AUX_ENA_MINIUART;

  _data_memory_barrier();

  /* Disable flow control,enable transmitter and receiver! */
  auxiliary->MU_CNTL = 0;

  /* Decide between seven or eight-bit mode */
  if (bits == 8)
    auxiliary->MU_LCR = AUX_MULCR_8BIT_MODE;
  else
    auxiliary->MU_LCR = 0;

  auxiliary->MU_MCR = 0;

  /* Disable all interrupts from MU and clear the fifos */
  auxiliary->MU_IER = 0;
  auxiliary->MU_IIR = 0xC6;

  /* Transposed calculation from Section 2.2.1 of the ARM peripherals manual */
  auxiliary->MU_BAUD = (( sys_freq / (8 * baud)) - 1);

  extern unsigned int _interrupt_vector_h;
  _interrupt_vector_h = (uint32_t) _main_irq_handler;

#ifdef USE_IRQ
  {
    tx_buffer = malloc(TX_BUFFER_SIZE);
    tx_head = tx_tail = 0;
    _data_memory_barrier();
    RPI_GetIrqController()->Enable_IRQs_1 = (1 << 29);
    _data_memory_barrier();
    auxiliary->MU_IER |= AUX_MUIER_RX_INT;
  }
#endif

  _data_memory_barrier();

  /* Disable flow control,enable transmitter and receiver! */
  auxiliary->MU_CNTL = AUX_MUCNTL_TX_ENABLE | AUX_MUCNTL_RX_ENABLE;

  _data_memory_barrier();

}

void RPI_AuxMiniUartWrite(char c)
{
#ifdef USE_IRQ
  int tmp_head = (tx_head + 1) & (TX_BUFFER_SIZE - 1);

  /* Test if the buffer is full */
  if (tmp_head == tx_tail) {
     uint32_t cpsr = _get_cpsr();
     if (cpsr & 0x80) {
        /* IRQ disabled: drop the character to avoid deadlock */
        return;
     } else {
        /* IRQ enabled: wait for space in buffer */
        while (tmp_head == tx_tail) {
        }
     }
  }
  /* Buffer the character */
  tx_buffer[tmp_head] = c;
  tx_head = tmp_head;

  _data_memory_barrier();

  /* Enable TxEmpty interrupt */
  auxiliary->MU_IER |= AUX_MUIER_TX_INT;

  _data_memory_barrier();

#else
  /* Wait until the UART has an empty space in the FIFO */
  while ((auxiliary->MU_LSR & AUX_MULSR_TX_EMPTY) == 0)
  {
  }
  /* Write the character to the FIFO for transmission */
  auxiliary->MU_IO = c;
#endif
}

extern void RPI_EnableUart(const char* pMessage)
{
  RPI_AuxMiniUartInit(115200, 8);          // Initialise the UART

  printf(pMessage);
}
