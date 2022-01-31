#include "../load_wasm.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "../../wasm_shared.h"

#define IMPORT_TYPE_FUNC 0

void section_import(buffer_t* bf, uint32_t size, wasm_t* wat)
{
	size_t p = bf->pos;
	wat->num_imports = read_leb128_32(bf);
	if (trace>=LogDetail)
		print("%d import section(s) at 0x%07lX\n", wat->num_imports, p);

	// Allocate space
	wat->section_imports = malloc(sizeof(section_import_t)*wat->num_imports);

	// Import each in turn
	wat->num_import_functions = 0;
	for (uint32_t i = 0; i<wat->num_imports; i++) {
		section_import_t* p = &wat->section_imports[i];
		p->module = read_string(bf);
		p->name = read_string(bf);
		p->tag = read_byte_int(bf);
		switch (p->tag) {
			case IMPORT_TYPE_FUNC:
				p->index = read_leb128_32(bf);
				wat->num_import_functions++;
				if (strcmp(p->name, "call_OSD_API")==0) {
					p->type = API_CALL;
				}
				break;
			default:
				print("Unknown tag type in import: %d", p->tag);
				exit(1);
		}
	}
}
