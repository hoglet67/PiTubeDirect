#include "cache.h"
#include "rpi-mailbox.h"
#include "rpi-base.h"

/* Define a structure which defines the register access to a mailbox.
   Not all mailboxes support the full register set! */
typedef struct {
    rpi_reg_rw_t Data;
    rpi_reg_ro_t reserved1[3]; // cppcheck-suppress unusedStructMember
    rpi_reg_ro_t Poll; // cppcheck-suppress unusedStructMember
    rpi_reg_ro_t Sender; // cppcheck-suppress unusedStructMember
    rpi_reg_ro_t Status;
    rpi_reg_rw_t Configuration; // cppcheck-suppress unusedStructMember
    } mailbox_t;

/* Mailbox 0 mapped to it's base address */
static mailbox_t* rpiMailbox0 = (mailbox_t*)( PERIPHERAL_BASE + 0xB880 );
static mailbox_t* rpiMailbox1 = (mailbox_t*)( PERIPHERAL_BASE + 0xB8A0 );

/* These defines come from the Broadcom Videocode driver source code, see:
   brcm_usrlib/dag/vmcsx/vcinclude/bcm2708_chip/arm_control.h */

#define ARM_MS_FULL   0x80000000
#define ARM_MS_EMPTY  0x40000000
#define ARM_MS_LEVEL  0x400000FF

void RPI_Mailbox0Write( mailbox0_channel_t channel, uint32_t *ptr )
{
    _clean_invalidate_dcache_area(ptr, ptr[0]);
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
    unsigned int value ;

    /* Keep reading the register until the desired channel gives us a value */

    do {
        /* Wait while the mailbox is empty because otherwise there's no value
           to read! */
        while( rpiMailbox0->Status & ARM_MS_EMPTY ) { }

        /* Extract the value from the Read register of the mailbox. The value
           is actually in the upper 28 bits */
        value = rpiMailbox0->Data;
    }  while( ( value & 0xF ) != channel ) ;

    /* Return just the value (the upper 28-bits) */
    return (int) (value >> 4);
}
