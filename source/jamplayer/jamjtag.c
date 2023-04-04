/****************************************************************************/
/*																			*/
/*	Module:			jamjtag.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Contains array management functions, including			*/
/*					functions for reading array initialization data in		*/
/*					compressed formats.										*/
/*																			*/
/****************************************************************************/

#include "jamexprt.h"
#include "jamdefs.h"
#include "jamsym.h"
#include "jamutil.h"
#include "jamjtag.h"

/*
*	Global variable to store the current JTAG state
*/
JAME_JTAG_STATE jam_jtag_state = JAM_ILLEGAL_JTAG_STATE;

/*
*	Store current stop-state for DR and IR scan commands
*/
JAME_JTAG_STATE jam_drstop_state = IDLE;
JAME_JTAG_STATE jam_irstop_state = IDLE;

/*
*	Store current padding values
*/
int jam_dr_preamble  = 0;
int jam_dr_postamble = 0;
int jam_ir_preamble  = 0;
int jam_ir_postamble = 0;

/*
*	Table of JTAG state names
*/
struct JAMS_JTAG_MAP
{
	JAME_JTAG_STATE state;
	char string[JAMC_MAX_JTAG_STATE_LENGTH + 1];
} jam_jtag_state_table[] =
{
	{ RESET,     "RESET"     },
	{ IDLE,      "IDLE"      },
	{ DRSELECT,  "DRSELECT"  },
	{ DRCAPTURE, "DRCAPTURE" },
	{ DRSHIFT,   "DRSHIFT"   },
	{ DREXIT1,   "DREXIT1"   },
	{ DRPAUSE,   "DRPAUSE"   },
	{ DREXIT2,   "DREXIT2"   },
	{ DRUPDATE,  "DRUPDATE"  },
	{ IRSELECT,  "IRSELECT"  },
	{ IRCAPTURE, "IRCAPTURE" },
	{ IRSHIFT,   "IRSHIFT"   },
	{ IREXIT1,   "IREXIT1"   },
	{ IRPAUSE,   "IRPAUSE"   },
	{ IREXIT2,   "IREXIT2"   },
	{ IRUPDATE,  "IRUPDATE"  }
};

#define JAMC_JTAG_STATE_COUNT \
  (sizeof(jam_jtag_state_table) / sizeof(jam_jtag_state_table[0]))

/*
*	This structure shows, for each JTAG state, which state is reached after
*	a single TCK clock cycle with TMS high or TMS low, respectively.  This
*	describes all possible state transitions in the JTAG state machine.
*/
struct JAMS_JTAG_MACHINE
{
	JAME_JTAG_STATE tms_high;
	JAME_JTAG_STATE tms_low;
} jam_jtag_state_transitions[] =
{
/* RESET     */	{ RESET,	IDLE },
/* IDLE      */	{ DRSELECT,	IDLE },
/* DRSELECT  */	{ IRSELECT,	DRCAPTURE },
/* DRCAPTURE */	{ DREXIT1,	DRSHIFT },
/* DRSHIFT   */	{ DREXIT1,	DRSHIFT },
/* DREXIT1   */	{ DRUPDATE,	DRPAUSE },
/* DRPAUSE   */	{ DREXIT2,	DRPAUSE },
/* DREXIT2   */	{ DRUPDATE,	DRSHIFT },
/* DRUPDATE  */	{ DRSELECT,	IDLE },
/* IRSELECT  */	{ RESET,	IRCAPTURE },
/* IRCAPTURE */	{ IREXIT1,	IRSHIFT },
/* IRSHIFT   */	{ IREXIT1,	IRSHIFT },
/* IREXIT1   */	{ IRUPDATE,	IRPAUSE },
/* IRPAUSE   */	{ IREXIT2,	IRPAUSE },
/* IREXIT2   */	{ IRUPDATE,	IRSHIFT },
/* IRUPDATE  */	{ DRSELECT,	IDLE }
};

/*
*	This table contains the TMS value to be used to take the NEXT STEP on
*	the path to the desired state.  The array index is the current state,
*	and the bit position is the desired endstate.  To find out which state
*	is used as the intermediate state, look up the TMS value in the
*	jam_jtag_state_transitions[] table.
*/
unsigned short jam_jtag_path_map[16] =
{
	0x0001, 0xFFFD, 0xFE01, 0xFFE7, 0xFFEF, 0xFF0F, 0xFFBF, 0xFF0F,
	0xFEFD, 0x0001, 0xF3FF, 0xF7FF, 0x87FF, 0xDFFF, 0x87FF, 0x7FFD
};

/*
*	Flag bits for jam_jtag_io() function
*/
#define TMS_HIGH 0x01
#define TMS_LOW  0x00
#define TDI_HIGH 0x02
#define TDI_LOW  0x00
#define READ_TDO 0x04

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_init_jtag()

/*																			*/
/****************************************************************************/
{
	/* initial JTAG state is unknown */
	jam_jtag_state = JAM_ILLEGAL_JTAG_STATE;

	/* initialize global variables to default state */
	jam_drstop_state = IDLE;
	jam_irstop_state = IDLE;
	jam_dr_preamble  = 0;
	jam_dr_postamble = 0;
	jam_ir_preamble  = 0;
	jam_ir_postamble = 0;

	return (JAMC_SUCCESS);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_set_drstop_state
(
	JAME_JTAG_STATE state
)

/*																			*/
/****************************************************************************/
{
	jam_drstop_state = state;

	return (JAMC_SUCCESS);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_set_irstop_state
(
	JAME_JTAG_STATE state
)

/*																			*/
/****************************************************************************/
{
	jam_irstop_state = state;

	return (JAMC_SUCCESS);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_set_jtag_padding
(
	int dr_preamble,
	int dr_postamble,
	int ir_preamble,
	int ir_postamble
)

/*																			*/
/****************************************************************************/
{
	jam_dr_preamble  = dr_preamble;
	jam_dr_postamble = dr_postamble;
	jam_ir_preamble  = ir_preamble;
	jam_ir_postamble = ir_postamble;

	return (JAMC_SUCCESS);
}

/****************************************************************************/
/*																			*/

void jam_jtag_reset_idle()

/*																			*/
/****************************************************************************/
{
	int i = 0;

	/*
	*	Go to Test Logic Reset (no matter what the starting state may be)
	*/
	for (i = 0; i < 5; ++i)
	{
		jam_jtag_io(TMS_HIGH | TDI_LOW);
	}

	/*
	*	Now step to Run Test / Idle
	*/
	jam_jtag_io(TMS_LOW | TDI_LOW);

	jam_jtag_state = IDLE;
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_goto_jtag_state
(
	JAME_JTAG_STATE state
)

/*																			*/
/****************************************************************************/
{
	int tms = 0;
	int count = 0;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	if (jam_jtag_state == JAM_ILLEGAL_JTAG_STATE)
	{
		/* initialize JTAG chain to known state */
		jam_jtag_reset_idle();
	}

	if (jam_jtag_state == state)
	{
		/*
		*	We are already in the desired state.  If it is a stable state,
		*	loop here.  Otherwise do nothing (no clock cycles).
		*/
		if ((state == IDLE) ||
			(state == DRSHIFT) ||
			(state == DRPAUSE) ||
			(state == IRSHIFT) ||
			(state == IRPAUSE))
		{
			jam_jtag_io(TMS_LOW | TDI_LOW);
		}
		else if (state == RESET)
		{
			jam_jtag_io(TMS_HIGH | TDI_LOW);
		}
	}
	else
	{
		while ((jam_jtag_state != state) && (count < 9))
		{
			/*
			*	Get TMS value to take a step toward desired state
			*/
			tms = (jam_jtag_path_map[jam_jtag_state] & (1 << state)) ?
				TMS_HIGH : TMS_LOW;

			/*
			*	Take a step
			*/
			jam_jtag_io(tms | TDI_LOW);

			if (tms)
			{
				jam_jtag_state =
					jam_jtag_state_transitions[jam_jtag_state].tms_high;
			}
			else
			{
				jam_jtag_state =
					jam_jtag_state_transitions[jam_jtag_state].tms_low;
			}

			++count;
		}
	}

	if (jam_jtag_state != state)
	{
		status = JAMC_INTERNAL_ERROR;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAME_JTAG_STATE jam_get_jtag_state_from_name
(
	char *name
)

/*																			*/
/*	Description:	Finds JTAG state code corresponding to name of state	*/
/*					supplied as a string									*/
/*																			*/
/*	Returns:		JTAG state code, or JAM_ILLEGAL_JTAG_STATE if string	*/
/*					does not match any valid state name						*/
/*																			*/
/****************************************************************************/
{
	int i = 0;
	JAME_JTAG_STATE jtag_state = JAM_ILLEGAL_JTAG_STATE;

	for (i = 0; (jtag_state == JAM_ILLEGAL_JTAG_STATE) &&
		(i < (int) JAMC_JTAG_STATE_COUNT); ++i)
	{
		if (jam_strcmp(name, jam_jtag_state_table[i].string) == 0)
		{
			jtag_state = jam_jtag_state_table[i].state;
		}
	}

	return (jtag_state);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_do_wait_cycles
(
	long cycles,
	JAME_JTAG_STATE wait_state
)

/*																			*/
/*	Description:	Causes JTAG hardware to loop in the specified stable	*/
/*					state for the specified number of TCK clock cycles.		*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int tms = 0;
	long count = 0L;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	if (jam_jtag_state != wait_state)
	{
		status = jam_goto_jtag_state(wait_state);
	}

	if (status == JAMC_SUCCESS)
	{
		/*
		*	Set TMS high to loop in RESET state
		*	Set TMS low to loop in any other stable state
		*/
		tms = (wait_state == RESET) ? TMS_HIGH : TMS_LOW;

		for (count = 0L; count < cycles; count++)
		{
			jam_jtag_io(tms | TDI_LOW);
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_do_wait_microseconds
(
	long microseconds,
	JAME_JTAG_STATE wait_state
)

/*																			*/
/*	Description:	Causes JTAG hardware to sit in the specified stable		*/
/*					state for the specified duration of real time.			*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	if (jam_jtag_state != wait_state)
	{
		status = jam_goto_jtag_state(wait_state);
	}

	if (status == JAMC_SUCCESS)
	{
		/*
		*	Wait for specified time interval
		*/
		jam_delay(microseconds);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_do_irscan
(
	long count,
	long *data,
	long start_index
)

/*																			*/
/*	Description:	Shifts data into instruction register					*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int tdi = 0;
	long i = 0L;
	long index = start_index;
	long shift_count = count + jam_ir_preamble + jam_ir_postamble - 1;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	status = jam_goto_jtag_state(IRSHIFT);

	if (status == JAMC_SUCCESS)
	{
		/* loop in the SHIFT-IR state */
		for (i = 0; i < shift_count; i++)
		{
			if ((i >= jam_ir_preamble) && (i < (count + jam_ir_preamble)))
			{
				tdi = (data[index >> 5] & (1L << (index & 0x1f))) ?
					TDI_HIGH : TDI_LOW;
				++index;
			}
			else tdi = TDI_HIGH;

			jam_jtag_io(TMS_LOW | tdi);
		}

		if (jam_ir_postamble == 0)
		{
			tdi = (data[index >> 5] & (1L << (index & 0x1f))) ?
				TDI_HIGH : TDI_LOW;
		}
		else tdi = TDI_HIGH;

		/* TMS high for last bit as we exit the SHIFT-IR state */
		jam_jtag_io(TMS_HIGH | tdi);

		jam_jtag_state = IREXIT1;

		status = jam_goto_jtag_state(jam_irstop_state);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_do_drscan
(
	long count,
	long *data,
	long start_index
)

/*																			*/
/*	Description:	Shifts data into data register (ignoring output data)	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int tdi = 0;
	long i = 0L;
	long index = start_index;
	long shift_count = count + jam_dr_preamble + jam_dr_postamble - 1;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	status = jam_goto_jtag_state(DRSHIFT);

	if (status == JAMC_SUCCESS)
	{
		/* loop in the SHIFT-DR state */
		for (i = 0; i < shift_count; i++)
		{
			if ((i >= jam_dr_preamble) && (i < (count + jam_dr_preamble)))
			{
				tdi = (data[index >> 5] & (1L << (index & 0x1f))) ?
					TDI_HIGH : TDI_LOW;
				++index;
			}
			else tdi = TDI_LOW;

			jam_jtag_io(TMS_LOW | tdi);
		}

		if (jam_dr_postamble == 0)
		{
			tdi = (data[index >> 5] & (1L << (index & 0x1f))) ?
				TDI_HIGH : TDI_LOW;
		}
		else tdi = TDI_LOW;

		/* TMS high for last bit as we exit the SHIFT-DR state */
		jam_jtag_io(TMS_HIGH | tdi);

		jam_jtag_state = DREXIT1;

		status = jam_goto_jtag_state(jam_drstop_state);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_swap_dr
(
	long count,
	long *in_data,
	long in_index,
	long *out_data,
	long out_index
)

/*																			*/
/*	Description:	Shifts data into data register, capturing output data	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int tdi = 0;
	int tdo = 0;
	long i = 0L;
	long shift_count = count + jam_dr_preamble + jam_dr_postamble - 1L;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	status = jam_goto_jtag_state(DRSHIFT);

	if (status == JAMC_SUCCESS)
	{
		/* loop in the SHIFT-DR state */
		for (i = 0; i < shift_count; i++)
		{
			if ((i >= jam_dr_preamble) && (i < (count + jam_dr_preamble)))
			{
				tdi = (in_data[in_index >> 5] & (1L << (in_index & 0x1f))) ?
					TDI_HIGH : TDI_LOW;
				++in_index;
			}
			else tdi = TDI_LOW;

			tdo = jam_jtag_io(tdi | TMS_LOW | READ_TDO);

			if ((i >= (jam_dr_preamble)) &&
				(i < (count + jam_dr_preamble)))
			{
				if (tdo)
				{
					out_data[out_index >> 5] |= (1L << (out_index & 0x1f));
				}
				else
				{
					out_data[out_index >> 5] &= ~(unsigned long)
						(1L << (out_index & 0x1f));
				}
				++out_index;
			}
		}

		if (jam_dr_postamble == 0)
		{
			tdi = (in_data[in_index >> 5] & (1L << (in_index & 0x1f))) ?
				TDI_HIGH : TDI_LOW;
		}
		else tdi = TDI_LOW;

		/* TMS high for last bit as we exit the SHIFT-DR state */
		tdo = jam_jtag_io(tdi | TMS_HIGH | READ_TDO);

		if (jam_dr_postamble == 0)
		{
			if (tdo)
			{
				out_data[out_index >> 5] |= (1L << (out_index & 0x1f));
			}
			else
			{
				out_data[out_index >> 5] &= ~(unsigned long)
					(1L << (out_index & 0x1f));
			}
		}

		jam_jtag_state = DREXIT1;

		status = jam_goto_jtag_state(jam_drstop_state);
	}

	return (status);
}
