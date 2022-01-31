#include "../load_wasm.h"
#include <inttypes.h>
#include <stdlib.h>
#include "../../wasm_shared.h"

void section_custom(buffer_t* bf, uint32_t size, wasm_t* wat)
{
	const char* name = read_string(bf);
	if (trace>=LogDetail)
		print("Custom: %s\n", name);
	uint32_t num_fields = read_leb128_32(bf);
	for (uint32_t i = 0; i<num_fields; i++) {
		const char* name_field = read_string(bf);
		if (trace>=LogDetail)
			print("  %s: ", name_field);
		uint32_t field_value_count = read_leb128_32(bf);
		for (uint32_t  j = 0; j<field_value_count; j++) {
			const char* name_tool = read_string(bf);
			const char* version_tool = read_string(bf);
			if (trace>=LogDetail)
				print("%s-%s\n", name_tool, version_tool);
		}
	}
}
