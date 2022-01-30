#include <stdio.h>
#include <stdlib.h>
#include "wasm/load_wasm.h"
#include "wasm/sections/sections.h"

void section_type(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_function(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_memory(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_global(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_import(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_export(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_code(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_custom(buffer_t* bf, uint32_t size, wasm_t* wat);
void section_data(buffer_t* bf, uint32_t size, wasm_t* wat);

void import_wat(wasm_t* wat, buffer_t* bf)
{
	// Now to process each section
	while (bf->pos<bf->len) {
		uint8_t section = read_byte_int(bf);
		uint32_t sz = read_leb128_32(bf);
		switch (section) {
			case TYPE_SECTION:
				section_type(bf, sz, wat);
				break;
			case FUNCTION_SECTION:
				section_function(bf, sz, wat);
				break;
			case MEMORY_SECTION:
				section_memory(bf, sz, wat);
				break;
			case GLOBAL_SECTION:
				section_global(bf, sz, wat);
				break;
			case IMPORT_SECTION:
				section_import(bf, sz, wat);
				break;
			case EXPORT_SECTION:
				section_export(bf, sz, wat);
				break;
			case CODE_SECTION:
				section_code(bf, sz, wat);
				break;
			case CUSTOM_SECTION:
				section_custom(bf, sz, wat);
				break;
			case DATA_SECTION:
				section_data(bf, sz, wat);
				break;
			default:
				printf("Unhandled section: %d %u\n", section, sz);
				exit(1);
		}
	}
}
