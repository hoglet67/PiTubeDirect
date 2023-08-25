#include "rpi-auxreg.h"
#include "rpi-gpio.h"
#include "info.h"
#include "rpi-asm-helpers.h"
#include "tube-pins.h"
#include <limits.h>

#ifdef INCLUDE_DEBUGGER
#include "debugger/debugger.h"
#endif

/* Define the system clock frequency in MHz for the baud rate calculation.
 This is clearly defined on the BCM2835 datasheet errata page:
 http://elinux.org/BCM2835_datasheet_errata */
#define FALLBACK_SYS_FREQ    250000000

#define USE_IRQ

#define TX_BUFFER_SIZE (1<<22)  // Must be a power of 2

static aux_t* auxiliary = (aux_t*) AUX_BASE;

#ifdef USE_IRQ

#include "rpi-interrupts.h"
__attribute__ ((section (".noinit"))) static volatile char tx_buffer[TX_BUFFER_SIZE];
static volatile int tx_head;
static volatile int tx_tail;
#endif // USE_IRQ

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

void RPI_UnbufferedWrite(char c)
{  /* Wait until the UART has an empty space in the FIFO */
  while ((auxiliary->MU_LSR & AUX_MULSR_TX_EMPTY) == 0)
  {
  }
  /* Write the character to the FIFO for transmission */
  auxiliary->MU_IO = c;
}

int RPI_AuxMiniUartString(const char *c, int len)
{
#ifdef USE_IRQ
  int num = len-1;
  int count = 0;
  if ( len == 0 ) num = INT_MAX;
  do {
    int tmp_head = (tx_head + 1) & (TX_BUFFER_SIZE - 1);

    /* Test if the buffer is full */
    if (tmp_head == tx_tail) {
      uint32_t cpsr = _get_cpsr();
      if (cpsr & 0x80) {
          /* IRQ disabled: drop the character to avoid deadlock */
          return count;
      } else {
          /* IRQ enabled: wait for space in buffer */
          while (tmp_head == tx_tail) {
          }
      }
    }
    /* Buffer the character */
    tx_buffer[tmp_head] = *c++;
    tx_head = tmp_head;
    count++;
    if ( (*c==0) && ( len == 0 ) )
      break;
  } while (num--);

  _data_memory_barrier();

  /* Enable TxEmpty interrupt */
  auxiliary->MU_IER |= AUX_MUIER_TX_INT;

  _data_memory_barrier();
#else
  int num = len-1;
  int count = 0;
  if ( len == 0 ) num = INT_MAX;
  do {
  /* Wait until the UART has an empty space in the FIFO */
  while ((auxiliary->MU_LSR & AUX_MULSR_TX_EMPTY) == 0)
  {
  }
  /* Write the character to the FIFO for transmission */
  auxiliary->MU_IO = *c++;
    count++;
  if ( (*c==0) && ( len == 0 ) )
    break;
  } while (num--);
#endif
  return count;
}

int RPI_UnbufferedString( const char *c, int len)
{
  int num = len-1;
  int count = 0;
  if ( len == 0 ) num = INT_MAX;
  do {
  /* Wait until the UART has an empty space in the FIFO */
  while ((auxiliary->MU_LSR & AUX_MULSR_TX_EMPTY) == 0)
  {
  }
  /* Write the character to the FIFO for transmission */
  auxiliary->MU_IO = *c++;
    count++;
  if ( (*c==0) && ( len == 0 ) )
    break;
  } while (num--);
  return count;
}


void RPI_AuxMiniUartIRQHandler() {

  //_data_memory_barrier();
  //RPI_SetGpioHi(TEST3_PIN);

#ifdef USE_IRQ
  _data_memory_barrier();

  while (1) {

    uint8_t iir = auxiliary->MU_IIR;

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
      /* Else just echo characters */
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
        auxiliary->MU_IER &= (rpi_reg_byte_rw_t)~AUX_MUIER_TX_INT;
      }
    }
  }
#endif // USE_IRQ

 // RPI_SetGpioLo(TEST3_PIN);

 // _data_memory_barrier();
}

void RPI_AuxMiniUartInit(uint32_t baud)
{
  // Data memory barrier need to be places between accesses to different peripherals
  //
  // See page 7 of the BCM2853 manual

  _data_memory_barrier();

  const clock_info_t * const sys_clock_info = get_clock_rates(CORE_CLK_ID);

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

  /* Eight-bit mode */
  auxiliary->MU_LCR = AUX_MULCR_8BIT_MODE;

  auxiliary->MU_MCR = 0;

  /* Disable all interrupts from MU and clear the fifos */
  auxiliary->MU_IER = 0;
  auxiliary->MU_IIR = 0xC6;

  /* Transposed calculation from Section 2.2.1 of the ARM peripherals manual */
  auxiliary->MU_BAUD = (( sys_freq / (8 * baud)) - 1);

#ifdef USE_IRQ
  {
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


void dump_binary(unsigned int value, int unbuffered) {
  for (int i = 0; i < 32; i++) {
    if (unbuffered)
      RPI_UnbufferedWrite((uint8_t)('0' + (value >> 31)));
    else
      RPI_AuxMiniUartWrite((uint8_t)('0' + (value >> 31)));
    value <<= 1;
  }
}

void dump_hex(unsigned int value, int bits, int unbuffered)
{
   value = value << (32-bits);
   for (int i = 0; i < (bits>>2); i++) {
      unsigned int c = value >> 28;
      if (c < 10) {
         c = '0' + c;
      } else {
         c = 'A' + c - 10;
      }
      if (unbuffered)
        RPI_UnbufferedWrite((uint8_t)c);
      else
        RPI_AuxMiniUartWrite((uint8_t)c);
      value <<= 4;
   }
}

void dump_string( const char * string, int padding, int unbuffered)
{
   int i;
   if (unbuffered)
      i = RPI_UnbufferedString( string, 0);
    else
      i = RPI_AuxMiniUartString( string, 0);

   while ( i<padding) {
      if (unbuffered)
        RPI_UnbufferedWrite((uint8_t)' ');
      else
        RPI_AuxMiniUartWrite(' ');
      i++;
   }
}

void padding(int padding, int unbuffered)
{
  int i=0;
   while ( i<padding) {
      if (unbuffered)
        RPI_UnbufferedWrite((uint8_t)' ');
      else
        RPI_AuxMiniUartWrite(' ');
      i++;
   }
}
