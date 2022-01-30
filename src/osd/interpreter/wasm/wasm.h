#ifndef WASM_H
#define WASM_H
#include <stdbool.h>
#include "instructions.h"

#define API_CALL 0

typedef struct {
	uint32_t num_params;
	uint32_t* params;
	uint32_t num_returns;
	uint32_t* result;
} section_type_t;

typedef struct {
	bool has_max;
	uint32_t min;
	uint32_t max;
} section_memory_t;

typedef struct {
	uint32_t type;
	uint8_t mut;
	instruction_t* init;
	union {
		int32_t value_i32;
		int64_t value_i64;
		float value_f32;
		double value_f64;
	};
} section_global_t;

typedef struct {
	uint32_t type;
	union {
		int32_t value_i32;
		int64_t value_i64;
		float value_f32;
		double value_f64;
	};
} local_t;

typedef struct {
	const char* name;
	uint8_t tag;
	uint32_t index;
} section_export_t;

typedef struct {
	const char* module;
	const char* name;
	uint8_t tag;
	uint32_t index;
	uint32_t type;
} section_import_t;

typedef struct {
	uint32_t count;
	uint32_t type;
} section_code_locals_t;

typedef struct {
	uint32_t num_locals;
	uint32_t num_params;
	uint32_t* params;
	uint32_t num_locals_total;
	section_code_locals_t* locals;
	instruction_t* code;
} section_code_t;

typedef struct {
	uint32_t memory_index;
	uint32_t offset_size;
	instruction_t* offset;
	uint32_t data_size;
	uint8_t* data;
} section_data_t;

typedef struct {
	uint32_t num_types;
	uint32_t num_import_functions;
	uint32_t num_functions;
	uint32_t num_memories;
	uint32_t num_globals;
	uint32_t num_exports;
	uint32_t num_imports;
	uint32_t num_codes;
	uint32_t num_datas;
	section_type_t* section_types;
	uint32_t* section_functions;
	uint32_t* section_function_addresses;
	section_memory_t* section_memories;
	section_global_t* section_globals;
	section_export_t* section_exports;
	section_import_t* section_imports;
	section_code_t* section_codes;
	section_data_t* section_datas;
	uint32_t ctors_function;
	uint32_t start_function;
} wasm_t;

#endif
