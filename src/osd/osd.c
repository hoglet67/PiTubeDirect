#include <stdio.h>
#include "osd.h"
#include "../tube.h"
#include "../tube-lib.h"

void osd_reset() {
}

int osd_execute(int tube_cycles) {
    sendByte(1, 'a');
    return 1;
}
