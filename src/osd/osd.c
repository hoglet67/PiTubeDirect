#include <stdint.h>
#include "osd.h"
#include "osd-tube.h"
#include "osd-vdu.h"

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
	return 1;
}
