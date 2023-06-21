
#include <stdio.h>
#include <stdint.h>
#include "rpi-base.h"
#include "rpi-armtimer.h"
#include "rpi-asm-helpers.h"

static rpi_arm_timer_t* rpiArmTimer = (rpi_arm_timer_t*)RPI_ARMTIMER_BASE;

rpi_arm_timer_t* RPI_GetArmTimer(void)
{
    return rpiArmTimer;
}

void RPI_ArmTimerInit(void)
{
   _data_memory_barrier();

   // The Pi 0 and 3A PreDivider defaults to 399, which gives a
   // timer base frequency of 400 / (399 + 1) = 1MHz
   //
   // The Pi 4B PreDivider defaults to 10, which gives a
   // timer base frequency of 500 / (10 + 1) = 45.45MHz.
   //
   // So we need to change the value on the Pi 4B:
#ifdef RPI4
   rpiArmTimer->PreDivider = 499;
#endif

   // Timer defaults to running at ~1MHz

   rpiArmTimer->Load = 1000;
   rpiArmTimer->Control = RPI_ARMTIMER_CTRL_PRESCALE_1 | RPI_ARMTIMER_CTRL_INT_ENABLE | RPI_ARMTIMER_CTRL_ENABLE;

   _data_memory_barrier();
}
