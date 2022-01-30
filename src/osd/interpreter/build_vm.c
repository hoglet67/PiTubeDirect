#include "wasm/load_wasm.h"
#include <stdio.h>
#include <memory.h>
#include <string.h>

uint32_t start_of_heap(wasm_t* wat) {
	for (int i = 0; i<wat->num_exports; i++) {
		section_export_t* t = &wat->section_exports[i];
		if (strcmp(t->name, "__heap_base")==0) {
			section_global_t* g = &wat->section_globals[t->index];
			return g->init[0].value_i32;
		}
	}
	return 0;
}

void build_vm(wasm_t* wat, uint8_t* RAM, size_t RAM_size) {
	// Copy static data
	for (int i = 0; i<wat->num_datas; i++) {
		section_data_t* t = &wat->section_datas[i];
		uint32_t offset = t->offset[0].value_i32;
		memcpy(&RAM[offset], t->data, t->data_size);
	}

	// Compile all init sections for globals first
	for (int i = 0; i<wat->num_globals; i++) {
		section_global_t* t = &wat->section_globals[i];
		instruction_t *instruction = &t->init[0];
		switch (t->type) {
		case TYPE_I32:
			t->value_i32 = instruction->value_i32;
			break;
		case TYPE_I64:
			t->value_i64 = instruction->value_i64;
			break;
		case TYPE_F32:
			t->value_f32 = instruction->value_f32;
			break;
		case TYPE_F64:
			t->value_f64 = instruction->value_f64;
			break;
		default:
			fprintf(stderr, "Unknown type in global init\n");
			exit(1);
		}
	}
}