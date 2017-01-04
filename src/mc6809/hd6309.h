/*  XRoar - a Dragon/Tandy Coco emulator
 *  Copyright (C) 2003-2016  Ciaran Anscomb
 *
 *  See COPYING.GPL for redistribution conditions. */

#ifndef XROAR_HD6309_H_
#define XROAR_HD6309_H_

#include <stdint.h>

#include "mc6809.h"

#define MC6809_VARIANT_HD6309 (0x00006309)

#define HD6309_INT_VEC_ILLEGAL (0xfff0)

/* MPU state.  Represents current position in the high-level flow chart from
 * the data sheet (figure 14). */
enum hd6309_state {
	hd6309_state_label_a,
	hd6309_state_sync,
	hd6309_state_dispatch_irq,
	hd6309_state_label_b,
	hd6309_state_reset,
	hd6309_state_reset_check_halt,
	hd6309_state_next_instruction,
	hd6309_state_instruction_page_2,
	hd6309_state_instruction_page_3,
	hd6309_state_cwai_check_halt,
	hd6309_state_sync_check_halt,
	hd6309_state_done_instruction,
	hd6309_state_tfm,
	hd6309_state_tfm_write
};

struct HD6309 {
	struct MC6809 mc6809;
	// Separate state variable for the sake of debugging
	enum hd6309_state state;
	// Extra registers
	uint16_t reg_w;
	uint8_t reg_md;
	uint16_t reg_v;
	// TFM state
	uint16_t *tfm_src;
	uint16_t *tfm_dest;
	uint8_t tfm_data;
	uint16_t tfm_src_mod;
	uint16_t tfm_dest_mod;
};

#define HD6309_REG_E(hcpu) (*((uint8_t *)&hcpu->reg_w + MC6809_REG_HI))
#define HD6309_REG_F(hcpu) (*((uint8_t *)&hcpu->reg_w + MC6809_REG_LO))

/* new still returns a struct MC6809 */
struct MC6809 *hd6309_new(void);

#endif  /* XROAR_HD6309_H_ */
