#include <stdint.h>
#include "rpi-gpio.h"
#include "rpi-systimer.h"

void RPI_SetGpioPinFunction(rpi_gpio_pin_t gpio, rpi_gpio_alt_function_t func)
{
  rpi_reg_rw_t* fsel_reg = &RPI_GpioBase->GPFSEL[gpio / 10];

  rpi_reg_rw_t fsel_copy = *fsel_reg;
  fsel_copy &= (uint32_t)~(FS_MASK << ((gpio % 10) * 3));
  fsel_copy |= (func << ((gpio % 10) * 3));
  *fsel_reg = fsel_copy;
}

void RPI_SetGpioOutput(rpi_gpio_pin_t gpio)
{
  RPI_SetGpioPinFunction(gpio, FS_OUTPUT);
}

void RPI_SetGpioInput(rpi_gpio_pin_t gpio)
{
  RPI_SetGpioPinFunction(gpio, FS_INPUT);
}

rpi_gpio_value_t RPI_GetGpioValue(rpi_gpio_pin_t gpio)
{
  switch (gpio / 32)
  {
    case 0:
      if (RPI_GpioBase->GPLEV0 & (1 << gpio))
         return RPI_IO_HI;
      else
         return RPI_IO_LO;

    case 1:
      if (RPI_GpioBase->GPLEV1 & (1 << (gpio - 32)))
         return RPI_IO_HI;
      else
         return RPI_IO_LO;

    default:
      return RPI_IO_UNKNOWN;
  }
}

void RPI_ToggleGpio(rpi_gpio_pin_t gpio)
{
  if (RPI_GetGpioValue(gpio) == RPI_IO_HI)
    RPI_SetGpioLo(gpio);
  else
    RPI_SetGpioHi(gpio);
}

void RPI_SetGpioHi(rpi_gpio_pin_t gpio)
{
  switch (gpio / 32)
  {
    case 0:
      RPI_GpioBase->GPSET0 = (1 << gpio);
    break;

    case 1:
      RPI_GpioBase->GPSET1 = (1 << (gpio - 32));
    break;

    default:
    break;
  }
}

void RPI_SetGpioLo(rpi_gpio_pin_t gpio)
{
  switch (gpio / 32)
  {
    case 0:
      RPI_GpioBase->GPCLR0 = (1 << gpio);
    break;

    case 1:
      RPI_GpioBase->GPCLR1 = (1 << (gpio - 32));
    break;

    default:
    break;
  }
}

void RPI_SetGpioValue(rpi_gpio_pin_t gpio, rpi_gpio_value_t value)
{
  if ((value == RPI_IO_LO) || (value == RPI_IO_OFF))
    RPI_SetGpioLo(gpio);
  else if ((value == RPI_IO_HI) || (value == RPI_IO_ON))
    RPI_SetGpioHi(gpio);
}

void RPI_SetGpioLowLevelIRQ(rpi_gpio_pin_t gpio)
{

  rpi_reg_rw_t* fsel_reg = &RPI_GpioBase->GPLEN0;

  rpi_reg_rw_t fsel_copy = *fsel_reg;
  fsel_copy |= 1 << gpio;
  *fsel_reg = fsel_copy;
}

void RPI_SetGpioPull(rpi_gpio_pin_t gpio, rpi_gpio_pull pull)
{
#if defined(RPI4)
  rpi_reg_rw_t* pull_reg = &RPI_GpioBase->GPPULL[gpio / 16];

  rpi_reg_rw_t pull_copy = *pull_reg;
  pull_copy &= (uint32_t)~(0x3 << ((gpio % 16) * 2));
  pull_copy |= (pull << ((gpio % 16) * 2));
  *pull_reg = pull_copy;
#else
  RPI_GpioBase->GPPUD = pull;
  RPI_WaitMicroSeconds(2); // wait of 150 cycles needed see datasheet

  RPI_GpioBase->GPPUDCLK0 = 1<<gpio;
  RPI_WaitMicroSeconds(2); // wait of 150 cycles needed see datasheet

  RPI_GpioBase->GPPUDCLK0 = 0;
#endif
}
