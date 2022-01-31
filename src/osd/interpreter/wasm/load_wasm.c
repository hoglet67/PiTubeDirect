#include "load_wasm.h"
#include <inttypes.h>
#include <stdlib.h>
#include "leb128.h"
#include <string.h>
#include <memory.h>
#include "../wasm_shared.h"

const char* read_string(buffer_t* fp)
{
	uint32_t sz = read_byte_int(fp);
	char* text = malloc(sz+1);
	strncpy(text, (char*)&fp->buffer[fp->pos], sz);
	text[sz] = 0;
	fp->pos += sz;
	return text;
}

uint8_t* read_data(buffer_t* fp, uint32_t sz)
{
	uint8_t* data = &fp->buffer[fp->pos];
	fp->pos += sz;
	return data;
/*	uint8_t* data = malloc(sz);
	memcpy(data, &fp->buffer[fp->pos], sz);
	fp->pos += sz;
	return data;*/
}

uint8_t read_byte_int(buffer_t* fp)
{
	return fp->buffer[fp->pos++];
}

uint32_t read_uint(buffer_t* fp)
{
	uint32_t i = (fp->buffer[fp->pos+3] << 24) | (fp->buffer[fp->pos+2] << 16) | (fp->buffer[fp->pos+1] << 8)
			| fp->buffer[fp->pos];
	fp->pos += 4;
	return i;
}

void write_uint(buffer_t* fp, uint32_t v)
{
	fp->buffer[fp->pos++] = (v >> 24) & 0xFF;
	fp->buffer[fp->pos++] = (v >> 16) & 0xFF;
	fp->buffer[fp->pos++] = (v >> 8) & 0xFF;
	fp->buffer[fp->pos++] = (v & 0xFF);
}

uint32_t read_uint_simple(uint8_t* buffer)
{
	uint32_t* b = (uint32_t*)buffer;
	uint32_t i = *b;
	if (trace>=LogTrace)
		print("RU = %d %d %d %d = %d\n", buffer[0], buffer[1], buffer[2], buffer[3], i);
	return i;
}

int32_t read_int_simple(uint8_t* buffer)
{
	int32_t* b = (int32_t*)buffer;
	int32_t i = *b;
	if (trace>=LogTrace)
		print("RS = %d %d %d %d = %d\n", buffer[0], buffer[1], buffer[2], buffer[3], i);
	return i;
}

void write_uint_simple(uint8_t* buffer, uint32_t v)
{
	uint32_t* b = (uint32_t*)buffer;
	*b = v;
	if (trace>=LogTrace)
		print("WU = %d %d %d %d = %d\n", buffer[0], buffer[1], buffer[2], buffer[3], v);
}

void write_int_simple(uint8_t* buffer, int32_t v)
{
	int32_t* b = (int32_t*)buffer;
	*b = v;
	if (trace>=LogTrace)
		print("WS = %d %d %d %d = %d\n", buffer[0], buffer[1], buffer[2], buffer[3], v);
}

uint32_t read_leb128_32(buffer_t* fp)
{
	uint64_t r;
	size_t rd = read_uleb128_to_uint64(&fp->buffer[fp->pos], &fp->buffer[fp->len-1], &r);
	if (rd==0) {
		print("Total fail reading leb128\n");
		exit(1);
	}
	fp->pos += rd;
	return r;
}

int32_t read_sleb128_32(buffer_t* fp)
{
	int64_t r;
	size_t rd = read_sleb128_to_int64(&fp->buffer[fp->pos], &fp->buffer[fp->len-1], &r);
	if (rd==0) {
		fprintf(stderr, "Total fail reading leb128\n");
		exit(1);
	}
	fp->pos += rd;
	return r;
}

void validate_type(uint32_t type, wasm_t* wat)
{
	switch (type) {
		case TYPE_NONE:
		case TYPE_I32:
		case TYPE_I64:
		case TYPE_F32:
		case TYPE_F64:
			return;
		default: {
			if (type>wat->num_types) {
				print("Type is invalid\n");
				exit(1);
			}
			break;
			//section_type_t* t = &wat->section_types[type];
		}
	}
}
