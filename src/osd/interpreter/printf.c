#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void print(const char *format, ...)
{
    char buffer[512];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 512, format, args);
    va_end(args);
    TubeWriteString(buffer);
    //    printf("%s", buffer);
}
