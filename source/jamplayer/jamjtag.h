/****************************************************************************/
/*																			*/
/*	Module:			jamjtag.h												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Definitions of JTAG constants, types, and functions		*/
/*																			*/
/****************************************************************************/

#ifndef INC_JAMJTAG_H
#define INC_JAMJTAG_H

/****************************************************************************/
/*																			*/
/*	Constant definitions													*/
/*																			*/
/****************************************************************************/

#define JAMC_MAX_JTAG_STATE_LENGTH 9

/****************************************************************************/
/*																			*/
/*	Enumerated Types														*/
/*																			*/
/****************************************************************************/

typedef enum
{
	JAM_ILLEGAL_JTAG_STATE = -1,
	RESET = 0,
	IDLE = 1,
	DRSELECT = 2,
	DRCAPTURE = 3,
	DRSHIFT = 4,
	DREXIT1 = 5,
	DRPAUSE = 6,
	DREXIT2 = 7,
	DRUPDATE = 8,
	IRSELECT = 9,
	IRCAPTURE = 10,
	IRSHIFT = 11,
	IREXIT1 = 12,
	IRPAUSE = 13,
	IREXIT2 = 14,
	IRUPDATE = 15

} JAME_JTAG_STATE;

/****************************************************************************/
/*																			*/
/*	Function Prototypes														*/
/*																			*/
/****************************************************************************/

JAME_JTAG_STATE jam_get_jtag_state_from_name
(
	char *name
);

JAM_RETURN_TYPE jam_set_drstop_state
(
	JAME_JTAG_STATE state
);

JAM_RETURN_TYPE jam_set_irstop_state
(
	JAME_JTAG_STATE state
);

JAM_RETURN_TYPE jam_set_jtag_padding
(
	int dr_preamble,
	int dr_postamble,
	int ir_preamble,
	int ir_postamble
);

JAM_RETURN_TYPE jam_goto_jtag_state
(
	JAME_JTAG_STATE state
);

JAM_RETURN_TYPE jam_do_wait_cycles
(
	long cycles,
	JAME_JTAG_STATE wait_state
);

JAM_RETURN_TYPE jam_do_wait_microseconds
(
	long microseconds,
	JAME_JTAG_STATE wait_state
);

JAM_RETURN_TYPE jam_do_irscan
(
	long count,
	long *data,
	long start_index
);

JAM_RETURN_TYPE jam_do_drscan
(
	long count,
	long *data,
	long start_index
);

JAM_RETURN_TYPE jam_swap_dr
(
	long count,
	long *in_data,
	long in_index,
	long *out_data,
	long out_index
);

#endif /* INC_JAMJTAG_H */
