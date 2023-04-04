/****************************************************************************/
/*																			*/
/*	Module:			jamdefs.h												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Definitions of JAM constants and user-defined types		*/
/*																			*/
/****************************************************************************/

#ifndef INC_JAMDEFS_H
#define INC_JAMDEFS_H

#include <stdint.h>
/****************************************************************************/
/*																			*/
/*	Constant definitions													*/
/*																			*/
/****************************************************************************/

#ifndef NULL
#define NULL (0)
#endif
#define EOF (-1)
#define BOOL int
#define TRUE 1
#define FALSE 0
typedef uint32_t DWORD;
/* maximum quantities of some items */
#define JAMC_MAX_SYMBOL_COUNT 1021	/* should be a prime number */
#define JAMC_MAX_NESTING_DEPTH 16

/* size (in bytes) of cache buffer for initialized arrays */
#define JAMC_ARRAY_CACHE_SIZE 1024

/* character length limits */
#define JAMC_MAX_STATEMENT_LENGTH 1024
#define JAMC_MAX_NAME_LENGTH 32
#define JAMC_MAX_INSTR_LENGTH 8

/* character codes */
#define JAMC_COMMENT_CHAR   ('\'')
#define JAMC_QUOTE_CHAR     ('\"')
#define JAMC_COLON_CHAR     (':')
#define JAMC_SEMICOLON_CHAR (';')
#define JAMC_COMMA_CHAR     (',')
#define JAMC_NEWLINE_CHAR   ('\n')
#define JAMC_RETURN_CHAR    ('\r')
#define JAMC_TAB_CHAR       ('\t')
#define JAMC_SPACE_CHAR     (' ')
#define JAMC_EQUAL_CHAR     ('=')
#define JAMC_MINUS_CHAR     ('-')
#define JAMC_LPAREN_CHAR    ('(')
#define JAMC_RPAREN_CHAR    (')')
#define JAMC_LBRACKET_CHAR  ('[')
#define JAMC_RBRACKET_CHAR  (']')
#define JAMC_NULL_CHAR      ('\0')

/****************************************************************************/
/*																			*/
/*	Enumerated Types														*/
/*																			*/
/****************************************************************************/

/* instruction codes */
typedef enum
{
	JAM_ILLEGAL_INSTR = 0,
	JAM_BOOLEAN_INSTR,
	JAM_CALL_INSTR,
	JAM_CRC_INSTR,
	JAM_DRSCAN_INSTR,
	JAM_DRSTOP_INSTR,
	JAM_EXIT_INSTR,
	JAM_EXPORT_INSTR,
	JAM_FOR_INSTR,
	JAM_GOTO_INSTR,
	JAM_IF_INSTR,
	JAM_INTEGER_INSTR,
	JAM_IRSCAN_INSTR,
	JAM_IRSTOP_INSTR,
	JAM_LET_INSTR,
	JAM_NEXT_INSTR,
	JAM_NOTE_INSTR,
	JAM_PADDING_INSTR,
	JAM_POP_INSTR,
	JAM_PRINT_INSTR,
	JAM_PUSH_INSTR,
	JAM_REM_INSTR,
	JAM_RETURN_INSTR,
	JAM_STATE_INSTR,
	JAM_VECTOR_INSTR,
	JAM_VMAP_INSTR,
	JAM_WAIT_INSTR,
	JAM_INSTR_MAX

} JAME_INSTRUCTION;

/* types of expressions */
typedef enum
{
	JAM_ILLEGAL_EXPR_TYPE = 0,
	JAM_INTEGER_EXPR,
	JAM_BOOLEAN_EXPR,
	JAM_INT_OR_BOOL_EXPR,
	JAM_ARRAY_REFERENCE,
	JAM_EXPR_MAX

} JAME_EXPRESSION_TYPE;

/* mode of operation */
typedef enum
{
	JAM_ILLEGAL_MODE = 0,
	JAM_EXECUTE_MODE,
	JAM_NOTE_MODE,
	JAM_CRC_MODE,
	JAM_MODE_MAX

} JAME_MODE;

/****************************************************************************/
/*																			*/
/*	Global variables														*/
/*																			*/
/****************************************************************************/

extern char *jam_workspace;

extern long jam_workspace_size;

extern char **jam_init_list;

#endif /* INC_JAMDEFS_H */
