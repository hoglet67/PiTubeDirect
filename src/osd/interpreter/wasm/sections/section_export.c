#include "../load_wasm.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "../../wasm_shared.h"

void section_export(buffer_t* bf, uint32_t size, wasm_t* wat)
{
	size_t p = bf->pos;
	wat->num_exports = read_leb128_32(bf);
	if (trace>=LogDetail)
		print("%d export section(s) at 0x%07lX\n", wat->num_exports, p);

	// Allocate space
	wat->section_exports = malloc(sizeof(section_export_t)*wat->num_exports);

	// Import each in turn
	wat->ctors_function = 0xFFFFFFFF;
	wat->start_function = 0xFFFFFFFF;
	for (uint32_t i = 0; i<wat->num_exports; i++) {
		section_export_t* p = &wat->section_exports[i];
		p->name = read_string(bf);
		p->tag = read_byte_int(bf);
		p->index = read_leb128_32(bf);
		if (p->tag==0 && strcmp(p->name, "_start")==0) {
			wat->start_function = p->index-wat->num_import_functions;
		}
		if (p->tag==0 && strcmp(p->name, "__wasm_call_ctors")==0) {
			wat->ctors_function = p->index-wat->num_import_functions;
		}
		if (trace>=LogDetail)
			print("  [%s] [%d] [%d]\n", p->name, p->tag, p->index);
	}
}
