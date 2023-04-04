/****************************************************************************/
/*																			*/
/*	Module:			jamheap.h												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Prototypes for heap management functions				*/
/*																			*/
/****************************************************************************/

#ifndef INC_JAMHEAP_H
#define INC_JAMHEAP_H

/****************************************************************************/
/*																			*/
/*	Type definitions														*/
/*																			*/
/****************************************************************************/

/* types of stack records */
typedef enum
{
	JAM_ILLEGAL_HEAP_TYPE = 0,
	JAM_HEAP_INTEGER_ARRAY,
	JAM_HEAP_BOOLEAN_ARRAY,
	JAM_HEAP_BOOLEAN_ARRAY_CACHE,
	JAM_HEAP_MAX

} JAME_HEAP_RECORD_TYPE;

/* codes for Boolean data representation schemes */
typedef enum
{
	JAM_ILLEGAL_REP,
	JAM_BOOL_COMMA_SEP,
	JAM_BOOL_BINARY,
	JAM_BOOL_HEX,
	JAM_BOOL_RUN_LENGTH,
	JAM_BOOL_COMPRESSED

} JAME_BOOLEAN_REP;

/* heap record structure */
typedef struct JAMS_HEAP_STRUCT
{
	struct JAMS_HEAP_STRUCT *next;
	JAMS_SYMBOL_RECORD *symbol_record;
	JAME_BOOLEAN_REP rep;	/* data representation format */
	BOOL cached;		/* TRUE if array data is cached */
	long dimension;		/* number of elements in array */
	long position;		/* position in file of initialization data */
	long data[1];		/* first word of data (or cache buffer) */

} JAMS_HEAP_RECORD;

/****************************************************************************/
/*																			*/
/*	Global variables														*/
/*																			*/
/****************************************************************************/

extern JAMS_HEAP_RECORD *jam_heap;

/****************************************************************************/
/*																			*/
/*	Function prototypes														*/
/*																			*/
/****************************************************************************/

JAM_RETURN_TYPE jam_init_heap
(
	void
);

JAM_RETURN_TYPE jam_add_heap_record
(
	JAMS_SYMBOL_RECORD *symbol_record,
	JAMS_HEAP_RECORD **heap_record,
	long dimension
);

long jam_get_temp_workspace
(
	char **ptr
);

#endif /* INC_JAMHEAP_H */
