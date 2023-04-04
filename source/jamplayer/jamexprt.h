/****************************************************************************/
/*																			*/
/*	Module:			jamexprt.h												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	JAM Interpreter Export Header File						*/
/*																			*/
/****************************************************************************/

#ifndef INC_JAMEXPRT_H
#define INC_JAMEXPRT_H

/****************************************************************************/
/*																			*/
/*	Return codes from most JAM functions									*/
/*																			*/
/****************************************************************************/

#define JAM_RETURN_TYPE int

#define JAMC_SUCCESS           0
#define JAMC_OUT_OF_MEMORY     1
#define JAMC_IO_ERROR          2
#define JAMC_SYNTAX_ERROR      3
#define JAMC_UNEXPECTED_END    4
#define JAMC_UNDEFINED_SYMBOL  5
#define JAMC_REDEFINED_SYMBOL  6
#define JAMC_INTEGER_OVERFLOW  7
#define JAMC_DIVIDE_BY_ZERO    8
#define JAMC_CRC_ERROR         9
#define JAMC_INTERNAL_ERROR   10
#define JAMC_BOUNDS_ERROR     11
#define JAMC_TYPE_MISMATCH    12
#define JAMC_ASSIGN_TO_CONST  13
#define JAMC_NEXT_UNEXPECTED  14
#define JAMC_POP_UNEXPECTED   15
#define JAMC_RETURN_UNEXPECTED 16
#define JAMC_ILLEGAL_SYMBOL   17
#define JAMC_UNSUPPORTED_FEATURE 18

/****************************************************************************/
/*																			*/
/*	Function Prototypes														*/
/*																			*/
/****************************************************************************/

JAM_RETURN_TYPE jam_execute
(
	char **init_list,
	char *workspace,
	long size,
	long *error_line,
	int *exit_code
);

JAM_RETURN_TYPE jam_get_note
(
	long *offset,
	char *key,
	char *value,
	int length
);

JAM_RETURN_TYPE jam_check_crc
(
	unsigned short *expected_crc,
	unsigned short *actual_crc
);

int jam_getc
(
	void
);

int jam_seek
(
	long offset
);

int jam_jtag_io
(
	int tms_tdi
);

void jam_message
(
	char *message_text
);

void jam_export
(
	char *key,
	long value
);

void jam_delay
(
	long microseconds
);

int jam_vector_map
(
	int signal_count,
	char **signals
);

int jam_vector_io
(
	int signal_count,
	long *dir_vect,
	long *data_vect,
	long *capture_vect
);

#endif /* INC_JAMEXPRT_H */