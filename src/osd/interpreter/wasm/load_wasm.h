#ifndef LOAD_WASM_H
#define LOAD_WASM_H
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "../containers/vector.h"
#include "buffer.h"
#include "wasm.h"

instruction_t * load_expression(buffer_t* bf);

#define TYPE_NONE 0x0
#define TYPE_I32 0x7F
#define TYPE_I64 0x7E
#define TYPE_F32 0x7D
#define TYPE_F64 0x7C

const char* read_string(buffer_t* fp);
uint8_t read_byte_int(buffer_t* fp);
void write_uint(buffer_t* fp, uint32_t v);
uint32_t read_uint(buffer_t* fp);
void write_uint_simple(uint8_t* buffer, uint32_t v);
void write_int_simple(uint8_t* buffer, int32_t v);
uint32_t read_uint_simple(uint8_t* buffer);
int32_t read_int_simple(uint8_t* buffer);
uint32_t read_leb128_32(buffer_t* fp);
int32_t read_sleb128_32(buffer_t* fp);
uint8_t* read_data(buffer_t* fp, uint32_t sz);
void validate_type(uint32_t type, wasm_t* wat);

#endif