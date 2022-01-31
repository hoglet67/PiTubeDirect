#include "osd-tube.h"
#include "../framebuffer/framebuffer.h"
#include <string.h>

void TubeOSWRCH(unsigned char c)
{
    sendByte(1, c);
    fb_writec(c);
}

void TubeWriteString(const char *s)
{
    for (int i = 0; i < strlen(s); i++)
    {
        if (s[i] == '\n')
        {
            TubeOSWRCH('\n');
            TubeOSWRCH('\r');
        }
        else
        {
            TubeOSWRCH(s[i]);
        }
    }
}
