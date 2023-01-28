#include "rpi-systimer.h"

#include "rpi-base.h"

#define RPI_SYSTIMER_BASE       ( PERIPHERAL_BASE + 0x3000 )
typedef struct {
    volatile uint32_t control_status; // cppcheck-suppress unusedStructMember
    volatile uint32_t counter_lo;
    volatile uint32_t counter_hi; // cppcheck-suppress unusedStructMember
    volatile uint32_t compare0; // cppcheck-suppress unusedStructMember
    volatile uint32_t compare1; // cppcheck-suppress unusedStructMember
    volatile uint32_t compare2; // cppcheck-suppress unusedStructMember
    volatile uint32_t compare3; // cppcheck-suppress unusedStructMember
    } rpi_sys_timer_t;


static rpi_sys_timer_t* rpiSystemTimer = (rpi_sys_timer_t*)RPI_SYSTIMER_BASE;

void RPI_WaitMicroSeconds( uint32_t us )
{
    volatile uint32_t ts = rpiSystemTimer->counter_lo;

    while( ( rpiSystemTimer->counter_lo - ts ) < us )
    {
        /* BLANK */
    }
}
