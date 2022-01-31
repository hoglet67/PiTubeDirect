#include "osd.h"
#include "osd-tube.h"
#include "osd-vdu.h"
#include <stdbool.h>
#include <stdint.h>

bool ran = false;
void WASM_init();
void WASM_loadmemory();
void WASM_verify();
void WASM_import();
void WASM_build();
void WASM_run();

void osd_welcome()
{
    VDUClg();
    TubeWriteString("OS/D (daryl@dariclang.com)\n\n# ");
}

void osd_reset()
{
}

int osd_execute(int tube_cycles)
{
    if (!ran)
    {
        WASM_init();
        WASM_loadmemory();
        WASM_verify();
        WASM_import();
        WASM_build();
        WASM_run();
        ran = true;
    }
    return 1;
}
