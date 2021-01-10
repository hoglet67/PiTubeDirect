#ifndef _VDU_STATE_H
#define _VDU_STATE_H

typedef enum {
   NORMAL,
   IN_VDU4,
   IN_VDU5,
   IN_VDU17,
   IN_VDU18,
   IN_VDU19,
   IN_VDU22,
   IN_VDU23,
   IN_VDU25,
   IN_VDU29,
   IN_VDU31
} vdu_state_t;

#endif
