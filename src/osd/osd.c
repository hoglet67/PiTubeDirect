#include "osd.h"
#include "osd-tube.h"
#include "osd-vdu.h"
#include <stdbool.h>
#include <stdint.h>

extern void OSD_interpreter_main();
bool ran = false;

void osd_welcome()
{
    VDUClg();
    TubeWriteString("OS/D 0.1 [128K RAM] (daryl@dariclang.com)\r\n\r\n# ");
}

void osd_reset()
{
}

int osd_execute(int tube_cycles)
{
    if (!ran)
    {
        OSD_interpreter_main();
        ran = true;
    }
    return 1;
}
