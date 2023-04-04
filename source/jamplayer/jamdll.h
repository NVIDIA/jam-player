/****************************************************************************/
/*																			*/
/*	Module:			jamdll.h												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Header file for JAM Interpreter DLL interface			*/
/*																			*/
/****************************************************************************/

#ifndef INC_JAMDLL_H
#define INC_JAMDLL_H

/****************************************************************************/
/*																			*/
/*	Type definitions for function pointers									*/
/*																			*/
/****************************************************************************/

typedef JAM_RETURN_TYPE (*JAMT_EXECUTE)
(
	char **init_list,
	char *workspace,
	long size,
	long *error_line,
	int *exit_code
);

typedef JAM_RETURN_TYPE (*JAMT_GET_NOTE)
(
	long *offset,
	char *key,
	char *value,
	int length
);

typedef JAM_RETURN_TYPE (*JAMT_CHECK_CRC)
(
	unsigned short *expected_crc,
	unsigned short *actual_crc
);

typedef int (*JAMT_GETC)
(
	void
);

typedef int (*JAMT_SEEK)
(
	long offset
);

typedef int (*JAMT_JTAG_IO)
(
	int tms_tdi
);

typedef void (*JAMT_MESSAGE)
(
	char *message_text
);

typedef void (*JAMT_EXPORT)
(
	char *key,
	long value
);

typedef void (*JAMT_DELAY)
(
	long microseconds
);

typedef JAM_RETURN_TYPE (*JAMT_SET_CALLBACKS)
(
	JAMT_GETC get_ch,
	JAMT_SEEK seek,
	JAMT_JTAG_IO jtag_io,
	JAMT_MESSAGE message_func,
	JAMT_DELAY delay
);

/****************************************************************************/
/*																			*/
/*	Function names (required for GetProcAddress)							*/
/*																			*/
/****************************************************************************/

#define JAMC_EXECUTE_FUNC "jam_execute"
#define JAMC_GET_NOTE_FUNC "jam_get_note"
#define JAMC_CHECK_CRC_FUNC "jam_check_crc"
#define JAMC_SET_CALLBACKS_FUNC "jam_set_callbacks"

#endif /* INC_JAMDLL_H */
