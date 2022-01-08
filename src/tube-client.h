// tube-client.h

#ifndef TUBE_CLIENT_H
#define TUBE_CLIENT_H


unsigned char * copro_mem_reset(unsigned int length);
void copro_memcpy( unsigned char * dst , const unsigned char * src , unsigned int length);
unsigned int get_copro_mhz(unsigned int copro_num);


#endif
