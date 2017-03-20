// copro-65tube.h
#ifndef COPRO_65TUBE_H
#define COPRO_65TUBE_H

extern void copro_65tube_emulator();

extern void exec_65tube(unsigned char *memory);

//extern unsigned char mpu_memory[];

#ifndef USE_MULTICORE
extern uint32_t Event_Handler_Dispatch_Table[];

extern void Event_Handler();

extern void Event_Handler_Single_Core_Slow();
#endif

#endif
