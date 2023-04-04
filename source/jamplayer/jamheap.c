/****************************************************************************/
/*																			*/
/*	Module:			jamheap.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Heap management functions.  The heap is implemented as	*/
/*					a linked list of blocks of variable size.				*/
/*																			*/
/****************************************************************************/

#include "jamexprt.h"
#include "jamdefs.h"
#include "jamsym.h"
#include "jamstack.h"
#include "jamheap.h"
#include "jamutil.h"

/****************************************************************************/
/*																			*/
/*	Global variables														*/
/*																			*/
/****************************************************************************/

JAMS_HEAP_RECORD *jam_heap = NULL;

long jam_heap_records = 0L;

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_init_heap()

/*																			*/
/*	Description:	Initializes the heap area.  This is where all array		*/
/*					data is stored.											*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, or JAMC_OUT_OF_MEMORY if no	*/
/*					memory was available for the heap.						*/
/*																			*/
/****************************************************************************/
{
	JAM_RETURN_TYPE status = JAMC_SUCCESS;
	JAMS_SYMBOL_RECORD *symbol_table = (JAMS_SYMBOL_RECORD *) jam_workspace;
	JAMS_STACK_RECORD *stack =
		(JAMS_STACK_RECORD *) &symbol_table[JAMC_MAX_SYMBOL_COUNT];

	jam_heap = (JAMS_HEAP_RECORD *) &stack[JAMC_MAX_NESTING_DEPTH];

	jam_heap_records = 0L;

	/*
	*	Check that there is some memory available for the heap
	*/
	if (((long)jam_heap) > (((long)jam_workspace_size) + ((long)jam_workspace)))
	{
		status = JAMC_OUT_OF_MEMORY;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_add_heap_record
(
	JAMS_SYMBOL_RECORD *symbol_record,
	JAMS_HEAP_RECORD **heap_record,
	long dimension
)

/*																			*/
/*	Description:	Adds a heap record of the specified size to the heap.	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, or JAMC_OUT_OF_MEMORY if not	*/
/*					enough memory was available.							*/
/*																			*/
/****************************************************************************/
{
	int record = 0;
	int count = 0;
	int element = 0;
	long space_needed = 0L;
	BOOL cached = FALSE;
	JAMS_HEAP_RECORD *heap_ptr = NULL;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	heap_ptr = jam_heap;

	/*
	*	Step through the heap to find the next block to be used
	*/
	for (record = 0; (status == JAMC_SUCCESS) && (record < jam_heap_records);
		++record)
	{
		heap_ptr = heap_ptr->next;
		if ((heap_ptr < jam_heap) || (((long)heap_ptr) >
			(((long)jam_workspace) + ((long)jam_workspace_size))))
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	/*
	*	Compute space needed for array or cache buffer.  Initialized arrays
	*	will not be cached if their size is less than the cache buffer size.
	*/
	switch (symbol_record->type)
	{
	case JAM_INTEGER_ARRAY_WRITABLE:
		space_needed = dimension * sizeof(long);
		break;

	case JAM_BOOLEAN_ARRAY_WRITABLE:
		space_needed = ((dimension >> 5) + ((dimension & 0x1f) ? 1 : 0)) *
			sizeof(long);
		break;

	case JAM_INTEGER_ARRAY_INITIALIZED:
		space_needed = dimension * sizeof(long);
/*		if (space_needed > JAMC_ARRAY_CACHE_SIZE)	*/
/*		{											*/
/*			space_needed = JAMC_ARRAY_CACHE_SIZE;	*/
/*			cached = TRUE;							*/
/*		}											*/
		break;

	case JAM_BOOLEAN_ARRAY_INITIALIZED:
		space_needed = ((dimension >> 5) + ((dimension & 0x1f) ? 1 : 0)) *
			sizeof(long);
/*		if (space_needed > JAMC_ARRAY_CACHE_SIZE)	*/
/*		{											*/
/*			space_needed = JAMC_ARRAY_CACHE_SIZE;	*/
/*			cached = TRUE;							*/
/*		}											*/
		break;

	default:
		status = JAMC_INTERNAL_ERROR;
		break;
	}

	/*
	*	Check if there is enough space
	*/
	if (status == JAMC_SUCCESS)
	{
		if ((((long)heap_ptr) + (long)sizeof(JAMS_HEAP_RECORD) + space_needed -
			((long)jam_workspace)) > ((long)jam_workspace_size))
		{
			status = JAMC_OUT_OF_MEMORY;
		}
	}

	/*
	*	Add the new record to the heap
	*/
	if (status == JAMC_SUCCESS)
	{
		heap_ptr->next = (JAMS_HEAP_RECORD *) (((long)heap_ptr) +
			sizeof(JAMS_HEAP_RECORD) + space_needed);
		heap_ptr->symbol_record = symbol_record;
		heap_ptr->dimension = dimension;
		heap_ptr->cached = cached;
		heap_ptr->position = 0L;

		/* initialize data area to zero */
		count = (int) (space_needed / sizeof(long));
		for (element = 0; element < count; ++element)
		{
			heap_ptr->data[element] = 0L;
		}

		++jam_heap_records;

		*heap_record = heap_ptr;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

long jam_get_temp_workspace
(
	char **ptr
)

/*																			*/
/*	Description:	Gets a pointer to the unused area of the heap for		*/
/*					temporary use.  This area will be used for heap records	*/
/*					if jam_add_heap_record() is called.						*/
/*																			*/
/*	Returns:		length (in bytes) of unused workspace area available	*/
/*																			*/
/****************************************************************************/
{
	int record = 0;
	long space_available = 0L;
	JAMS_HEAP_RECORD *heap_ptr = NULL;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	heap_ptr = jam_heap;

	/*
	*	Step through the heap to find the next block to be used
	*/
	for (record = 0; (status == JAMC_SUCCESS) && (record < jam_heap_records);
		++record)
	{
		heap_ptr = heap_ptr->next;
		if ((heap_ptr < jam_heap) || (((long)heap_ptr) >
			(((long)jam_workspace) + ((long)jam_workspace_size))))
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	if (status == JAMC_SUCCESS)
	{
		(*ptr) = (char *)((long)heap_ptr + (long)sizeof(JAMS_HEAP_RECORD));

		space_available = ((long)jam_workspace + (long)jam_workspace_size) -
			((long)heap_ptr + (long)sizeof(JAMS_HEAP_RECORD));
	}

	return (space_available);
}
