#include <string.h>
#include "../framebuffer/framebuffer.h"
#include "osd-tube.h"

void TubeOSWRCH(unsigned char c)
{
	sendByte(1, c);
	fb_writec(c);
}

void TubeWriteString(const char* s)
{
	for (int i = 0; i<strlen(s); i++) {
		TubeOSWRCH(s[i]);
	}
}
