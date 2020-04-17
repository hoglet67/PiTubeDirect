
#include <stdio.h>
#include <stdint.h>
#include "rpi-armtimer.h"
#include "startup.h"

static rpi_arm_timer_t* rpiArmTimer = (rpi_arm_timer_t*)RPI_ARMTIMER_BASE;

rpi_arm_timer_t* RPI_GetArmTimer(void)
{
    return rpiArmTimer;
}

void RPI_ArmTimerInit(void)
{
   _data_memory_barrier();

   // Timer defaults to running at ~1MHz

   rpiArmTimer->Load = 1000;
   rpiArmTimer->Control = RPI_ARMTIMER_CTRL_PRESCALE_1 | RPI_ARMTIMER_CTRL_INT_ENABLE | RPI_ARMTIMER_CTRL_ENABLE;

   _data_memory_barrier();

}
