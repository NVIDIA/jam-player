/****************************************************************************/
/*																			*/
/*	Module:			jamsym.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Functions for maintaining symbol table, including		*/
/*					adding a synbol, searching for a symbol, and			*/
/*					modifying the value associated with a symbol			*/
/*																			*/
/****************************************************************************/

#include "jamexprt.h"
#include "jamdefs.h"
#include "jamsym.h"
#include "jamheap.h"
#include "jamutil.h"

/****************************************************************************/
/*																			*/
/*	Global variables														*/
/*																			*/
/****************************************************************************/

JAMS_SYMBOL_RECORD *jam_symbol_table = NULL;

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_init_symbol_table()

/*																			*/
/*	Description:	Initializes the symbol table.  The symbol table is		*/
/*					located at the beginning of the workspace buffer.		*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, or JAMC_OUT_OF_MEMORY if the	*/
/*					size of the workspace buffer is too small to hold the	*/
/*					desired number of symbol records.						*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	jam_symbol_table = (JAMS_SYMBOL_RECORD *) jam_workspace;

	if (jam_workspace_size <
		(long)(JAMC_MAX_SYMBOL_COUNT * sizeof(JAMS_SYMBOL_RECORD)))
	{
		status = JAMC_OUT_OF_MEMORY;
	}
	else
	{
		for (index = 0; index < JAMC_MAX_SYMBOL_COUNT; ++index)
		{
			jam_symbol_table[index].type = JAM_ILLEGAL_SYMBOL_TYPE;
			jam_symbol_table[index].name[0] = '\0';
			jam_symbol_table[index].value = 0L;
			jam_symbol_table[index].position = 0L;
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

BOOL jam_check_init_list
(
	char *name,
	long *value
)

/*																			*/
/*	Description:	Compares variable name to names in initialization list	*/
/*					and, if name is found, returns the corresponding		*/
/*					initialization value for the variable.					*/
/*																			*/
/*	Returns:		TRUE if variable was found, else FALSE					*/
/*																			*/
/****************************************************************************/
{
	char r, l;
	int ch_index = 0;
	int init_entry = 0;
	char *init_string = NULL;
	long val;
	BOOL match = FALSE;
	BOOL negate = FALSE;
	BOOL status = FALSE;

	if (jam_init_list != NULL)
	{
		while ((!match) && (jam_init_list[init_entry] != NULL))
		{
			init_string = jam_init_list[init_entry];
			match = TRUE;
			ch_index = 0;
			do
			{
				r = jam_toupper(init_string[ch_index]);
				if (!jam_is_name_char(r)) r = '\0';
				l = name[ch_index];
				match = (r == l);
				++ch_index;
			}
			while (match && (r != '\0') && (l != '\0'));

			if (match)
			{
				--ch_index;
				while (jam_isspace(init_string[ch_index])) ++ch_index;
				if (init_string[ch_index] == JAMC_EQUAL_CHAR)
				{
					++ch_index;
					while (jam_isspace(init_string[ch_index])) ++ch_index;

					if (init_string[ch_index] == JAMC_MINUS_CHAR)
					{
						++ch_index;
						negate = TRUE;
					}

					if (jam_isdigit(init_string[ch_index]))
					{
						val = jam_atol(&init_string[ch_index]);

						if (negate) val = 0L - val;

						if (value != NULL) *value = val;

						status = TRUE;
					}
				}
			}
			else
			{
				++init_entry;
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

int jam_hash
(
	char *name
)

/*																			*/
/*	Description:	Calcluates 'hash value' for a symbolic name.  This is	*/
/*					a pseudo-random number which is used as an offset into	*/
/*					the symbol table, as the initial position for the		*/
/*					symbol record.											*/
/*																			*/
/*	Returns:		An integer between 0 and JAMC_MAX_SYMBOL_COUNT-1		*/
/*																			*/
/****************************************************************************/
{
	int ch_index = 0;
	int hash = 0;

	while ((ch_index < JAMC_MAX_NAME_LENGTH) && (name[ch_index] != '\0'))
	{
		hash <<= 1;
		hash += (name[ch_index] & 0x1f);
		++ch_index;
	}
	if (hash < 0) hash = 0 - hash;

	return (hash % JAMC_MAX_SYMBOL_COUNT);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_add_symbol
(
	JAME_SYMBOL_TYPE type,
	char *name,
	long value,
	long position
)

/*																			*/
/*	Description:	Adds a new symbol to the symbol table.  If the symbol	*/
/*					name already exists in the symbol table, it is an error	*/
/*					unless the symbol type and the position in the file		*/
/*					where the symbol was declared are identical.  This is	*/
/*					necessary to allow labels and variable declarations		*/
/*					inside loops and subroutines where they may be			*/
/*					encountered multiple times.								*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, or JAMC_REDEFINED_SYMBOL		*/
/*					if symbol was already declared elsewhere, or			*/
/*					JAMC_OUT_OF_MEMORY if symbol table is full.				*/
/*																			*/
/****************************************************************************/
{
	char r, l;
	int sym_index = 0;
	int ch_index = 0;
	int hash = 0;
	long init_list_value = 0L;
	BOOL match = FALSE;
	BOOL identical_redeclaration = FALSE;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	/*
	*	Check for legal characters in name, and legal name length
	*/
	while (name[ch_index] != JAMC_NULL_CHAR)
	{
		if (!jam_is_name_char(name[ch_index++])) status = JAMC_ILLEGAL_SYMBOL;
	}

	if ((ch_index == 0) || (ch_index > JAMC_MAX_NAME_LENGTH))
	{
		status = JAMC_ILLEGAL_SYMBOL;
	}

	/*
	*	Get hash key for this name
	*/
	hash = jam_hash(name);
	sym_index = hash;

	/*
	*	Then check for duplicate entry in symbol table
	*/
	while ((status == JAMC_SUCCESS) &&
		(jam_symbol_table[sym_index].type != JAM_ILLEGAL_SYMBOL_TYPE) &&
		(!identical_redeclaration))
	{
		match = TRUE;
		ch_index = 0;
		do
		{
			r = jam_symbol_table[sym_index].name[ch_index];
			l = name[ch_index];
			match = (r == l);
			++ch_index;
		}
		while (match && (r != '\0') && (l != '\0'));

		if (match)
		{
			/*
			*	Check if symbol was already declared identically
			*	(same name, type, and source position)
			*/
			if ((jam_symbol_table[sym_index].type == type) &&
				(jam_symbol_table[sym_index].position == position))
			{
				/*
				*	For identical redeclaration, simply assign the value
				*/
				identical_redeclaration = TRUE;
				jam_symbol_table[sym_index].value = value;
			}
			else
			{
				status = JAMC_REDEFINED_SYMBOL;
			}
		}

		/*
		*	If position is occupied, look at next position
		*/
		if ((status == JAMC_SUCCESS) &&
			(jam_symbol_table[sym_index].type != JAM_ILLEGAL_SYMBOL_TYPE))
		{
			++sym_index;
			if (sym_index >= JAMC_MAX_SYMBOL_COUNT) sym_index = 0;

			if ((status == JAMC_SUCCESS) && (sym_index == hash))
			{
				/* the symbol is not in the table, and the table is full */
				status = JAMC_OUT_OF_MEMORY;
			}
		}
	}

	/*
	*	If no duplicate entry found, add the symbol
	*/
	if ((status == JAMC_SUCCESS) &&
		(jam_symbol_table[sym_index].type == JAM_ILLEGAL_SYMBOL_TYPE) &&
		(!identical_redeclaration))
	{
		/*
		*	Check initialization list -- if matching name is found,
		*	override the initialization value with the new value
		*/
		if (((type == JAM_INTEGER_SYMBOL) || (type == JAM_BOOLEAN_SYMBOL)) &&
			(jam_init_list != NULL))
		{
			if (jam_check_init_list(name, &init_list_value))
			{
				/* value was found -- override old value */
				value = init_list_value;
			}
		}

		/*
		*	Add the symbol
		*/
		jam_symbol_table[sym_index].type = type;
		jam_symbol_table[sym_index].value = value;
		jam_symbol_table[sym_index].position = position;

		ch_index = 0;
		while ((ch_index < JAMC_MAX_NAME_LENGTH) &&
			(name[ch_index] != '\0'))
		{
			jam_symbol_table[sym_index].name[ch_index] = name[ch_index];
			++ch_index;
		}
		jam_symbol_table[sym_index].name[ch_index] = '\0';
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAMS_SYMBOL_RECORD *jam_get_symbol_record
(
	char *name
)

/*																			*/
/*	Description:	Searches in symbol table for a symbol record with		*/
/*					matching name.											*/
/*																			*/
/*	Return:			Pointer to symbol record, or NULL if symbol not found	*/
/*																			*/
/****************************************************************************/
{
	char r, l;
	int sym_index = 0;
	int ch_index = 0;
	int hash = 0;
	BOOL match = FALSE;
	BOOL done = FALSE;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;

	/*
	*	Get hash key for this name
	*/
	hash = jam_hash(name);
	sym_index = hash;

	/*
	*	Search for name in symbol table
	*/
	while ((!done) && (symbol_record == NULL) &&
		(jam_symbol_table[sym_index].type != JAM_ILLEGAL_SYMBOL_TYPE))
	{
		match = TRUE;
		ch_index = 0;
		do
		{
			r = jam_symbol_table[sym_index].name[ch_index];
			l = name[ch_index];
			match = (r == l);
			++ch_index;
		}
		while (match && (r != '\0') && (l != '\0'));

		if (match)
		{
			symbol_record = &jam_symbol_table[sym_index];
		}

		++sym_index;
		if (sym_index >= JAMC_MAX_SYMBOL_COUNT) sym_index = 0;

		if ((!match) && (sym_index == hash)) done = TRUE;
	}

	return (symbol_record);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_get_symbol_value
(
	JAME_SYMBOL_TYPE type,
	char *name,
	long *value
)

/*																			*/
/*	Description:	Gets the value of a symbol based on the name and		*/
/*					symbol type.											*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else JAMC_UNDEFINED_SYMBOL	*/
/*					if symbol is not found									*/
/*																			*/
/****************************************************************************/
{
	char r, l;
	int sym_index = 0;
	int ch_index = 0;
	int hash = 0;
	BOOL match = FALSE;
	BOOL done = FALSE;
	JAM_RETURN_TYPE status = JAMC_UNDEFINED_SYMBOL;

	/*
	*	Get hash key for this name
	*/
	hash = jam_hash(name);
	sym_index = hash;

	/*
	*	Search for matching entry in symbol table
	*/
	while ((!match) && (!done) &&
		(jam_symbol_table[sym_index].type != JAM_ILLEGAL_SYMBOL_TYPE))
	{
		/* check for matching type... */
		if (jam_symbol_table[sym_index].type == type)
		{
			/* if type matches, check the name */
			match = TRUE;
			ch_index = 0;
			do
			{
				r = jam_symbol_table[sym_index].name[ch_index];
				l = name[ch_index];
				match = (r == l);
				++ch_index;
			}
			while (match && (r != '\0') && (l != '\0'));
		}

		/*
		*	If type and name match, return the value
		*/
		if (match)
		{
			if (value != 0) *value = jam_symbol_table[sym_index].value;
			status = JAMC_SUCCESS;
		}

		++sym_index;
		if (sym_index >= JAMC_MAX_SYMBOL_COUNT) sym_index = 0;

		if ((!match) && (sym_index == hash)) done = TRUE;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_set_symbol_value
(
	JAME_SYMBOL_TYPE type,
	char *name,
	long value
)

/*																			*/
/*	Description:	Assigns the value corresponding to a symbol, based on	*/
/*					the name and symbol type								*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else JAMC_UNDEFINED_SYMBOL	*/
/*					if symbol is not found									*/
/*																			*/
/****************************************************************************/
{
	char r, l;
	int sym_index = 0;
	int ch_index = 0;
	int hash = 0;
	BOOL match = FALSE;
	BOOL done = FALSE;
	JAM_RETURN_TYPE status = JAMC_UNDEFINED_SYMBOL;

	/*
	*	Get hash key for this name
	*/
	hash = jam_hash(name);
	sym_index = hash;

	/*
	*	Search for matching entry in symbol table
	*/
	while ((!match) && (!done) &&
		(jam_symbol_table[sym_index].type != JAM_ILLEGAL_SYMBOL_TYPE))
	{
		/* check for matching type... */
		if (jam_symbol_table[sym_index].type == type)
		{
			/* if type matches, check the name */
			match = TRUE;
			ch_index = 0;
			do
			{
				r = jam_symbol_table[sym_index].name[ch_index];
				l = name[ch_index];
				match = (r == l);
				++ch_index;
			}
			while (match && (r != '\0') && (l != '\0'));
		}

		/*
		*	If type and name match, set the value
		*/
		if (match)
		{
			jam_symbol_table[sym_index].value = value;
			status = JAMC_SUCCESS;
		}

		++sym_index;
		if (sym_index >= JAMC_MAX_SYMBOL_COUNT) sym_index = 0;

		if ((!match) && (sym_index == hash)) done = TRUE;
	}

	return (status);
}
