#ifndef RPI_MAILBOX_H
#define RPI_MAILBOX_H

#include <stdint.h>

/* The available mailbox channels in the BCM2835 Mailbox interface.
   See https://github.com/raspberrypi/firmware/wiki/Mailboxes for
   information */
typedef enum {
    MB0_POWER_MANAGEMENT = 0,
    MB0_FRAMEBUFFER,
    MB0_VIRTUAL_UART,
    MB0_VCHIQ,
    MB0_LEDS,
    MB0_BUTTONS,
    MB0_TOUCHSCREEN,
    MB0_UNUSED,
    MB0_TAGS_ARM_TO_VC,
    MB0_TAGS_VC_TO_ARM,
} mailbox0_channel_t;

extern void RPI_Mailbox0Write( mailbox0_channel_t channel, uint32_t *ptr );
extern int RPI_Mailbox0Read( mailbox0_channel_t channel );

#endif
