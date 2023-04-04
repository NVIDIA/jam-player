/****************************************************************************/
/*																			*/
/*	Module:			jamdll.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Dynamic Link Library interface for JAM Interpreter		*/
/*																			*/
/****************************************************************************/

#include "jamexprt.h"
#include "jamdll.h"

/****************************************************************************/
/*																			*/
/*	Function pointers to store addresses of callback functions				*/
/*																			*/
/****************************************************************************/

JAMT_GETC jam_fn_getc = 0;
JAMT_SEEK jam_fn_seek = 0;
JAMT_JTAG_IO jam_fn_jtag_io = 0;
JAMT_MESSAGE jam_fn_message = 0;
JAMT_EXPORT jam_fn_export = 0;
JAMT_DELAY jam_fn_delay = 0;

/****************************************************************************/
/*																			*/
JAM_RETURN_TYPE jam_set_callbacks
(
	JAMT_GETC get_ch,
	JAMT_SEEK seek,
	JAMT_JTAG_IO jtag_io,
	JAMT_MESSAGE message_func,
	JAMT_EXPORT export_func,
	JAMT_DELAY delay
)
/*																			*/
/*	Description:	This function stores the addresses of the five			*/
/*					callback functions needed by the JAM interpreter		*/
/*																			*/
/*	Return:			JAMC_SUCCESS for success.								*/
/*																			*/
/****************************************************************************/
{
	/* save function pointers in global variables */
	jam_fn_getc = get_ch;
	jam_fn_seek = seek;
	jam_fn_jtag_io = jtag_io;
	jam_fn_message = message_func;
	jam_fn_export = export_func;
	jam_fn_delay = delay;

	return (JAMC_SUCCESS);
}

/****************************************************************************/
/*																			*/
/*	Match up functions to callback function pointers						*/
/*																			*/
/****************************************************************************/

int jam_getc(void)
{
	return (((jam_fn_getc == 0) ? (-1) : jam_fn_getc()));
}

int jam_seek(long offset)
{
	return ((jam_fn_seek == 0) ? (-1) : jam_fn_seek(offset));
}

int jam_jtag_io(int tms_tdi)
{
	return ((jam_fn_jtag_io == 0) ? 0 : jam_fn_jtag_io(tms_tdi));
}

void jam_message(char *message_text)
{
	if (jam_fn_message != 0) jam_fn_message(message_text);
}

void jam_export(char *key, long value)
{
	if (jam_fn_export != 0) jam_fn_export(key, value);
}

void jam_delay(long microseconds)
{
	if (jam_fn_delay != 0) jam_fn_delay(microseconds);
}
