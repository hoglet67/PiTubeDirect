// armbasic.h

#ifndef ARMBASIC_H
#define ARMBASIC_H

// BeebMaster's BAS135H
#define ARM_BASIC_START (0x0D800000)
#define ARM_BASIC_EXEC  (ARM_BASIC_START + 0x10C)

// Flag to indicate if ARM Basic was active before reset
extern int armbasic;

// Install ARM Basic into Memory
extern void copy_armbasic();

#endif
