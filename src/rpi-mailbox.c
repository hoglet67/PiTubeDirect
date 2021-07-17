#include <stdint.h>

#include "cache.h"
#include "rpi-gpio.h"
#include "rpi-mailbox.h"

/* Mailbox 0 mapped to it's base address */
static mailbox_t* rpiMailbox0 = (mailbox_t*)RPI_MAILBOX0_BASE;
static mailbox_t* rpiMailbox1 = (mailbox_t*)RPI_MAILBOX1_BASE;

void RPI_Mailbox0Write( mailbox0_channel_t channel, uint32_t *ptr )
{
    _clean_cache_area(ptr, ptr[0]);
    /* For information about accessing mailboxes, see:
       https://github.com/raspberrypi/firmware/wiki/Accessing-mailboxes */

    // Flush any old requests
    while (!(rpiMailbox0->Status & ARM_MS_EMPTY)) {
       (void) rpiMailbox0->Data;
    }

    /* Wait until the mailbox becomes available and then write to the mailbox
       channel */
    while( ( rpiMailbox1->Status & ARM_MS_FULL ) != 0 ) { }

    /* Write the modified value + channel number into the write register */
    rpiMailbox1->Data = ((uint32_t)ptr ) | channel;
}


int RPI_Mailbox0Read( mailbox0_channel_t channel )
{
    /* For information about accessing mailboxes, see:
       https://github.com/raspberrypi/firmware/wiki/Accessing-mailboxes */
    int value = -1;

    /* Keep reading the register until the desired channel gives us a value */
    while( ( value & 0xF ) != channel )
    {
        /* Wait while the mailbox is empty because otherwise there's no value
           to read! */
        while( rpiMailbox0->Status & ARM_MS_EMPTY ) { }

        /* Extract the value from the Read register of the mailbox. The value
           is actually in the upper 28 bits */
        value = (int )rpiMailbox0->Data;
    }

    /* Return just the value (the upper 28-bits) */
    return value >> 4;
}
