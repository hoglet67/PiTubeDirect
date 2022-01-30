#ifndef BUFFER_H
#define BUFFER_H

typedef struct
{
	uint8_t *buffer;
	size_t pos;
	size_t len;
} buffer_t;

#endif
