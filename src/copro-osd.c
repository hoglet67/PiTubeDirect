/*
 * OS/D Operating Environment
 *
 * (c) 2022 Daryl Dudey
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "tube-defs.h"
#include "tube.h"
#include "tube-ula.h"
#include "osd/osd.h"

static void copro_osd_poweron_reset()
{
}

static void copro_osd_reset()
{
	// Log ARM performance counters
	tube_log_performance_counters();

	// Reset the processor
	osd_reset();

	// Wait for rst become inactive before continuing to execute
	tube_wait_for_rst_release();

	// Reset ARM performance counters
	tube_reset_performance_counters();
}

void copro_osd_emulator()
{
	// Remember the current copro so we can exit if it changes
	unsigned int last_copro = copro;

	copro_osd_poweron_reset();
	copro_osd_reset();

	bool prompt = false;
	while (1) {
		if (!prompt) {
			osd_welcome();
			prompt = true;
		}
		// Execute emulator for one instruction
		osd_execute(1);

		int tube_irq_copy = tube_irq & (RESET_BIT+NMI_BIT+IRQ_BIT);
		if (tube_irq_copy) {
			// Reset the processor on active edge of rst
			if (tube_irq_copy & RESET_BIT) {
				// Exit if the copro has changed
				if (copro!=last_copro) {
					break;
				}
				copro_osd_reset();
			}
			// IRQ is level sensitive so check between every instruction
			if (tube_irq_copy & IRQ_BIT) {
//            opc7_irq(0);
			}
		}
	}
}