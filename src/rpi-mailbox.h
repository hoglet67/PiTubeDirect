#ifndef RPI_MAILBOX_H
#define RPI_MAILBOX_H

#include "rpi-base.h"

#define RPI_MAILBOX0_BASE    ( PERIPHERAL_BASE + 0xB880 )
#define RPI_MAILBOX1_BASE    ( PERIPHERAL_BASE + 0xB8A0 )

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

/* These defines come from the Broadcom Videocode driver source code, see:
   brcm_usrlib/dag/vmcsx/vcinclude/bcm2708_chip/arm_control.h */
enum mailbox_status_reg_bits {
    ARM_MS_FULL  = 0x80000000,
    ARM_MS_EMPTY = 0x40000000,
    ARM_MS_LEVEL = 0x400000FF,
};

/* Define a structure which defines the register access to a mailbox.
   Not all mailboxes support the full register set! */
typedef struct {
    rpi_reg_rw_t Data;
    rpi_reg_ro_t reserved1[3];
    rpi_reg_ro_t Poll;
    rpi_reg_ro_t Sender;
    rpi_reg_ro_t Status;
    rpi_reg_rw_t Configuration;
    } mailbox_t;

extern void RPI_Mailbox0Write( mailbox0_channel_t channel, uint32_t *ptr );
extern int RPI_Mailbox0Read( mailbox0_channel_t channel );

#endif
