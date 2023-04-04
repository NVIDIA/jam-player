/****************************************************************************/
/*																			*/
/*	Module:			jamexec.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Contains the main entry point jam_execute(), and		*/
/*					other functions to implement the main execution loop.	*/
/*					This loop repeatedly calls jam_get_statement() and		*/
/*					jam_execute_statement() to process statements in		*/
/*					the JAM source file.									*/
/*																			*/
/****************************************************************************/

#include "jamexprt.h"
#include "jamdefs.h"
#include "jamexec.h"
#include "jamutil.h"
#include "jamexp.h"
#include "jamsym.h"
#include "jamstack.h"
#include "jamheap.h"
#include "jamarray.h"
#include "jamjtag.h"

/****************************************************************************/
/*																			*/
/*	Global variables														*/
/*																			*/
/****************************************************************************/

/* pointer to memory buffer for variable, symbol and stack storage */
char *jam_workspace = NULL;

/* size of available memory buffer */
long jam_workspace_size = 0L;

/* current position in input stream */
long jam_current_file_position = 0L;

/* position in input stream of the beginning of the current statement */
long jam_current_statement_position = 0L;

/* position of the beginning of the next statement (the one after the */
/* current statement, but not necessarily the next one to be executed) */
long jam_next_statement_position = 0L;

/* pointer to initialization list */
char **jam_init_list = NULL;

/* buffer for constant literal boolean array data */
#define JAMC_MAX_LITERAL_ARRAYS 3
long jam_literal_array_buffer[JAMC_MAX_LITERAL_ARRAYS];

/* number of vector signals */
int jam_vector_signal_count = 0;

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_get_statement
(
	char *statement_buffer,
	char *label_buffer
)

/*																			*/
/*	Description:	This function reads a full statement from the input		*/
/*					stream, preprocesses it to remove comments, and stores	*/
/*					it in a buffer.  If the statement is an array			*/
/*					declaration the initialization data is not stored in	*/
/*					the buffer but must be read from the input stream when	*/
/*					the array is used.										*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int label_index = 0;
	int ch = 0;
	int last_ch = 0;
	BOOL comment = FALSE;
	BOOL quoted_string = FALSE;
	BOOL boolean_array_data = FALSE;
	BOOL label_found = FALSE;
	BOOL done = FALSE;
	long position = jam_current_file_position;
	long first_char_position = -1L;
	long semicolon_position = -1L;
	long left_quote_position = -1L;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	label_buffer[0] = JAMC_NULL_CHAR;
	statement_buffer[0] = JAMC_NULL_CHAR;

	while (!done)
	{
		last_ch = ch;
		ch = jam_getc();

		if ((!comment) && (!quoted_string))
		{
			if (ch == JAMC_COMMENT_CHAR)
			{
				/* beginning of comment */
				comment = TRUE;
			}
			else if (ch == JAMC_QUOTE_CHAR)
			{
				/* beginning of quoted string */
				quoted_string = TRUE;
				left_quote_position = position;
			}
			else if (ch == JAMC_COLON_CHAR)
			{
				/* statement contains a label */
				if (label_found)
				{
					/* multiple labels found */
					status = JAMC_SYNTAX_ERROR;
					done = TRUE;
				}
				else if (index <= JAMC_MAX_NAME_LENGTH)
				{
					/* copy label into label_buffer */
					for (label_index = 0; label_index < index; label_index++)
					{
						label_buffer[label_index] =
							statement_buffer[label_index];
					}
					label_buffer[index] = JAMC_NULL_CHAR;
					label_found = TRUE;

					/* delete label from statement_buffer */
					index = 0;
					statement_buffer[0] = JAMC_NULL_CHAR;
					first_char_position = -1L;
					ch = JAMC_SPACE_CHAR;
				}
				else
				{
					/* label name was too long */
					status = JAMC_ILLEGAL_SYMBOL;
					done = TRUE;
				}
			}
			else if ((ch == JAMC_TAB_CHAR) ||
				(ch == JAMC_NEWLINE_CHAR) ||
				(ch == JAMC_RETURN_CHAR))
			{
				/* convert tab, CR, LF to space character */
				ch = JAMC_SPACE_CHAR;
			}
		}

		/* save character in statement_buffer */
		if ((!comment) && (index < JAMC_MAX_STATEMENT_LENGTH) &&
			((first_char_position != -1L) || (ch != JAMC_SPACE_CHAR)) &&
			(quoted_string ||
				(ch != JAMC_SPACE_CHAR) || (last_ch != JAMC_SPACE_CHAR)))
		{
			/* save the character */
			/* convert to upper case except quotes and boolean arrays */
			if (quoted_string || boolean_array_data)
			{
				statement_buffer[index] = (char) ch;
			}
			else
			{
				statement_buffer[index] = (char)
					(((ch >= 'a') && (ch <= 'z')) ? (ch - ('a' - 'A')) : ch);
			}
			++index;
			if (first_char_position == -1L) first_char_position = position;

			/*
			*	Whenever we see a right bracket character, check if the
			*	statement is a Boolean array declaration statement.
			*/
			if ((!boolean_array_data) && (ch == JAMC_RBRACKET_CHAR)
				&& (statement_buffer[0] == 'B'))
			{
				if (jam_strncmp(statement_buffer, "BOOLEAN", 7) == 0)
				{
					boolean_array_data = TRUE;
				}
			}
		}

		if ((!comment) && (!quoted_string) && (ch == JAMC_SEMICOLON_CHAR))
		{
			/* end of statement */
			done = TRUE;
			semicolon_position = position;
		}

		if (ch == EOF)
		{
			/* end of file */
			done = TRUE;
			status = JAMC_UNEXPECTED_END;
		}

		if (comment &&
			((ch == JAMC_NEWLINE_CHAR) || (ch == JAMC_RETURN_CHAR)))
		{
			/* end of comment */
			comment = FALSE;
		}
		else if (quoted_string && (ch == JAMC_QUOTE_CHAR) &&
			(position > left_quote_position))
		{
			/* end of quoted string */
			quoted_string = FALSE;
		}

		++position;	/* position of next character to be read */
	}

	if (index < JAMC_MAX_STATEMENT_LENGTH)
	{
		statement_buffer[index] = JAMC_NULL_CHAR;
	}
	else
	{
		statement_buffer[JAMC_MAX_STATEMENT_LENGTH] = JAMC_NULL_CHAR;
	}

	jam_current_file_position = position;

	if (first_char_position != -1L)
	{
		jam_current_statement_position = first_char_position;
	}

	if (semicolon_position != -1L)
	{
		jam_next_statement_position = semicolon_position + 1;
	}

	return (status);
}

struct JAMS_INSTR_MAP
{
	JAME_INSTRUCTION instruction;
	char string[JAMC_MAX_INSTR_LENGTH + 1];
} jam_instruction_table[] =
{
	{ JAM_BOOLEAN_INSTR, "BOOLEAN" },
	{ JAM_CALL_INSTR,    "CALL"    },
	{ JAM_CRC_INSTR,     "CRC"     },
	{ JAM_DRSCAN_INSTR,  "DRSCAN"  },
	{ JAM_DRSTOP_INSTR,  "DRSTOP"  },
	{ JAM_EXIT_INSTR,    "EXIT"    },
	{ JAM_EXPORT_INSTR,  "EXPORT"  },
	{ JAM_FOR_INSTR,     "FOR"     },
	{ JAM_GOTO_INSTR,    "GOTO"    },
	{ JAM_IF_INSTR,      "IF"      },
	{ JAM_INTEGER_INSTR, "INTEGER" },
	{ JAM_IRSCAN_INSTR,  "IRSCAN"  },
	{ JAM_IRSTOP_INSTR,  "IRSTOP"  },
	{ JAM_LET_INSTR,     "LET"     },
	{ JAM_NEXT_INSTR,    "NEXT"    },
	{ JAM_NOTE_INSTR,    "NOTE"    },
	{ JAM_PADDING_INSTR, "PADDING" },
	{ JAM_POP_INSTR,     "POP"     },
	{ JAM_PRINT_INSTR,   "PRINT"   },
	{ JAM_PUSH_INSTR,    "PUSH"    },
	{ JAM_REM_INSTR,     "REM"     },
	{ JAM_RETURN_INSTR,  "RETURN"  },
	{ JAM_STATE_INSTR,   "STATE"   },
	{ JAM_VECTOR_INSTR,  "VECTOR"  },
	{ JAM_VMAP_INSTR,    "VMAP"    },
	{ JAM_WAIT_INSTR,    "WAIT"    }
};

#define JAMC_INSTR_COUNT \
  ((int) (sizeof(jam_instruction_table) / sizeof(jam_instruction_table[0])))

/****************************************************************************/
/*																			*/

JAME_INSTRUCTION jam_get_instruction
(
	char *statement
)

/*																			*/
/*	Description:	This function extracts the instruction name from the	*/
/*					statement buffer and looks up the instruction code.		*/
/*																			*/
/*	Returns:		instruction code										*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int instr_index = 0;
	int length = 0;
	BOOL done = FALSE;
	JAME_INSTRUCTION instruction = JAM_ILLEGAL_INSTR;
	char instr_name[JAMC_MAX_INSTR_LENGTH + 1];

	/*
	*	Extract instruction name and convert to upper case
	*/
	for (index = 0; (!done) && (index < JAMC_MAX_INSTR_LENGTH); index++)
	{
		/* copy characters until non-alphabetic character */
		if ((statement[index] >= 'A') && (statement[index] <= 'Z'))
		{
			instr_name[index] = statement[index];
		}
		else if ((statement[index] >= 'a') && (statement[index] <= 'z'))
		{
			/* convert to upper case */
			instr_name[index] = (char) ((statement[index] - 'a') + 'A');
		}
		else
		{
			/* end of instruction name */
			instr_name[index] = JAMC_NULL_CHAR;
			length = index;
			done = TRUE;
		}
	}

	/*
	*	Search for instruction name in instruction table
	*/
	if (done && (length > 0))
	{
		done = FALSE;

		for (index = 0; (!done) && (index < JAMC_INSTR_COUNT); index++)
		{
			done = TRUE;

			for (instr_index = 0; done && (instr_index < length); instr_index++)
			{
				if (instr_name[instr_index] !=
					jam_instruction_table[index].string[instr_index])
				{
					done = FALSE;
				}
			}

			if (done)
			{
				instruction = jam_instruction_table[index].instruction;
			}
		}
	}

	return (instruction);
}

/****************************************************************************/
/*																			*/

int jam_skip_instruction_name
(
	char *statement_buffer
)

/*																			*/
/*	Description:	This function skips over the first "word" in the		*/
/*					statement buffer, which is assumed to be the name of	*/
/*					the instruction, and returns the index of the next		*/
/*					non-white-space character in the buffer.				*/
/*																			*/
/*	Returns:		index of statement text after instruction name			*/
/*																			*/
/****************************************************************************/
{
	int index = 0;

	while ((jam_isspace(statement_buffer[index])) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;	/* skip over white space */
	}

	while ((jam_is_name_char(statement_buffer[index])) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;	/* skip over instruction name */
	}

	while ((jam_isspace(statement_buffer[index])) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;	/* skip over white space */
	}

	return (index);
}

/****************************************************************************/
/*																			*/

int jam_find_keyword
(
	char *buffer,
	char *keyword
)

/*																			*/
/*	Description:	This function searches in the statement buffer for the	*/
/*					specified keyword.										*/
/*																			*/
/*	Returns:		index of keyword in buffer, or -1 if keyword not found	*/
/*																			*/
/****************************************************************************/
{
	BOOL found = FALSE;
	int index = 0;
	int buffer_length = jam_strlen(buffer);
	int keyword_length = jam_strlen(keyword);

	/* look at beginning of string */
	if ((buffer[0] == keyword[0]) &&
		(jam_strncmp(buffer, keyword, keyword_length) == 0) &&
		(!jam_is_name_char(buffer[keyword_length])))
	{
		found = TRUE;
	}

	/* look inside string */
	while ((!found) && (index + keyword_length <= buffer_length))
	{
		if ((buffer[index + 1] == keyword[0]) &&
			(!jam_is_name_char(buffer[index])) &&
			(jam_strncmp(&buffer[index + 1], keyword, keyword_length) == 0) &&
			(!jam_is_name_char(buffer[index + keyword_length + 1])))
		{
			found = TRUE;
		}

		++index;
	}

	return (found ? index : -1);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_get_array_subrange
(
	char *statement_buffer,
	long *start_index,
	long *stop_index
)

/*																			*/
/*	Description:	Gets start_index and stop_index of an array subrange	*/
/*					specification of the form <start_index>..<stop_index>	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	char save_ch = 0;
	BOOL found_elipsis = FALSE;
	BOOL found = FALSE;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;

	while ((statement_buffer[index] != JAMC_NULL_CHAR) && !found_elipsis)
	{
		if ((statement_buffer[index] == '.') &&
			(statement_buffer[index + 1] == '.'))
		{
			expr_end = index;
			found_elipsis = TRUE;
		}
		++index;
	}

	if (found_elipsis && (expr_end > expr_begin))
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_evaluate_expression(
			&statement_buffer[expr_begin], start_index, &expr_type);
		statement_buffer[expr_end] = save_ch;

		/*
		*	Check for integer expression
		*/
		if ((status == JAMC_SUCCESS) &&
			(expr_type != JAM_INTEGER_EXPR) &&
			(expr_type != JAM_INT_OR_BOOL_EXPR))
		{
			status = JAMC_TYPE_MISMATCH;
		}
	}
	else
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		expr_begin = expr_end + 2;

		status = jam_evaluate_expression(
			&statement_buffer[expr_begin], stop_index, &expr_type);

		if ((status == JAMC_SUCCESS) &&
			(expr_type != JAM_INTEGER_EXPR) &&
			(expr_type != JAM_INT_OR_BOOL_EXPR))
		{
			status = JAMC_TYPE_MISMATCH;
		}
		else
		{
			found = TRUE;
		}
	}

	if ((status == JAMC_SUCCESS) && (!found))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_convert_literal_array
(
	char *statement_buffer,
	long **output_buffer,
	long *length,
	int arg
)

/*																			*/
/*	Description:	converts HEX string in statement buffer into binary		*/
/*					values.  Stores binary result back into the buffer,		*/
/*					overwriting the input text.								*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int in_index = 0;
	int out_index = 0;
	int rev_index = 0;
	int i = 0;
	int j = 0;
	char ch = 0;
	int data = 0;
	long *long_ptr = NULL;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	arg = arg;

	while ((status == JAMC_SUCCESS) &&
		((ch = statement_buffer[in_index]) != '\0'))
	{
		if ((ch >= 'A') && (ch <= 'F'))
		{
			data = (int) (ch + 10 - 'A');
		}
		else if ((ch >= 'a') && (ch <= 'f'))
		{
			data = (int) (ch + 10 - 'a');
		}
		else if ((ch >= '0') && (ch <= '9'))
		{
			data = (int) (ch - '0');
		}
		else
		{
			status = JAMC_SYNTAX_ERROR;
		}

		if (status == JAMC_SUCCESS)
		{
			if (in_index & 1)
			{
				/* odd nibble is lower nibble */
				data |= (statement_buffer[out_index] & 0xf0);
				statement_buffer[out_index] = (char) data;
				++out_index;
			}
			else
			{
				/* even nibble is upper nibble */
				statement_buffer[out_index] = (char) (data << 4);
			}
		}

		++in_index;
	}

	if (status == JAMC_SUCCESS)
	{
		*length = (long) in_index * 4L;

		if (in_index & 1)
		{
			/* odd number of nibbles - do a nibble-shift */
			out_index = in_index / 2;
			while (out_index > 0)
			{
				statement_buffer[out_index] = (char)
					(((statement_buffer[out_index - 1] & 0x0f) << 4) |
					((statement_buffer[out_index] & 0xf0) >> 4));
				--out_index;
			}
			statement_buffer[0] = (char) ((statement_buffer[0] & 0xf0) >> 4);
			++in_index;
		}

		/* reverse the order of binary data */
		out_index = in_index / 2;	/* number of bytes */
		rev_index = out_index / 2;
		while (rev_index > 0)
		{
			ch = statement_buffer[rev_index - 1];
			statement_buffer[rev_index - 1] =
				statement_buffer[out_index - rev_index];
			statement_buffer[out_index - rev_index] = ch;
			--rev_index;
		}

		out_index = (in_index + 1) / 2;		/* number of bytes */
		rev_index = (out_index + 3) / 4;	/* number of longs */

		if (rev_index > 1)
		{
			long_ptr = (long *) (((long) statement_buffer) & 0xfffffffcL);
		}
		else if (arg < JAMC_MAX_LITERAL_ARRAYS)
		{
			long_ptr = &jam_literal_array_buffer[arg];
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	if (status == JAMC_SUCCESS)
	{
		for (i = 0; i < rev_index; ++i)
		{
			j = i * 4;
			long_ptr[i] = (
				(((long)statement_buffer[j + 3] << 24L) & 0xff000000L) |
				(((long)statement_buffer[j + 2] << 16L) & 0x00ff0000L) |
				(((long)statement_buffer[j + 1] <<  8L) & 0x0000ff00L) |
				(((long)statement_buffer[j    ]       ) & 0x000000ffL));
		}

		if (output_buffer != NULL) *output_buffer = long_ptr;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_get_array_argument
(
	char *statement_buffer,
	JAMS_SYMBOL_RECORD **symbol_record,
	long **literal_array_data,
	long *start_index,
	long *stop_index,
	int arg
)

/*																			*/
/*	Description:	Looks for a sub-range-indexed array argument in the		*/
/*					statement buffer.  Calls expression parser to evaluate	*/
/*					the start_index and end_index arguments.				*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int bracket_count = 0;
	long literal_array_length = 0;
	char save_ch = 0;
	JAMS_SYMBOL_RECORD *tmp_symbol_rec = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	/* first look for literal array constant */
	while ((jam_isspace(statement_buffer[index])) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;	/* skip over white space */
	}

	if (jam_isdigit(statement_buffer[index]))
	{
		/* it is a literal array constant */
		*symbol_record = NULL;
		expr_begin = index;

		while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
			(statement_buffer[index] != JAMC_COMMA_CHAR) &&
			(statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;
		}
		while ((index > expr_begin) && jam_isspace(statement_buffer[index - 1]))
		{
			--index;
		}
		expr_end = index;
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_convert_literal_array(&statement_buffer[expr_begin],
			literal_array_data, &literal_array_length, arg);
		statement_buffer[expr_end] = save_ch;

		*start_index = 0L;
		*stop_index = literal_array_length - 1;
	}
	else
	{
		/* it is not a literal constant, look for array variable */
		*literal_array_data = NULL;

		while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
			(statement_buffer[index] != JAMC_LBRACKET_CHAR) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;
		}

		if (statement_buffer[index] != JAMC_LBRACKET_CHAR)
		{
			status = JAMC_SYNTAX_ERROR;
		}
		else
		{
			expr_end = index;
			++index;

			save_ch = statement_buffer[expr_end];
			statement_buffer[expr_end] = JAMC_NULL_CHAR;
			tmp_symbol_rec = jam_get_symbol_record(&statement_buffer[expr_begin]);
			statement_buffer[expr_end] = save_ch;

			if (tmp_symbol_rec == NULL)
			{
				status = JAMC_UNDEFINED_SYMBOL;
			}
			else
			{
				*symbol_record = tmp_symbol_rec;

				if ((tmp_symbol_rec->type != JAM_BOOLEAN_ARRAY_WRITABLE) &&
					(tmp_symbol_rec->type != JAM_BOOLEAN_ARRAY_INITIALIZED))
				{
					status = JAMC_TYPE_MISMATCH;
				}
				else
				{
					/* it is a Boolean array variable */
					while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
						(statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
						((statement_buffer[index] != JAMC_RBRACKET_CHAR) ||
							(bracket_count > 0)) &&
						(index < JAMC_MAX_STATEMENT_LENGTH))
					{
						if (statement_buffer[index] == JAMC_LBRACKET_CHAR)
						{
							++bracket_count;
						}
						else if (statement_buffer[index] == JAMC_RBRACKET_CHAR)
						{
							--bracket_count;
						}

						++index;
					}

					if (statement_buffer[index] != JAMC_RBRACKET_CHAR)
					{
						status = JAMC_SYNTAX_ERROR;
					}
					else
					{
						statement_buffer[index] = JAMC_NULL_CHAR;

						status = jam_get_array_subrange(
							&statement_buffer[expr_end + 1],
							start_index, stop_index);
						statement_buffer[index] = JAMC_RBRACKET_CHAR;
						++index;

						if (status == JAMC_SUCCESS)
						{
							heap_record = (JAMS_HEAP_RECORD *)
								tmp_symbol_rec->value;

							if (heap_record == NULL)
							{
								status = JAMC_INTERNAL_ERROR;
							}
							else if ((*start_index < 0) || (*stop_index < 0) ||
								(*start_index >= heap_record->dimension) ||
								(*stop_index >= heap_record->dimension))
							{
								status = JAMC_BOUNDS_ERROR;
							}
							else
							{
								while (jam_isspace(statement_buffer[index]))
								{
									++index;
								}

								/* there should be no more characters */
								if (statement_buffer[index] != JAMC_NULL_CHAR)
								{
									status = JAMC_SYNTAX_ERROR;
								}
							}
						}
					}
				}
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_find_argument
(
	char *statement_buffer,
	int *begin,
	int *end,
	int *delimiter
)

/*																			*/
/*	Description:	Finds the next argument in the statement buffer, where	*/
/*					the delimiters are COLON or SEMICOLON.  Returns indices	*/
/*					of begin and end of argument, and the delimiter after	*/
/*					the argument.											*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	while ((jam_isspace(statement_buffer[index])) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;	/* skip over white space */
	}

	*begin = index;

	while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
		(statement_buffer[index] != JAMC_COMMA_CHAR) &&
		(statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;
	}

	if ((statement_buffer[index] != JAMC_COMMA_CHAR) &&
		(statement_buffer[index] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}
	else
	{
		*delimiter = index;	/* delimiter is position of comma or semicolon */

		while (jam_isspace(statement_buffer[index - 1]))
		{
			--index;	/* skip backwards over white space */
		}

		*end = index;	/* end is position after last argument character */
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_boolean
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes a BOOLEAN variable declaration statement		*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int variable_begin = 0;
	int variable_end = 0;
	int dim_begin = 0;
	int dim_end = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	long dim_value = 0L;
	long init_value = 0L;
	char save_ch = 0;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	if (jam_isalpha(statement_buffer[index]))
	{
		/* locate variable name */
		variable_begin = index;
		while ((jam_is_name_char(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over variable name */
		}
		variable_end = index;

		while ((jam_isspace(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over white space */
		}

		if (statement_buffer[index] == JAMC_LBRACKET_CHAR)
		{
			/*
			*	Array declaration
			*/
			dim_begin = index + 1;
			while ((statement_buffer[index] != JAMC_RBRACKET_CHAR) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;	/* find matching bracket */
			}
			if (statement_buffer[index] == JAMC_RBRACKET_CHAR)
			{
				dim_end = index;
				++index;
			}
			while ((jam_isspace(statement_buffer[index])) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;	/* skip over white space */
			}

			if (dim_end > dim_begin)
			{
				save_ch = statement_buffer[dim_end];
				statement_buffer[dim_end] = JAMC_NULL_CHAR;
				status = jam_evaluate_expression(
					&statement_buffer[dim_begin], &dim_value, &expr_type);
				statement_buffer[dim_end] = save_ch;
			}

			/*
			*	Check for integer expression
			*/
			if ((status == JAMC_SUCCESS) &&
				(expr_type != JAM_INTEGER_EXPR) &&
				(expr_type != JAM_INT_OR_BOOL_EXPR))
			{
				status = JAMC_TYPE_MISMATCH;
			}

			if (status == JAMC_SUCCESS)
			{
				/*
				*	Add the array name to the symbol table
				*/
				save_ch = statement_buffer[variable_end];
				statement_buffer[variable_end] = JAMC_NULL_CHAR;
				status = jam_add_symbol(JAM_BOOLEAN_ARRAY_WRITABLE,
					&statement_buffer[variable_begin], 0L,
					jam_current_statement_position);

				/* get a pointer to the symbol record */
				if (status == JAMC_SUCCESS)
				{
					symbol_record = jam_get_symbol_record(
						&statement_buffer[variable_begin]);
					if (symbol_record == NULL) status = JAMC_INTERNAL_ERROR;
				}
				statement_buffer[variable_end] = save_ch;
			}

			if ((status == JAMC_SUCCESS) && (symbol_record != NULL))
			{
				if (statement_buffer[index] == JAMC_EQUAL_CHAR)
				{
					/*
					*	Array has initialization data
					*/
					symbol_record->type = JAM_BOOLEAN_ARRAY_INITIALIZED;

					status = jam_add_heap_record(symbol_record, &heap_record,
						dim_value);

					if (status == JAMC_SUCCESS)
					{
						symbol_record->value = (long) heap_record;

						/*
						*	Initialize heap data (or cache buffer) for array
						*/
						status = jam_read_boolean_array_data(heap_record,
							&statement_buffer[index + 1]);
					}
				}
				else if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
				{
					/*
					*	Array has no initialization data.
					*	Allocate a buffer on the heap:
					*/
					status = jam_add_heap_record(symbol_record, &heap_record,
						dim_value);

					if (status == JAMC_SUCCESS)
					{
						symbol_record->value = (long) heap_record;
					}
				}
			}
			else
			{
				/* this should be end of the statement */
				status = JAMC_SYNTAX_ERROR;
			}
		}
		else
		{
			/*
			*	Scalar variable declaration
			*/
			if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
			{
				status = JAMC_SUCCESS;
			}
			else if (statement_buffer[index] == JAMC_EQUAL_CHAR)
			{
				/*
				*	Evaluate initialization expression
				*/
				++index;
				status = jam_find_argument(&statement_buffer[index],
					&expr_begin, &expr_end, &delimiter);

				expr_begin += index;
				expr_end += index;
				delimiter += index;

				if ((status == JAMC_SUCCESS) &&
					(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
				{
					status = JAMC_SYNTAX_ERROR;
				}

				if ((status == JAMC_SUCCESS) && (expr_end > expr_begin))
				{
					save_ch = statement_buffer[expr_end];
					statement_buffer[expr_end] = JAMC_NULL_CHAR;
					status = jam_evaluate_expression(
						&statement_buffer[expr_begin], &init_value, &expr_type);
					statement_buffer[expr_end] = save_ch;
				}

				/*
				*	Check for Boolean expression
				*/
				if ((status == JAMC_SUCCESS) &&
					(expr_type != JAM_BOOLEAN_EXPR) &&
					(expr_type != JAM_INT_OR_BOOL_EXPR))
				{
					status = JAMC_TYPE_MISMATCH;
				}
			}

			if (status == JAMC_SUCCESS)
			{
				/*
				*	Add the variable name to the symbol table
				*/
				save_ch = statement_buffer[variable_end];
				statement_buffer[variable_end] = JAMC_NULL_CHAR;
				status = jam_add_symbol(JAM_BOOLEAN_SYMBOL,
					&statement_buffer[variable_begin],
					init_value, jam_current_statement_position);
				statement_buffer[variable_end] = save_ch;
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_call_or_goto
(
	char *statement_buffer,
	BOOL call_statement
)

/*																			*/
/*	Description:	Processes a CALL or GOTO statement.	 If it is a CALL	*/
/*					statement, a stack record is pushed onto the stack.		*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int label_begin = 0;
	int label_end = 0;
	char save_ch = 0;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	long current_position = 0L;
	long goto_position = -1L;
	long return_position = jam_next_statement_position;
	char label_buffer[JAMC_MAX_NAME_LENGTH + 1];
	char goto_label[JAMC_MAX_NAME_LENGTH + 1];
	BOOL done = FALSE;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Extract the label name from the statement buffer.
	*/
	if (jam_isalpha(statement_buffer[index]))
	{
		/* locate label name */
		label_begin = index;
		while ((jam_is_name_char(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over label name */
		}
		label_end = index;

		while ((jam_isspace(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over white space */
		}

		if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
		{
			/*
			*	Look in symbol table for label
			*/
			save_ch = statement_buffer[label_end];
			statement_buffer[label_end] = JAMC_NULL_CHAR;
			symbol_record = jam_get_symbol_record(
				&statement_buffer[label_begin]);

			if ((symbol_record != NULL) &&
				(symbol_record->type == JAM_LABEL))
			{
				/*
				*	Label is defined - get the address for the jump
				*/
				goto_position = symbol_record->position;
				status = JAMC_SUCCESS;
			}
			else if (symbol_record == NULL)
			{
				/*
				*	Label is not defined... may be a forward reference.
				*	Search through the file to find the symbol.
				*/
				current_position = jam_current_statement_position;

				jam_strcpy(goto_label, &statement_buffer[label_begin]);

				status = JAMC_SUCCESS;

				while ((!done) && (status == JAMC_SUCCESS))
				{
					/*
					*	Get statements without executing them
					*/
					status = jam_get_statement(statement_buffer, label_buffer);

					if ((status == JAMC_SUCCESS) &&
						(label_buffer[0] != JAMC_NULL_CHAR))
					{
						/*
						*	If there is a label, add it to the symbol table
						*/
						status = jam_add_symbol(JAM_LABEL, label_buffer, 0L,
							jam_current_statement_position);

						/*
						*	Is it the label we are looking for?
						*/
						if ((status == JAMC_SUCCESS) &&
							(jam_strcmp(label_buffer, goto_label) == 0))
						{
							/*
							*	We found the label we were looking for.
							*	Get the address for the jump.
							*/
							done = TRUE;
							goto_position = jam_current_statement_position;
						}
					}
				}

				if (!done)
				{
					/* label was not found -- report "undefined symbol" */
					/* rather than "unexpected EOF" */
					status = JAMC_UNDEFINED_SYMBOL;

					/* seek to location of the CALL or GOTO statement */
					/* which caused the error */
					jam_seek(current_position);
					jam_current_file_position = current_position;
					jam_current_statement_position = current_position;
				}
			}

			statement_buffer[label_end] = save_ch;

			/*
			*	If this is a CALL statement (not a GOTO) then push a CALL
			*	record onto the stack
			*/
			if ((call_statement) && (status == JAMC_SUCCESS) &&
				(goto_position != (-1L)))
			{
				status = jam_push_callret_record(return_position);
			}

			/*
			*	Now seek to the desired position so we can execute that
			*	statement next
			*/
			if ((status == JAMC_SUCCESS) && (goto_position != (-1L)))
			{
				if (jam_seek(goto_position) == 0)
				{
					jam_current_file_position = goto_position;
				}
				else
				{
					/* seek failed */
					status = JAMC_IO_ERROR;
				}
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_drscan_compare
(
	char *statement_buffer,
	long count_value,
	long *in_data,
	long in_index
)

/*																			*/
/*	Description:	Processes the arguments for the COMPARE version of the	*/
/*					DRSCAN statement.  Calls jam_swap_dr() to access the	*/
/*					JTAG hardware interface.								*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{

/* syntax: DRSCAN <length> [, <data>] [COMPARE <array>, <mask>, <result>] ; */

	int bit = 0;
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	int actual = 0;
	int expected = 0;
	int mask = 0;
	long comp_start_index = 0L;
	long comp_stop_index = 0L;
	long mask_start_index = 0L;
	long mask_stop_index = 0L;
	long space_available = 0L;
	char save_ch = 0;
	long *temp_array = NULL;
	BOOL result = TRUE;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	long *comp_data = NULL;
	long *mask_data = NULL;
	long *literal_array_data = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	/*
	*	Statement buffer should contain the part of the statement string
	*	after the COMPARE keyword.
	*
	*	The first argument should be the compare array.
	*/
	status = jam_find_argument(statement_buffer,
		&expr_begin, &expr_end, &delimiter);

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_get_array_argument(&statement_buffer[expr_begin],
			&symbol_record, &literal_array_data,
			&comp_start_index, &comp_stop_index, 1);
		statement_buffer[expr_end] = save_ch;
		index = delimiter + 1;
	}

	if ((status == JAMC_SUCCESS) &&
		(literal_array_data != NULL) &&
		(comp_start_index == 0) &&
		(comp_stop_index > count_value - 1))
	{
		comp_stop_index = count_value - 1;
	}

	if ((status == JAMC_SUCCESS) &&
		(comp_stop_index != comp_start_index + count_value - 1))
	{
		status = JAMC_BOUNDS_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		if (symbol_record != NULL)
		{
			heap_record = (JAMS_HEAP_RECORD *)symbol_record->value;

			if (heap_record != NULL)
			{
				comp_data = heap_record->data;
			}
			else
			{
				status = JAMC_INTERNAL_ERROR;
			}
		}
		else if (literal_array_data != NULL)
		{
			comp_data = literal_array_data;
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	/*
	*	Find the next argument -- should be the mask array
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		expr_begin += index;
		expr_end += index;
		delimiter += index;
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_get_array_argument(&statement_buffer[expr_begin],
			&symbol_record, &literal_array_data,
			&mask_start_index, &mask_stop_index, 2);
		statement_buffer[expr_end] = save_ch;
		index = delimiter + 1;
	}

	if ((status == JAMC_SUCCESS) &&
		(literal_array_data != NULL) &&
		(mask_start_index == 0) &&
		(mask_stop_index > count_value - 1))
	{
		mask_stop_index = count_value - 1;
	}

	if ((status == JAMC_SUCCESS) &&
		(mask_stop_index != mask_start_index + count_value - 1))
	{
		status = JAMC_BOUNDS_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		if (symbol_record != NULL)
		{
			heap_record = (JAMS_HEAP_RECORD *)symbol_record->value;

			if (heap_record != NULL)
			{
				mask_data = heap_record->data;
			}
			else
			{
				status = JAMC_INTERNAL_ERROR;
			}
		}
		else if (literal_array_data != NULL)
		{
			mask_data = literal_array_data;
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	/*
	*	Find the third argument -- should be the result variable
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		expr_begin += index;
		expr_end += index;
		delimiter += index;
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	/*
	*	Result must be a scalar Boolean variable
	*/
	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		symbol_record = jam_get_symbol_record(&statement_buffer[expr_begin]);
		statement_buffer[expr_end] = save_ch;

		if (symbol_record == NULL)
		{
			status = JAMC_UNDEFINED_SYMBOL;
		}
		else if (symbol_record->type != JAM_BOOLEAN_SYMBOL)
		{
			status = JAMC_TYPE_MISMATCH;
		}
	}

	/*
	*	Find some free memory on the heap
	*/
	if (status == JAMC_SUCCESS)
	{
		space_available = jam_get_temp_workspace((char **) &temp_array);

		if (space_available < (count_value >> 3) + 4)
		{
			status = JAMC_OUT_OF_MEMORY;
		}
	}

	/*
	*	Do the JTAG operation, saving the result in temp_array
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_swap_dr(count_value, in_data, in_index, temp_array, 0);
	}

	/*
	*	Mask the data and do the comparison
	*/
	if (status == JAMC_SUCCESS)
	{
		for (bit = 0; (bit < count_value) && result; ++bit)
		{
			actual = temp_array[bit >> 5] & (1L << (bit & 0x1f)) ? 1 : 0;
			expected = comp_data[(bit + comp_start_index) >> 5]
				& (1L << ((bit + comp_start_index) & 0x1f)) ? 1 : 0;
			mask = mask_data[(bit + mask_start_index) >> 5]
				& (1L << ((bit + mask_start_index) & 0x1f)) ? 1 : 0;

			if ((actual & mask) != (expected & mask))
			{
				result = FALSE;
			}
		}

		symbol_record->value = result ? 1L : 0L;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_drscan_capture
(
	char *statement_buffer,
	long count_value,
	long *in_data,
	long in_index
)

/*																			*/
/*	Description:	Processes the arguments for the CAPTURE version of the	*/
/*					DRSCAN statement.  Calls jam_swap_dr() to access the	*/
/*					JTAG hardware interface.								*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	/* syntax:  DRSCAN <length> [, <data>] [CAPTURE <array>] ; */

	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	long start_index = 0L;
	long stop_index = 0L;
	char save_ch = 0;
	long *tdi_data = NULL;
	long *literal_array_data = NULL;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	/*
	*	Statement buffer should contain the part of the statement string
	*	after the CAPTURE keyword.
	*
	*	The only argument should be the capture array.
	*/
	status = jam_find_argument(statement_buffer,
		&expr_begin, &expr_end, &delimiter);

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_get_array_argument(&statement_buffer[expr_begin],
			&symbol_record, &literal_array_data,
			&start_index, &stop_index, 1);
		statement_buffer[expr_end] = save_ch;
	}

	if ((status == JAMC_SUCCESS) && (literal_array_data != NULL))
	{
		/* literal array may not be used for capture buffer */
		status = JAMC_SYNTAX_ERROR;
	}

	if ((status == JAMC_SUCCESS) &&
		(stop_index != start_index + count_value - 1))
	{
		status = JAMC_BOUNDS_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		if (symbol_record != NULL)
		{
			heap_record = (JAMS_HEAP_RECORD *)symbol_record->value;

			if (heap_record != NULL)
			{
				tdi_data = heap_record->data;
			}
			else
			{
				status = JAMC_INTERNAL_ERROR;
			}
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	/*
	*	Perform the JTAG operation, capturing data into the heap buffer
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_swap_dr(count_value, in_data, in_index,
			tdi_data, start_index);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_drscan
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes DRSCAN statement, which shifts data through	*/
/*					a data register of the JTAG interface					*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	/* syntax:  DRSCAN <length> [, <data>] [CAPTURE <array>] ; */
	/* or:  DRSCAN <length> [, <data>] [COMPARE <array>, <mask>, <result>] ; */

	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	long count_value = 0L;
	long start_index = 0L;
	long stop_index = 0L;
	char save_ch = 0;
	long *tdi_data = NULL;
	long *literal_array_data = NULL;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	/* locate length */
	status = jam_find_argument(&statement_buffer[index],
		&expr_begin, &expr_end, &delimiter);

	expr_begin += index;
	expr_end += index;
	delimiter += index;

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_evaluate_expression(
			&statement_buffer[expr_begin], &count_value, &expr_type);
		statement_buffer[expr_end] = save_ch;
	}

	/*
	*	Check for integer expression
	*/
	if ((status == JAMC_SUCCESS) &&
		(expr_type != JAM_INTEGER_EXPR) &&
		(expr_type != JAM_INT_OR_BOOL_EXPR))
	{
		status = JAMC_TYPE_MISMATCH;
	}

	/*
	*	Look for array variable with sub-range index
	*/
	if (status == JAMC_SUCCESS)
	{
		index = delimiter + 1;
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		expr_begin += index;
		expr_end += index;
		delimiter += index;
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_get_array_argument(&statement_buffer[expr_begin],
			&symbol_record, &literal_array_data,
			&start_index, &stop_index, 0);
		statement_buffer[expr_end] = save_ch;
	}

	if ((status == JAMC_SUCCESS) &&
		(literal_array_data != NULL) &&
		(start_index == 0) &&
		(stop_index > count_value - 1))
	{
		stop_index = count_value - 1;
	}

	if (status == JAMC_SUCCESS)
	{
		if (symbol_record != NULL)
		{
			heap_record = (JAMS_HEAP_RECORD *)symbol_record->value;

			if (heap_record != NULL)
			{
				tdi_data = heap_record->data;
			}
			else
			{
				status = JAMC_INTERNAL_ERROR;
			}
		}
		else if (literal_array_data != NULL)
		{
			tdi_data = literal_array_data;
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] == JAMC_SEMICOLON_CHAR))
	{
		/*
		*	Do a simple DRSCAN operation -- no capture or compare
		*/
		status = jam_do_drscan(count_value, tdi_data, start_index);
	}
	else if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] == JAMC_COMMA_CHAR))
	{
		/*
		*	Delimiter was a COMMA, so look for CAPTURE or COMPARE keyword
		*/
		index = delimiter + 1;
		while (jam_isspace(statement_buffer[index]))
		{
			++index;	/* skip over white space */
		}

		if ((jam_strncmp(&statement_buffer[index], "CAPTURE", 7) == 0) &&
			(jam_isspace(statement_buffer[index + 7])))
		{
			/*
			*	Do a DRSCAN with capture
			*/
			status = jam_process_drscan_capture(&statement_buffer[index + 8],
				count_value, tdi_data, start_index);
		}
		else if ((jam_strncmp(&statement_buffer[index], "COMPARE", 7) == 0) &&
			(jam_isspace(statement_buffer[index + 7])))
		{
			/*
			*	Do a DRSCAN with compare
			*/
			status = jam_process_drscan_compare(&statement_buffer[index + 8],
				count_value, tdi_data, start_index);
		}
		else
		{
			status = JAMC_SYNTAX_ERROR;
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_drstop(char *statement_buffer)

/*																			*/
/*	Description:	Sets stop-state for DR scan operations					*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;
	JAME_JTAG_STATE state = JAM_ILLEGAL_JTAG_STATE;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Get next argument
	*/
	status = jam_find_argument(&statement_buffer[index],
		&expr_begin, &expr_end, &delimiter);

	expr_begin += index;
	expr_end += index;
	delimiter += index;

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		state = jam_get_jtag_state_from_name(&statement_buffer[expr_begin]);

		if (state == JAM_ILLEGAL_JTAG_STATE)
		{
			status = JAMC_SYNTAX_ERROR;
		}
		else
		{
			/*
			*	Set DRSCAN stop state to the specified state
			*/
			status = jam_set_drstop_state(state);
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_exit
(
	char *statement_buffer,
	BOOL *done,
	int *exit_code
)

/*																			*/
/*	Description:	This function terminates an JAM program.  The 'done'	*/
/*					flag is set, halting program execution, and the			*/
/*					exit_code value is set as specified in the statement.	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	char save_ch = 0;
	long exit_code_value = 0L;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Evaluate expression for exit code value
	*/
	expr_begin = index;
	while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;
	}
	while ((statement_buffer[index] != JAMC_SEMICOLON_CHAR) && (index > 0))
	{
		--index;
	}
	expr_end = index;

	if (expr_end > expr_begin)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_evaluate_expression(
			&statement_buffer[expr_begin], &exit_code_value, &expr_type);
		statement_buffer[expr_end] = save_ch;
	}

	/*
	*	Check for integer expression
	*/
	if ((status == JAMC_SUCCESS) &&
		(expr_type != JAM_INTEGER_EXPR) &&
		(expr_type != JAM_INT_OR_BOOL_EXPR))
	{
		status = JAMC_TYPE_MISMATCH;
	}

	/*
	*	Check range of exit code -- must be in range of signed 16-bit number
	*	(from -32767 to 32767) for compatibility with 16-bit systems.
	*/
	if ((status == JAMC_SUCCESS) &&
		(((exit_code_value < -32767L)) || (exit_code_value > 32767L)))
	{
		status = JAMC_INTEGER_OVERFLOW;
	}

	if (status == JAMC_SUCCESS)
	{
		/*
		*	Terminate the program
		*/
		*done = TRUE;
		*exit_code = (int) exit_code_value;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_export
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Exports data outside the JAM interpreter (to the		*/
/*					calling program)										*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int key_begin = 0;
	int key_end = 0;
	int expr_begin = 0;
	int expr_end = 0;
	long value = 0L;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Find key string
	*/
	key_begin = index;
	while (jam_isspace(statement_buffer[index]) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;
	}

	/* the first argument must be a quoted string */
	if (statement_buffer[index] == JAMC_QUOTE_CHAR)
	{
		++index;	/* step over quote char */
		key_begin = index;

		/* find matching quote */
		while ((statement_buffer[index] != JAMC_QUOTE_CHAR) &&
			(statement_buffer[index] != JAMC_NULL_CHAR) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;
		}

		if (statement_buffer[index] == JAMC_QUOTE_CHAR)
		{
			key_end = index;
			++index;	/* step over quote char */

			while (jam_isspace(statement_buffer[index]) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;
			}

			if (statement_buffer[index] == JAMC_COMMA_CHAR)
			{
				++index;	/* step over comma */
				expr_begin = index;

				while ((statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
					(statement_buffer[index] != JAMC_NULL_CHAR) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;
				}

				if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
				{
					expr_end = index;
					statement_buffer[expr_end] = JAMC_NULL_CHAR;
					status = jam_evaluate_expression(
						&statement_buffer[expr_begin], &value, &expr_type);

					/*
					*	May be integer or Boolean expression
					*/
					if ((status == JAMC_SUCCESS) &&
						(expr_type != JAM_INTEGER_EXPR) &&
						(expr_type != JAM_BOOLEAN_EXPR) &&
						(expr_type != JAM_INT_OR_BOOL_EXPR))
					{
						status = JAMC_TYPE_MISMATCH;
					}

					if (status == JAMC_SUCCESS)
					{
						/*
						*	Export the key and value
						*/
						statement_buffer[key_end] = JAMC_NULL_CHAR;
						jam_export(&statement_buffer[key_begin], value);
					}
				}
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_for
(
	char *statement_buffer
)

/*																			*/
/*	Description:	This function processes a FOR statement.  It creates a	*/
/*					stack record and assigns the start value to the			*/
/*					iterator variable										*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int variable_begin = 0;
	int variable_end = 0;
	int expr_begin = 0;
	int expr_end = 0;
	long start_value = 0L;
	long stop_value = 0L;
	long step_value = 1L;
	char save_ch = 0;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;

	index = jam_skip_instruction_name(statement_buffer);

	if (jam_isalpha(statement_buffer[index]))
	{
		/* locate variable name */
		variable_begin = index;
		while ((jam_is_name_char(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over variable name */
		}
		variable_end = index;

		while ((jam_isspace(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over white space */
		}

		if (statement_buffer[index] == JAMC_EQUAL_CHAR)
		{
			/*
			*	Get start value for loop
			*/
			expr_begin = index + 1;

			expr_end = jam_find_keyword(&statement_buffer[expr_begin], "TO");

			if (expr_end > 0)
			{
				expr_end += expr_begin;
				save_ch = statement_buffer[expr_end];
				statement_buffer[expr_end] = JAMC_NULL_CHAR;
				status = jam_evaluate_expression(
					&statement_buffer[expr_begin], &start_value, &expr_type);
				statement_buffer[expr_end] = save_ch;
				index = expr_end + 2;	/* step over "TO" */
			}

			/*
			*	Check for integer expression
			*/
			if ((status == JAMC_SUCCESS) &&
				(expr_type != JAM_INTEGER_EXPR) &&
				(expr_type != JAM_INT_OR_BOOL_EXPR))
			{
				status = JAMC_TYPE_MISMATCH;
			}

			if (status == JAMC_SUCCESS)
			{
				/*
				*	Get stop value for loop
				*/
				while ((jam_isspace(statement_buffer[index])) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;	/* skip over white space */
				}

				expr_begin = index;

				expr_end = jam_find_keyword(&statement_buffer[expr_begin],
					"STEP");

				status = JAMC_SYNTAX_ERROR;
				if (expr_end > 0)
				{
					/* STEP found */
					expr_end += expr_begin;
					save_ch = statement_buffer[expr_end];
					statement_buffer[expr_end] = JAMC_NULL_CHAR;
					status = jam_evaluate_expression(
						&statement_buffer[expr_begin], &stop_value, &expr_type);
					statement_buffer[expr_end] = save_ch;
					index = expr_end + 4;	/* step over "STEP" */

					/*
					*	Check for integer expression
					*/
					if ((status == JAMC_SUCCESS) &&
						(expr_type != JAM_INTEGER_EXPR) &&
						(expr_type != JAM_INT_OR_BOOL_EXPR))
					{
						status = JAMC_TYPE_MISMATCH;
					}

					if (status == JAMC_SUCCESS)
					{
						/*
						*	Get step value
						*/
						while ((jam_isspace(statement_buffer[index])) &&
							(index < JAMC_MAX_STATEMENT_LENGTH))
						{
							++index;	/* skip over white space */
						}

						expr_begin = index;
						expr_end = 0;
						while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
							(statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
							(index < JAMC_MAX_STATEMENT_LENGTH))
						{
							++index;
						}

						if ((statement_buffer[index] == JAMC_SEMICOLON_CHAR))
						{
							expr_end = index;
						}

						status = JAMC_SYNTAX_ERROR;
						if (expr_end > expr_begin)
						{
							save_ch = statement_buffer[expr_end];
							statement_buffer[expr_end] = JAMC_NULL_CHAR;
							status = jam_evaluate_expression(
								&statement_buffer[expr_begin],
								&step_value, &expr_type);
							statement_buffer[expr_end] = save_ch;
						}

						/* step value zero is illegal */
						if ((status == JAMC_SUCCESS) && (step_value == 0))
						{
							status = JAMC_SYNTAX_ERROR;
						}

						/*
						*	Check for integer expression
						*/
						if ((status == JAMC_SUCCESS) &&
							(expr_type != JAM_INTEGER_EXPR) &&
							(expr_type != JAM_INT_OR_BOOL_EXPR))
						{
							status = JAMC_TYPE_MISMATCH;
						}
					}
				}
				else
				{
					/* STEP not found -- look for semicolon */
					while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
						(statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
						(index < JAMC_MAX_STATEMENT_LENGTH))
					{
						++index;
					}

					if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
					{
						expr_end = index;
					}

					/*
					*	Get stop value for loop
					*/
					status = JAMC_SYNTAX_ERROR;
					if (expr_end > expr_begin)
					{
						save_ch = statement_buffer[expr_end];
						statement_buffer[expr_end] = JAMC_NULL_CHAR;
						status = jam_evaluate_expression(
							&statement_buffer[expr_begin], &stop_value,
							&expr_type);
						statement_buffer[expr_end] = save_ch;
					}

					/*
					*	Step value defaults to one
					*/
					step_value = 1L;

					/*
					*	Check for integer expression
					*/
					if ((status == JAMC_SUCCESS) &&
						(expr_type != JAM_INTEGER_EXPR) &&
						(expr_type != JAM_INT_OR_BOOL_EXPR))
					{
						status = JAMC_TYPE_MISMATCH;
					}
				}
			}
		}
	}

	/*
	*	We have extracted the variable name and the start, stop, and
	*	step values from the statement buffer.  Now set the variable
	*	to the start value and push a stack record onto the stack.
	*/
	if (status == JAMC_SUCCESS)
	{
		/*
		*	Find the variable (must be an integer)
		*/
		status = JAMC_SYNTAX_ERROR;
		save_ch = statement_buffer[variable_end];
		statement_buffer[variable_end] = JAMC_NULL_CHAR;
		symbol_record = jam_get_symbol_record(
			&statement_buffer[variable_begin]);

		if ((symbol_record != NULL) &&
			(symbol_record->type == JAM_INTEGER_SYMBOL))
		{
			/*
			*	Set the variable to the start value
			*/
			status = jam_set_symbol_value(
				JAM_INTEGER_SYMBOL,
				&statement_buffer[variable_begin],
				start_value);
		}
		statement_buffer[variable_end] = save_ch;
	}

	if (status == JAMC_SUCCESS)
	{
		/*
		*	Push a record onto the stack
		*/
		status = jam_push_fornext_record(symbol_record,
			jam_next_statement_position, stop_value, step_value);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_if
(
	char *statement_buffer,
	BOOL *reuse_statement_buffer
)

/*																			*/
/*	Description:	Processes an IF (conditional) statement.  If the		*/
/*					condition is true, then the input stream pointer is		*/
/*					set to the position of the statement to be executed		*/
/*					(whatever follows the THEN keyword) which will be		*/
/*					fetched normally and processed as the next statement.	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	long conditional_value = 0L;
	int then_index = 0L;
	char save_ch = 0;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Evaluate conditional expression
	*/
	expr_begin = index;
	then_index = jam_find_keyword(&statement_buffer[expr_begin], "THEN");

	if (then_index > 0)
	{
		expr_end = expr_begin + then_index;

		if (expr_end > expr_begin)
		{
			save_ch = statement_buffer[expr_end];
			statement_buffer[expr_end] = JAMC_NULL_CHAR;
			status = jam_evaluate_expression(
				&statement_buffer[expr_begin], &conditional_value, &expr_type);
			statement_buffer[expr_end] = save_ch;
		}

		/*
		*	Check for Boolean expression
		*/
		if ((status == JAMC_SUCCESS) &&
			(expr_type != JAM_BOOLEAN_EXPR) &&
			(expr_type != JAM_INT_OR_BOOL_EXPR))
		{
			status = JAMC_TYPE_MISMATCH;
		}

		if (status == JAMC_SUCCESS)
		{
			if (conditional_value)
			{
				index = expr_end + 4;
				while ((jam_isspace(statement_buffer[index])) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;	/* skip over white space */
				}

				/*
				*	Copy whatever appears after "THEN" to beginning of buffer
				*	so it can be reused.
				*/
				jam_strcpy(statement_buffer, &statement_buffer[index]);
				*reuse_statement_buffer = TRUE;
			}
			/*
			*	(else do nothing if conditional value is false)
			*/
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_integer
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes a INTEGER variable declaration statement		*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int variable_begin = 0;
	int variable_end = 0;
	int dim_begin = 0;
	int dim_end = 0;
	int expr_begin = 0;
	int expr_end = 0;
	long dim_value = 0L;
	long init_value = 0L;
	char save_ch = 0;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	if (jam_isalpha(statement_buffer[index]))
	{
		/* locate variable name */
		variable_begin = index;
		while ((jam_is_name_char(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over variable name */
		}
		variable_end = index;

		while ((jam_isspace(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over white space */
		}

		if (statement_buffer[index] == JAMC_LBRACKET_CHAR)
		{
			/*
			*	Array declaration
			*/
			dim_begin = index + 1;
			while ((statement_buffer[index] != JAMC_RBRACKET_CHAR) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;	/* find matching bracket */
			}
			if (statement_buffer[index] == JAMC_RBRACKET_CHAR)
			{
				dim_end = index;
				++index;
			}
			while ((jam_isspace(statement_buffer[index])) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;	/* skip over white space */
			}

			if (dim_end > dim_begin)
			{
				save_ch = statement_buffer[dim_end];
				statement_buffer[dim_end] = JAMC_NULL_CHAR;
				status = jam_evaluate_expression(
					&statement_buffer[dim_begin], &dim_value, &expr_type);
				statement_buffer[dim_end] = save_ch;
			}

			/*
			*	Check for integer expression
			*/
			if ((status == JAMC_SUCCESS) &&
				(expr_type != JAM_INTEGER_EXPR) &&
				(expr_type != JAM_INT_OR_BOOL_EXPR))
			{
				status = JAMC_TYPE_MISMATCH;
			}

			if (status == JAMC_SUCCESS)
			{
				/*
				*	Add the array name to the symbol table
				*/
				save_ch = statement_buffer[variable_end];
				statement_buffer[variable_end] = JAMC_NULL_CHAR;
				status = jam_add_symbol(JAM_INTEGER_ARRAY_WRITABLE,
					&statement_buffer[variable_begin], 0L,
					jam_current_statement_position);

				/* get a pointer to the symbol record */
				if (status == JAMC_SUCCESS)
				{
					symbol_record = jam_get_symbol_record(
						&statement_buffer[variable_begin]);
					if (symbol_record == NULL) status = JAMC_INTERNAL_ERROR;
				}
				statement_buffer[variable_end] = save_ch;
			}

			if ((status == JAMC_SUCCESS) && (symbol_record != NULL))
			{
				if (statement_buffer[index] == JAMC_EQUAL_CHAR)
				{
					/*
					*	Array has initialization data:  read it in.
					*/
					status = jam_add_heap_record(symbol_record, &heap_record,
						dim_value);

					if (status == JAMC_SUCCESS)
					{
						symbol_record->value = (long) heap_record;

						status = jam_read_integer_array_data(heap_record,
							&statement_buffer[index + 1]);
					}
				}
				else if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
				{
					/*
					*	Array has no initialization data.
					*	Allocate a buffer on the heap:
					*/
					status = jam_add_heap_record(symbol_record, &heap_record,
						dim_value);

					if (status == JAMC_SUCCESS)
					{
						symbol_record->value = (long) heap_record;
					}
				}
			}
			else
			{
				/* this should be end of the statement */
				status = JAMC_SYNTAX_ERROR;
			}
		}
		else
		{
			/*
			*	Scalar variable declaration
			*/
			if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
			{
				status = JAMC_SUCCESS;
			}
			else if (statement_buffer[index] == JAMC_EQUAL_CHAR)
			{
				/*
				*	Evaluate initialization expression
				*/
				expr_begin = index + 1;
				while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;
				}
				while ((statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
					(index > 0))
				{
					--index;
				}
				expr_end = index;

				if (expr_end > expr_begin)
				{
					save_ch = statement_buffer[expr_end];
					statement_buffer[expr_end] = JAMC_NULL_CHAR;
					status = jam_evaluate_expression(
						&statement_buffer[expr_begin], &init_value, &expr_type);
					statement_buffer[expr_end] = save_ch;
				}

				/*
				*	Check for integer expression
				*/
				if ((status == JAMC_SUCCESS) &&
					(expr_type != JAM_INTEGER_EXPR) &&
					(expr_type != JAM_INT_OR_BOOL_EXPR))
				{
					status = JAMC_TYPE_MISMATCH;
				}
			}

			if (status == JAMC_SUCCESS)
			{
				/*
				*	Add the variable name to the symbol table
				*/
				save_ch = statement_buffer[variable_end];
				statement_buffer[variable_end] = JAMC_NULL_CHAR;
				status = jam_add_symbol(JAM_INTEGER_SYMBOL,
					&statement_buffer[variable_begin],
					init_value, jam_current_statement_position);
				statement_buffer[variable_end] = save_ch;
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_irscan
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes IRSCAN statement, which shifts data through	*/
/*					an instruction register of the JTAG interface.  The		*/
/*					syntax for the IRSCAN statement is:						*/
/*																			*/
/*					IRSCAN <length>, <data>;								*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	/* syntax:  IRSCAN <length>, <data>; */

	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0L;
	long count_value = 0L;
	long start_index = 0L;
	long stop_index = 0L;
	char save_ch = 0;
	long *tdi_data = NULL;
	long *literal_array_data = NULL;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	/* locate length */
	status = jam_find_argument(&statement_buffer[index],
		&expr_begin, &expr_end, &delimiter);

	expr_begin += index;
	expr_end += index;
	delimiter += index;

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_evaluate_expression(
			&statement_buffer[expr_begin], &count_value, &expr_type);
		statement_buffer[expr_end] = save_ch;
	}

	/*
	*	Check for integer expression
	*/
	if ((status == JAMC_SUCCESS) &&
		(expr_type != JAM_INTEGER_EXPR) &&
		(expr_type != JAM_INT_OR_BOOL_EXPR))
	{
		status = JAMC_TYPE_MISMATCH;
	}

	/*
	*	Look for array variable with sub-range index
	*/
	if (status == JAMC_SUCCESS)
	{
		index = delimiter + 1;
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		expr_begin += index;
		expr_end += index;
		delimiter += index;
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_get_array_argument(&statement_buffer[expr_begin],
			&symbol_record, &literal_array_data,
			&start_index, &stop_index, 0);
		statement_buffer[expr_end] = save_ch;
	}

	if ((status == JAMC_SUCCESS) &&
		(literal_array_data != NULL) &&
		(start_index == 0) &&
		(stop_index > count_value - 1))
	{
		stop_index = count_value - 1;
	}

	if (status == JAMC_SUCCESS)
	{
		if (symbol_record != NULL)
		{
			heap_record = (JAMS_HEAP_RECORD *)symbol_record->value;

			if (heap_record != NULL)
			{
				tdi_data = heap_record->data;
			}
			else
			{
				status = JAMC_INTERNAL_ERROR;
			}
		}
		else if (literal_array_data != NULL)
		{
			tdi_data = literal_array_data;
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	if ((status == JAMC_SUCCESS) &&
		(stop_index != start_index + count_value - 1))
	{
		status = JAMC_BOUNDS_ERROR;
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		/*
		*	Do the IRSCAN operation
		*/
		status = jam_do_irscan(count_value, tdi_data, start_index);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_irstop
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Sets stop-state for IR scan operations					*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;
	JAME_JTAG_STATE state = JAM_ILLEGAL_JTAG_STATE;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Get next argument
	*/
	status = jam_find_argument(&statement_buffer[index],
		&expr_begin, &expr_end, &delimiter);

	expr_begin += index;
	expr_end += index;
	delimiter += index;

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		state = jam_get_jtag_state_from_name(&statement_buffer[expr_begin]);

		if (state == JAM_ILLEGAL_JTAG_STATE)
		{
			status = JAMC_SYNTAX_ERROR;
		}
		else
		{
			/*
			*	Set IRSCAN stop state to the specified state
			*/
			status = jam_set_irstop_state(state);
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_copy_array_subrange
(
	long *source_heap_data,
	long source_subrange_begin,
	long source_subrange_end,
	long *dest_heap_data,
	long dest_subrange_begin,
	long dest_subrange_end
)

/*																			*/
/*	Description:	Copies bits from one BOOLEAN array buffer to another	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	long source_length = 1 + source_subrange_end - source_subrange_begin;
	long dest_length = 1 + dest_subrange_end - dest_subrange_begin;
	long length = source_length;
	long index = 0L;
	long source_index = 0L;
	long dest_index = 0L;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	/* find minimum of source_length and dest_length */
	if (length > dest_length) length = dest_length;

	if (length <= 0L)
	{
		status = JAMC_BOUNDS_ERROR;
	}
	else
	{
		/* copy the bits */
		for (index = 0L; index < length; ++index)
		{
			source_index = index + source_subrange_begin;
			dest_index = index + dest_subrange_begin;

			if (source_heap_data[source_index >> 5] &
				(1L << (source_index & 0x1f)))
			{
				/* set a single bit */
				dest_heap_data[dest_index >> 5] |=
					(1L << (dest_index & 0x1f));
			}
			else
			{
				/* clear a single bit */
				dest_heap_data[dest_index >> 5] &=
					(~(unsigned long)(1L << (dest_index & 0x1f)));
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_let
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes a LET (assignment) statement.					*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int variable_begin = 0;
	int variable_end = 0;
	int dim_begin = 0;
	int dim_end = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int bracket_count = 0;
	long dim_value = 0L;
	long assign_value = 0L;
	char save_ch = 0;
	long source_subrange_begin = 0L;
	long source_subrange_end = 0L;
	long dest_subrange_begin = 0L;
	long dest_subrange_end = 0L;
	BOOL is_array = FALSE;
	BOOL full_array = FALSE;
	BOOL array_subrange = FALSE;
	long *source_heap_data = NULL;
	long *dest_heap_data = NULL;
	long *literal_array_data = NULL;
	JAME_EXPRESSION_TYPE assign_type = JAM_ILLEGAL_EXPR_TYPE;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	if (jam_isalpha(statement_buffer[index]))
	{
		/* locate variable name */
		variable_begin = index;
		while ((jam_is_name_char(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over variable name */
		}
		variable_end = index;

		while ((jam_isspace(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over white space */
		}

		status = JAMC_SUCCESS;

		if (statement_buffer[index] == JAMC_LBRACKET_CHAR)
		{
			/*
			*	Assignment to array element
			*/
			++index;
			is_array = TRUE;
			dim_begin = index;
			while ((jam_isspace(statement_buffer[dim_begin])) &&
				(dim_begin < JAMC_MAX_STATEMENT_LENGTH))
			{
				++dim_begin;	/* skip over white space */
			}
			while (((statement_buffer[index] != JAMC_RBRACKET_CHAR) ||
				(bracket_count > 0)) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				if (statement_buffer[index] == JAMC_LBRACKET_CHAR)
				{
					++bracket_count;
				}
				else if (statement_buffer[index] == JAMC_RBRACKET_CHAR)
				{
					--bracket_count;
				}

				++index;	/* find matching bracket */
			}
			if (statement_buffer[index] == JAMC_RBRACKET_CHAR)
			{
				dim_end = index;
			}

			if (dim_end == dim_begin)
			{
				/* full array notation */
				full_array = TRUE;
			}
			else if (dim_end > dim_begin)
			{
				/* look for ".." in array index expression */
				index = dim_begin;
				while ((index < dim_end) && !array_subrange)
				{
					if ((statement_buffer[index] == '.') &&
						(statement_buffer[index + 1] == '.'))
					{
						array_subrange = TRUE;
					}
					++index;
				}
			}
			else
			{
				/* right bracket not found */
				status = JAMC_SYNTAX_ERROR;
			}

			if (status == JAMC_SUCCESS)
			{
				index = dim_end + 1;
				while ((jam_isspace(statement_buffer[index])) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;	/* skip over white space */
				}

				/* get pointer to symbol record */
				save_ch = statement_buffer[variable_end];
				statement_buffer[variable_end] = JAMC_NULL_CHAR;
				symbol_record = jam_get_symbol_record(
					&statement_buffer[variable_begin]);
				statement_buffer[variable_end] = save_ch;

				/* check array type */
				if (symbol_record == NULL)
				{
					status = JAMC_UNDEFINED_SYMBOL;
				}
				else
				{
					switch (symbol_record->type)
					{
					case JAM_INTEGER_ARRAY_WRITABLE:
						assign_type = JAM_INTEGER_EXPR;
						break;

					case JAM_BOOLEAN_ARRAY_WRITABLE:
						assign_type = JAM_BOOLEAN_EXPR;
						break;

					case JAM_INTEGER_ARRAY_INITIALIZED:
					case JAM_BOOLEAN_ARRAY_INITIALIZED:
						status = JAMC_ASSIGN_TO_CONST;
						break;

					default:
						status = JAMC_TYPE_MISMATCH;
						break;
					}
				}

				/* get pointer to heap record */
				if (status == JAMC_SUCCESS)
				{
					heap_record = (JAMS_HEAP_RECORD *) symbol_record->value;

					if ((heap_record < jam_heap) || (((long)heap_record) >
						(((long)jam_workspace) + ((long)jam_workspace_size))))
					{
						status = JAMC_INTERNAL_ERROR;
					}
					else
					{
						dest_heap_data = heap_record->data;
					}
				}
			}

			if (status == JAMC_SUCCESS)
			{
				if (full_array || array_subrange)
				{
					if (assign_type == JAM_BOOLEAN_EXPR)
					{
						if (full_array)
						{
							dest_subrange_begin = 0L;
							dest_subrange_end = heap_record->dimension - 1L;
							array_subrange = TRUE;
						}
						else
						{
							save_ch = statement_buffer[dim_end];
							statement_buffer[dim_end] = JAMC_NULL_CHAR;
							status = jam_get_array_subrange(
								&statement_buffer[dim_begin],
								&dest_subrange_begin, &dest_subrange_end);
							statement_buffer[dim_end] = save_ch;

							/* check array bounds */
							if ((status == JAMC_SUCCESS) &&
								((dest_subrange_begin < 0L) ||
								(dest_subrange_begin >= heap_record->dimension)
								|| (dest_subrange_end < 0L) ||
								(dest_subrange_end >= heap_record->dimension)))
							{
								status = JAMC_BOUNDS_ERROR;
							}
						}
					}
					else
					{
						/* can't assign to an integer array */
						status = JAMC_SYNTAX_ERROR;
					}
				}
				else
				{
					/* assign to array element */
					save_ch = statement_buffer[dim_end];
					statement_buffer[dim_end] = JAMC_NULL_CHAR;
					status = jam_evaluate_expression(
						&statement_buffer[dim_begin], &dim_value, &expr_type);
					statement_buffer[dim_end] = save_ch;

					/*
					*	Check for integer expression
					*/
					if ((status == JAMC_SUCCESS) &&
						(expr_type != JAM_INTEGER_EXPR) &&
						(expr_type != JAM_INT_OR_BOOL_EXPR))
					{
						status = JAMC_TYPE_MISMATCH;
					}
				}
			}
		}
		else
		{
			/*
			*	Get type of variable on left-hand-side
			*/
			save_ch = statement_buffer[variable_end];
			statement_buffer[variable_end] = JAMC_NULL_CHAR;
			symbol_record = jam_get_symbol_record(
				&statement_buffer[variable_begin]);
			statement_buffer[variable_end] = save_ch;

			if (symbol_record != NULL)
			{
				switch (symbol_record->type)
				{
				case JAM_INTEGER_SYMBOL:
					assign_type = JAM_INTEGER_EXPR;
					break;

				case JAM_BOOLEAN_SYMBOL:
					assign_type = JAM_BOOLEAN_EXPR;
					break;

				default:
					status = JAMC_TYPE_MISMATCH;
					break;
				}
			}
			else
			{
				status = JAMC_UNDEFINED_SYMBOL;
			}
		}

		/*
		*	Evaluate assignment expression
		*/
		if (status == JAMC_SUCCESS)
		{
			status = JAMC_SYNTAX_ERROR;

			if (statement_buffer[index] == JAMC_EQUAL_CHAR)
			{
				/*
				*	Evaluate assignment expression
				*/
				expr_begin = index + 1;
				while ((jam_isspace(statement_buffer[expr_begin])) &&
					(expr_begin < JAMC_MAX_STATEMENT_LENGTH))
				{
					++expr_begin;	/* skip over white space */
				}
				while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;
				}
				while ((statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
					(index > 0))
				{
					--index;
				}
				expr_end = index;

				if (expr_end > expr_begin)
				{
					if (array_subrange)
					{
						symbol_record = NULL;
						save_ch = statement_buffer[expr_end];
						statement_buffer[expr_end] = JAMC_NULL_CHAR;
						status = jam_get_array_argument(
							&statement_buffer[expr_begin],
							&symbol_record,
							&literal_array_data,
							&source_subrange_begin,
							&source_subrange_end, 0);
						statement_buffer[expr_end] = save_ch;

						if (status == JAMC_SUCCESS)
						{
							if (symbol_record != NULL)
							{
								if ((symbol_record->type ==
									JAM_BOOLEAN_ARRAY_WRITABLE) ||
									(symbol_record->type ==
									JAM_BOOLEAN_ARRAY_INITIALIZED))
								{
									heap_record = (JAMS_HEAP_RECORD *)
										symbol_record->value;

									/* check array bounds */
									if ((source_subrange_begin < 0L) ||
										(source_subrange_begin >=
											heap_record->dimension) ||
										(source_subrange_end < 0L) ||
										(source_subrange_end >=
											heap_record->dimension))
									{
										status = JAMC_BOUNDS_ERROR;
									}
									else
									{
										source_heap_data = heap_record->data;
									}
								}
								else
								{
									status = JAMC_TYPE_MISMATCH;
								}
							}
							else if (literal_array_data != NULL)
							{
								source_heap_data = literal_array_data;
							}
							else
							{
								status = JAMC_INTERNAL_ERROR;
							}
						}
					}
					else
					{
						save_ch = statement_buffer[expr_end];
						statement_buffer[expr_end] = JAMC_NULL_CHAR;
						status = jam_evaluate_expression(
							&statement_buffer[expr_begin],
							&assign_value, &expr_type);
						statement_buffer[expr_end] = save_ch;
					}
				}
			}
		}

		if (status == JAMC_SUCCESS)
		{
			/*
			*	Check type of expression against type of variable
			*	being assigned
			*/
			if (array_subrange)
			{
				/* copy array data */
				status = jam_copy_array_subrange(
					source_heap_data,
					source_subrange_begin,
					source_subrange_end,
					dest_heap_data,
					dest_subrange_begin,
					dest_subrange_end);
			}
			else if ((expr_type != JAM_ILLEGAL_EXPR_TYPE) &&
				(assign_type != JAM_ILLEGAL_EXPR_TYPE) &&
				((expr_type == assign_type) ||
				(expr_type == JAM_INT_OR_BOOL_EXPR)))
			{
				/*
				*	Set the variable to the computed value
				*/
				if (is_array)
				{
					/* check array bounds */
					if ((dim_value >= 0) &&
						(dim_value < heap_record->dimension))
					{
						if (assign_type == JAM_INTEGER_EXPR)
						{
							dest_heap_data[dim_value] = assign_value;
						}
						else if (assign_type == JAM_BOOLEAN_EXPR)
						{
							if (assign_value == 0)
							{
								/* clear a single bit */
								dest_heap_data[dim_value >> 5] &=
									(~(unsigned long)(1L << (dim_value & 0x1f)));
							}
							else
							{
								/* set a single bit */
								dest_heap_data[dim_value >> 5] |=
									(1L << (dim_value & 0x1f));
							}
						}
						else
						{
							status = JAMC_INTERNAL_ERROR;
						}
					}
					else
					{
						status = JAMC_BOUNDS_ERROR;
					}
				}
				else
				{
					symbol_record->value = assign_value;
				}
			}
			else
			{
				status = JAMC_TYPE_MISMATCH;
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_next
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes a NEXT statement.  The NEXT statement is		*/
/*					used to mark the bottom of a FOR loop.  When a NEXT		*/
/*					statement is processed, there must be a corresponding	*/
/*					FOR loop stack record on top of the stack.				*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int variable_begin = 0;
	int variable_end = 0;
	char save_ch = 0;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_STACK_RECORD *stack_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	if (jam_isalpha(statement_buffer[index]))
	{
		/* locate variable name */
		variable_begin = index;
		while ((jam_is_name_char(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over variable name */
		}
		variable_end = index;

		while ((jam_isspace(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over white space */
		}

		if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
		{
			/*
			*	Look in symbol table for iterator variable
			*/
			save_ch = statement_buffer[variable_end];
			statement_buffer[variable_end] = JAMC_NULL_CHAR;
			symbol_record = jam_get_symbol_record(
				&statement_buffer[variable_begin]);
			statement_buffer[variable_end] = save_ch;

			/*
			*	Get stack record at top of stack
			*/
			stack_record = jam_peek_stack_record();

			/*
			*	Compare iterator to stack record
			*/
			if ((symbol_record == NULL) ||
				(symbol_record->type != JAM_INTEGER_SYMBOL))
			{
				status = JAMC_TYPE_MISMATCH;
			}
			else if ((stack_record == NULL) ||
				(stack_record->type != JAM_STACK_FOR_NEXT) ||
				(stack_record->iterator != symbol_record))
			{
				status = JAMC_NEXT_UNEXPECTED;
			}
			else
			{
				/*
				*	Check if loop has run to completion
				*/
				if (((stack_record->step_value > 0) &&
					(symbol_record->value >= stack_record->stop_value)) ||
					((stack_record->step_value < 0) &&
					(symbol_record->value <= stack_record->stop_value)))
				{
					/*
					*	Loop has run to completion -- pop the stack record.
					*	(Do not jump back to FOR statement position.)
					*/
					status = jam_pop_stack_record();
				}
				else
				{
					/*
					*	Increment (or step) the iterator variable
					*/
					symbol_record->value += stack_record->step_value;

					/*
					*	Jump back to the top of the loop
					*/
					if (jam_seek(stack_record->for_position) == 0)
					{
						jam_current_file_position =
							stack_record->for_position;
						status = JAMC_SUCCESS;
					}
					else
					{
						status = JAMC_IO_ERROR;
					}
				}
			}
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_padding
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes a PADDING statement.  This sets the number	*/
/*					of padding bits to be used before and after the data	*/
/*					indicated for each DRSCAN and IRSCAN operation.			*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int argc = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	char save_ch = 0;
	long padding[4] = {0};
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	index = jam_skip_instruction_name(statement_buffer);

	for (argc = 0; (argc < 4) && (status == JAMC_SUCCESS); ++argc)
	{
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		if (status == JAMC_SUCCESS)
		{
			padding[argc] = -1L;

			expr_begin += index;
			expr_end += index;
			delimiter += index;

			if (((argc < 3) &&
				(statement_buffer[delimiter] == JAMC_COMMA_CHAR)) ||
				((argc == 3) &&
				(statement_buffer[delimiter] == JAMC_SEMICOLON_CHAR)))
			{
				save_ch = statement_buffer[expr_end];
				statement_buffer[expr_end] = JAMC_NULL_CHAR;
				status = jam_evaluate_expression(
					&statement_buffer[expr_begin], &padding[argc], &expr_type);
				statement_buffer[expr_end] = save_ch;

				/*
				*	Check for integer expression
				*/
				if ((status == JAMC_SUCCESS) &&
					(expr_type != JAM_INTEGER_EXPR) &&
					(expr_type != JAM_INT_OR_BOOL_EXPR))
				{
					status = JAMC_TYPE_MISMATCH;
				}

				/*
				*	Check the range -- padding value must be between 0 and 1000
				*/
				if ((status == JAMC_SUCCESS) &&
					((padding[argc] < 0L) || (padding[argc] > 1000L)))
				{
					status = JAMC_SYNTAX_ERROR;
				}
				else
				{
					index = expr_end + 1;
				}
			}
			else
			{
				status = JAMC_SYNTAX_ERROR;
			}
		}
	}

	/*
	*	Store the new padding values
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_set_jtag_padding(
			(int) padding[0],	/* dr_preamble  */
			(int) padding[1],	/* dr_postamble */
			(int) padding[2],	/* ir_preamble  */
			(int) padding[3]);	/* ir_postamble */
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_pop
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Pops a data element (integer or Boolean) from the		*/
/*					internal stack.  The data value is stored in the		*/
/*					variable specified.  If the type of data is not			*/
/*					compatible with the variable type, a type mismatch		*/
/*					error results.											*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	long push_value = 0L;
	long dim_value = 0L;
	int variable_begin = 0;
	int variable_end = 0;
	int dim_begin = 0;
	int dim_end = 0;
	int bracket_count = 0;
	char save_ch = 0;
	BOOL is_array = FALSE;
	long *heap_data = NULL;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_STACK_RECORD *stack_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAME_EXPRESSION_TYPE assign_type = JAM_ILLEGAL_EXPR_TYPE;
	JAME_EXPRESSION_TYPE value_type = JAM_ILLEGAL_EXPR_TYPE;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Get the variable name
	*/
	if (jam_isalpha(statement_buffer[index]))
	{
		variable_begin = index;
		while ((jam_is_name_char(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over variable name */
		}
		variable_end = index;

		while ((jam_isspace(statement_buffer[index])) &&
			(index < JAMC_MAX_STATEMENT_LENGTH))
		{
			++index;	/* skip over white space */
		}

		if (statement_buffer[index] == JAMC_LBRACKET_CHAR)
		{
			/*
			*	Pop value into array element
			*/
			++index;
			is_array = TRUE;
			dim_begin = index;
			while (((statement_buffer[index] != JAMC_RBRACKET_CHAR) ||
				(bracket_count > 0)) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				if (statement_buffer[index] == JAMC_LBRACKET_CHAR)
				{
					++bracket_count;
				}
				else if (statement_buffer[index] == JAMC_RBRACKET_CHAR)
				{
					--bracket_count;
				}

				++index;	/* find matching bracket */
			}
			if (statement_buffer[index] == JAMC_RBRACKET_CHAR)
			{
				dim_end = index;
				++index;
			}
			while ((jam_isspace(statement_buffer[index])) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;	/* skip over white space */
			}

			if (dim_end > dim_begin)
			{
				save_ch = statement_buffer[dim_end];
				statement_buffer[dim_end] = JAMC_NULL_CHAR;
				status = jam_evaluate_expression(
					&statement_buffer[dim_begin], &dim_value, &expr_type);
				statement_buffer[dim_end] = save_ch;
			}

			/*
			*	Check for integer expression
			*/
			if ((status == JAMC_SUCCESS) &&
				(expr_type != JAM_INTEGER_EXPR) &&
				(expr_type != JAM_INT_OR_BOOL_EXPR))
			{
				status = JAMC_TYPE_MISMATCH;
			}

			if (status == JAMC_SUCCESS)
			{
				/* get pointer to symbol record */
				save_ch = statement_buffer[variable_end];
				statement_buffer[variable_end] = JAMC_NULL_CHAR;
				symbol_record = jam_get_symbol_record(
					&statement_buffer[variable_begin]);
				statement_buffer[variable_end] = save_ch;

				/* check array type */
				if (symbol_record == NULL)
				{
					status = JAMC_UNDEFINED_SYMBOL;
				}
				else
				{
					switch (symbol_record->type)
					{
					case JAM_INTEGER_ARRAY_WRITABLE:
						assign_type = JAM_INTEGER_EXPR;
						break;

					case JAM_BOOLEAN_ARRAY_WRITABLE:
						assign_type = JAM_BOOLEAN_EXPR;
						break;

					case JAM_INTEGER_ARRAY_INITIALIZED:
					case JAM_BOOLEAN_ARRAY_INITIALIZED:
						status = JAMC_ASSIGN_TO_CONST;
						break;

					default:
						status = JAMC_TYPE_MISMATCH;
						break;
					}
				}

				/* get pointer to heap record */
				if (status == JAMC_SUCCESS)
				{
					heap_record = (JAMS_HEAP_RECORD *) symbol_record->value;

					if ((heap_record < jam_heap) || (((long)heap_record) >
						(((long)jam_workspace) + ((long)jam_workspace_size))))
					{
						status = JAMC_INTERNAL_ERROR;
					}
					else
					{
						heap_data = &heap_record->data[0];
					}
				}
			}
		}
		else
		{
			/*
			*	Poping value into scalar (not array) variable
			*/
			if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
			{
				status = JAMC_SUCCESS;

				/*
				*	Get variable type
				*/
				save_ch = statement_buffer[variable_end];
				statement_buffer[variable_end] = JAMC_NULL_CHAR;
				symbol_record = jam_get_symbol_record(
					&statement_buffer[variable_begin]);
				statement_buffer[variable_end] = save_ch;

				if (symbol_record != NULL)
				{
					switch (symbol_record->type)
					{
					case JAM_INTEGER_SYMBOL:
						assign_type = JAM_INTEGER_EXPR;
						break;

					case JAM_BOOLEAN_SYMBOL:
						assign_type = JAM_BOOLEAN_EXPR;
						break;

					default:
						status = JAMC_TYPE_MISMATCH;
						break;
					}
				}
				else
				{
					status = JAMC_UNDEFINED_SYMBOL;
				}
			}
			else
			{
				status = JAMC_SYNTAX_ERROR;
			}
		}

		/*
		*	Get stack record at top of stack
		*/
		stack_record = jam_peek_stack_record();

		/*
		*	Check that stack record corresponds to a PUSH statement
		*/
		if ((stack_record != NULL) &&
			(stack_record->type == JAM_STACK_PUSH_POP))
		{
			/*
			*	Stack record is the correct type -- pop it off the stack.
			*/
			push_value = stack_record->push_value;
			status = jam_pop_stack_record();

			/*
			*	Now set the variable to the push value
			*/
			if (status == JAMC_SUCCESS)
			{
				/*
				*	Check type of expression against type of variable
				*	being assigned
				*/
				if ((push_value == 0) || (push_value == 1))
				{
					value_type = JAM_INT_OR_BOOL_EXPR;
				}
				else value_type = JAM_INTEGER_EXPR;

				if ((assign_type != JAM_ILLEGAL_EXPR_TYPE) &&
					((value_type == assign_type) ||
					(value_type == JAM_INT_OR_BOOL_EXPR)))
				{
					/*
					*	Set the variable to the computed value
					*/
					if (is_array)
					{
						if (assign_type == JAM_INTEGER_EXPR)
						{
							heap_data[dim_value] = push_value;
						}
						else if (assign_type == JAM_BOOLEAN_EXPR)
						{
							if (push_value == 0)
							{
								/* clear a single bit */
								heap_data[dim_value >> 5] &=
									(~(unsigned long)(1L << (dim_value & 0x1f)));
							}
							else
							{
								/* set a single bit */
								heap_data[dim_value >> 5] |=
									(1L << (dim_value & 0x1f));
							}
						}
						else status = JAMC_INTERNAL_ERROR;
					}
					else
					{
						symbol_record->value = push_value;
					}
				}
				else
				{
					status = JAMC_TYPE_MISMATCH;
				}
			}
		}
		else
		{
			/*
			*	Top of stack did not have a PUSH/POP record
			*/
			status = JAMC_POP_UNEXPECTED;
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_print
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes a PRINT statement.  Only constant literal		*/
/*					strings and INTEGER or BOOLEAN expressions are			*/
/*					supported.												*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int dest_index = 0;
	char text_buffer[JAMC_MAX_STATEMENT_LENGTH + 1];
	int expr_begin = 0;
	int expr_end = 0;
	char save_ch = 0;
	long expr_value = 0L;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	index = jam_skip_instruction_name(statement_buffer);

	while ((statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
		(status == JAMC_SUCCESS) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		/*
		*	Get arguments from statement and concatenate onto string
		*/
		if (statement_buffer[index] == JAMC_QUOTE_CHAR)
		{
			/*
			*	Argument is explicit string - copy it
			*/
			++index;
			while ((statement_buffer[index] != JAMC_QUOTE_CHAR) &&
				(index < JAMC_MAX_STATEMENT_LENGTH) &&
				(dest_index < JAMC_MAX_STATEMENT_LENGTH))
			{
				text_buffer[dest_index++] = statement_buffer[index++];
			}
			text_buffer[dest_index] = '\0';

			if (statement_buffer[index] == JAMC_QUOTE_CHAR)
			{
				/* skip over terminating quote character */
				++index;
			}
			else
			{
				/* terminating quote character not found */
				status = JAMC_SYNTAX_ERROR;
			}
		}
		else if ((statement_buffer[index] == 'C') &&
			(statement_buffer[index + 1] == 'H') &&
			(statement_buffer[index + 2] == 'R') &&
			(statement_buffer[index + 3] == '$') &&
			(statement_buffer[index + 4] == JAMC_LPAREN_CHAR))
		{
			/*
			*	Convert integer expression to character code
			*/
			index += 4;	/* skip over CHR$, point to left parenthesis */
			expr_begin = index;
			expr_end = 0;

			while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
				(statement_buffer[index] != JAMC_COMMA_CHAR) &&
				(statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;
			}

			if ((statement_buffer[index] == JAMC_COMMA_CHAR) ||
				(statement_buffer[index] == JAMC_SEMICOLON_CHAR))
			{
				expr_end = index;
			}

			if (expr_end > expr_begin)
			{
				save_ch = statement_buffer[expr_end];
				statement_buffer[expr_end] = JAMC_NULL_CHAR;
				status = jam_evaluate_expression(
					&statement_buffer[expr_begin], &expr_value, NULL);
				statement_buffer[expr_end] = save_ch;
			}
			else
			{
				status = JAMC_SYNTAX_ERROR;
			}

			/*
			*	Allow any seven-bit character code (zero to 127)
			*/
			if ((status == JAMC_SUCCESS) &&
				((expr_value < 0) || (expr_value > 127)))
			{
				/* character code out of range */

				/* instead of flagging an error, force the value to 127 */
				expr_value = 127;
			}

			if ((status == JAMC_SUCCESS) &&
				(dest_index >= JAMC_MAX_STATEMENT_LENGTH))
			{
				/* no space in output buffer */
				status = JAMC_SYNTAX_ERROR;
			}

			if (status == JAMC_SUCCESS)
			{
				/*
				*	Put the character code directly into the output buffer
				*/
				text_buffer[dest_index++] = (char) expr_value;
				text_buffer[dest_index] = '\0';
			}
		}
		else
		{
			/*
			*	Process it as an integer expression
			*/
			expr_begin = index;
			expr_end = 0;
			status = JAMC_SYNTAX_ERROR;

			while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
				(statement_buffer[index] != JAMC_COMMA_CHAR) &&
				(statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;
			}

			if ((statement_buffer[index] == JAMC_COMMA_CHAR) ||
				(statement_buffer[index] == JAMC_SEMICOLON_CHAR))
			{
				expr_end = index;
			}

			if (expr_end > expr_begin)
			{
				save_ch = statement_buffer[expr_end];
				statement_buffer[expr_end] = JAMC_NULL_CHAR;
				status = jam_evaluate_expression(
					&statement_buffer[expr_begin], &expr_value, NULL);
				statement_buffer[expr_end] = save_ch;
			}

			if (status == JAMC_SUCCESS)
			{
				/*
				*	Convert integer and concatenate to output string
				*/
				jam_ltoa(&text_buffer[dest_index], expr_value);

				/* advance pointer to new end of string */
				while ((text_buffer[dest_index] != JAMC_NULL_CHAR) &&
					(dest_index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++dest_index;
				}
			}
		}

		if (status == JAMC_SUCCESS)
		{
			while ((jam_isspace(statement_buffer[index])) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;	/* skip over white space */
			}

			if (statement_buffer[index] == JAMC_COMMA_CHAR)
			{
				/*
				*	Skip over comma and white space to process next argument
				*/
				++index;
				while ((jam_isspace(statement_buffer[index])) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;	/* skip over white space */
				}
			}
			else if (statement_buffer[index] != JAMC_SEMICOLON_CHAR)
			{
				/*
				*	If no comma to seperate arguments, statement must be
				*	terminated by a semicolon
				*/
				status = JAMC_SYNTAX_ERROR;
			}
		}
	}

	if (status == JAMC_SUCCESS)
	{
		jam_message(text_buffer);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_push
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Pushed an integer or Boolean value onto the stack		*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	char save_ch = 0;
	long push_value = 0L;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Evaluate expression for the PUSH value
	*/
	expr_begin = index;
	while ((statement_buffer[index] != JAMC_NULL_CHAR) &&
		(index < JAMC_MAX_STATEMENT_LENGTH))
	{
		++index;
	}
	while ((statement_buffer[index] != JAMC_SEMICOLON_CHAR) && (index > 0))
	{
		--index;
	}
	expr_end = index;

	if (expr_end > expr_begin)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_evaluate_expression(
			&statement_buffer[expr_begin], &push_value, NULL);
		statement_buffer[expr_end] = save_ch;
	}

	/*
	*	Push the value onto the stack
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_push_pushpop_record(push_value);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_return
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Returns from subroutine by popping the return address	*/
/*					off the stack											*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	long return_position = 0L;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;
	JAMS_STACK_RECORD *stack_record = NULL;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	The semicolon must be next.
	*/
	if (statement_buffer[index] == JAMC_SEMICOLON_CHAR)
	{
		/*
		*	Get stack record at top of stack
		*/
		stack_record = jam_peek_stack_record();

		/*
		*	Check that stack record corresponds to a CALL statement
		*/
		if ((stack_record != NULL) &&
			(stack_record->type == JAM_STACK_CALL_RETURN))
		{
			/*
			*	Stack record is the correct type -- pop it off the stack.
			*/
			return_position = stack_record->return_position;
			status = jam_pop_stack_record();

			/*
			*	Now jump to the return address
			*/
			if (status == JAMC_SUCCESS)
			{
				if (jam_seek(return_position) == 0)
				{
					jam_current_file_position = return_position;
				}
				else
				{
					/* seek failed */
					status = JAMC_IO_ERROR;
				}
			}
		}
		else
		{
			status = JAMC_RETURN_UNEXPECTED;
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_state
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Forces JTAG chain to specified state, or through		*/
/*					specified sequence of states							*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	char save_ch = 0;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;
	JAME_JTAG_STATE state = JAM_ILLEGAL_JTAG_STATE;

	index = jam_skip_instruction_name(statement_buffer);

	do
	{
		/*
		*	Get next argument
		*/
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		if (status == JAMC_SUCCESS)
		{
			expr_begin += index;
			expr_end += index;
			delimiter += index;

			save_ch = statement_buffer[expr_end];
			statement_buffer[expr_end] = JAMC_NULL_CHAR;
			state = jam_get_jtag_state_from_name(&statement_buffer[expr_begin]);

			if (state == JAM_ILLEGAL_JTAG_STATE)
			{
				status = JAMC_SYNTAX_ERROR;
			}
			else
			{
				/*
				*	Go to the specified state
				*/
				status = jam_goto_jtag_state(state);
				index = delimiter + 1;
			}

			statement_buffer[expr_end] = save_ch;
		}
	}
	while ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] == JAMC_COMMA_CHAR));

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_vector
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Applies signals to non-JTAG hardware interface.  There	*/
/*					are three versions:  output only, compare, and capture.	*/
/*																			*/
/*					NOTE:  This function is not complete.  Need to add		*/
/*					support for CAPTURE and COMPARE syntax, and handle the	*/
/*					case of non-zero start indices.  Also, it is untested.	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	/* syntax:  VECTOR <dir>, <data>; */
	/* or:      VECTOR <dir>, <data>, CAPTURE <capture> ; */
	/* or:      VECTOR <dir>, <data>, COMPARE <expected>, <mask>, <result> ; */

	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	long dir_start_index = 0L;
	long dir_stop_index = 0L;
	long data_start_index = 0L;
	long data_stop_index = 0L;
	char save_ch = 0;
	long *dir_vector = NULL;
	long *data_vector = NULL;
	long *literal_array_data = NULL;
	JAMS_SYMBOL_RECORD *symbol_record = NULL;
	JAMS_HEAP_RECORD *heap_record = NULL;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	/*
	*	Get direction vector
	*/
	status = jam_find_argument(&statement_buffer[index],
		&expr_begin, &expr_end, &delimiter);

	expr_begin += index;
	expr_end += index;
	delimiter += index;

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_get_array_argument(&statement_buffer[expr_begin],
			&symbol_record, &literal_array_data,
			&dir_start_index, &dir_stop_index, 0);
		statement_buffer[expr_end] = save_ch;
	}

	if (status == JAMC_SUCCESS)
	{
		if (symbol_record != NULL)
		{
			heap_record = (JAMS_HEAP_RECORD *)symbol_record->value;

			if (heap_record != NULL)
			{
				dir_vector = heap_record->data;
			}
			else
			{
				status = JAMC_INTERNAL_ERROR;
			}
		}
		else if (literal_array_data != NULL)
		{
			dir_vector = literal_array_data;
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	/*
	*	Get data vector
	*/
	if (status == JAMC_SUCCESS)
	{
		index = delimiter + 1;
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		expr_begin += index;
		expr_end += index;
		delimiter += index;
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_COMMA_CHAR) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		save_ch = statement_buffer[expr_end];
		statement_buffer[expr_end] = JAMC_NULL_CHAR;
		status = jam_get_array_argument(&statement_buffer[expr_begin],
			&symbol_record, &literal_array_data,
			&data_start_index, &data_stop_index, 0);
		statement_buffer[expr_end] = save_ch;
	}

	if (status == JAMC_SUCCESS)
	{
		if (symbol_record != NULL)
		{
			heap_record = (JAMS_HEAP_RECORD *)symbol_record->value;

			if (heap_record != NULL)
			{
				data_vector = heap_record->data;
			}
			else
			{
				status = JAMC_INTERNAL_ERROR;
			}
		}
		else if (literal_array_data != NULL)
		{
			data_vector = literal_array_data;
		}
		else
		{
			status = JAMC_INTERNAL_ERROR;
		}
	}

	if (status == JAMC_SUCCESS)
	{
		/*
		*	Do a simple VECTOR operation -- no capture or compare
		*/
		if (jam_vector_io(jam_vector_signal_count,
			dir_vector, data_vector, NULL) != jam_vector_signal_count)
		{
			status = JAMC_UNSUPPORTED_FEATURE;
		}


		/*
		*	Need to add support for CAPTURE and COMPARE, and non-zero
		*	start indices.  Also needs more error checking.
		*/
	}
	

	return (status);
}

/****************************************************************************/
/*																			*/

#define JAMC_MAX_VECTOR_SIGNALS 256

JAM_RETURN_TYPE jam_process_vmap
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Sets signal mapping for non-JTAG hardware interface.	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int signal_count = 0;
	char *signal_names[JAMC_MAX_VECTOR_SIGNALS];
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	index = jam_skip_instruction_name(statement_buffer);

	while ((statement_buffer[index] != JAMC_SEMICOLON_CHAR) &&
		(status == JAMC_SUCCESS) &&
		(index < JAMC_MAX_STATEMENT_LENGTH) &&
		(signal_count < JAMC_MAX_VECTOR_SIGNALS))
	{
		/*
		*	Get signal names from statement, add NULL terminination characters,
		*	and save a pointer to each name
		*/
		if (statement_buffer[index] == JAMC_QUOTE_CHAR)
		{
			++index;
			signal_names[signal_count] = &statement_buffer[index];
			while ((statement_buffer[index] != JAMC_QUOTE_CHAR) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;
			}
			if (statement_buffer[index] == JAMC_QUOTE_CHAR)
			{
				statement_buffer[index] = JAMC_NULL_CHAR;
				++index;

				if (*signal_names[signal_count] == JAMC_NULL_CHAR)
				{
					/* check for empty string */
					status = JAMC_SYNTAX_ERROR;
				}
				else
				{
					++signal_count;
				}
			}
			else
			{
				/* terminating quote character not found */
				status = JAMC_SYNTAX_ERROR;
			}
		}
		else
		{
			/* argument is not a quoted string */
			status = JAMC_SYNTAX_ERROR;
		}

		if (status == JAMC_SUCCESS)
		{
			while ((jam_isspace(statement_buffer[index])) &&
				(index < JAMC_MAX_STATEMENT_LENGTH))
			{
				++index;	/* skip over white space */
			}

			if (statement_buffer[index] == JAMC_COMMA_CHAR)
			{
				/*
				*	Skip over comma and white space to process next argument
				*/
				++index;
				while ((jam_isspace(statement_buffer[index])) &&
					(index < JAMC_MAX_STATEMENT_LENGTH))
				{
					++index;	/* skip over white space */
				}
			}
			else if (statement_buffer[index] != JAMC_SEMICOLON_CHAR)
			{
				/*
				*	If no comma to seperate arguments, statement must be
				*	terminated by a semicolon
				*/
				status = JAMC_SYNTAX_ERROR;
			}
		}
	}

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[index] != JAMC_SEMICOLON_CHAR))
	{
		/* exceeded statement buffer length or signal count limit */
		status = JAMC_SYNTAX_ERROR;
	}

	if (status == JAMC_SUCCESS)
	{
		if (jam_vector_map(signal_count, signal_names) == signal_count)
		{
			jam_vector_signal_count = signal_count;
		}
		else
		{
			status = JAMC_UNSUPPORTED_FEATURE;
			jam_vector_signal_count = 0;
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_wait_cycles
(
	char *statement_buffer,
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
	long cycles = 0L;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	/*
	*	Call parser to evaluate expression
	*/
	status = jam_evaluate_expression(statement_buffer, &cycles, &expr_type);

	/*
	*	Check for integer expression
	*/
	if ((status == JAMC_SUCCESS) &&
		(expr_type != JAM_INTEGER_EXPR) &&
		(expr_type != JAM_INT_OR_BOOL_EXPR))
	{
		status = JAMC_TYPE_MISMATCH;
	}

	/*
	*	Do the JTAG hardware operation
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_do_wait_cycles(cycles, wait_state);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_wait_microseconds
(
	char *statement_buffer,
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
	long microseconds = 0L;
	JAME_EXPRESSION_TYPE expr_type = JAM_ILLEGAL_EXPR_TYPE;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	/*
	*	Call parser to evaluate expression
	*/
	status = jam_evaluate_expression(statement_buffer, &microseconds,
		&expr_type);

	/*
	*	Check for integer expression
	*/
	if ((status == JAMC_SUCCESS) &&
		(expr_type != JAM_INTEGER_EXPR) &&
		(expr_type != JAM_INT_OR_BOOL_EXPR))
	{
		status = JAMC_TYPE_MISMATCH;
	}

	/*
	*	Do the JTAG hardware operation
	*/
	if (status == JAMC_SUCCESS)
	{
		status = jam_do_wait_microseconds(microseconds, wait_state);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_process_wait
(
	char *statement_buffer
)

/*																			*/
/*	Description:	Processes WAIT statement								*/
/*																			*/
/*					syntax: WAIT [<wait-state>,] [<integer-expr> CYCLES,]	*/
/*								 [<integer-expr> USEC,] [<end-state>];		*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	int index = 0;
	int expr_begin = 0;
	int expr_end = 0;
	int delimiter = 0;
	char save_ch = 0;
	BOOL found_wait_state = FALSE;
	BOOL found_condition = FALSE;
	BOOL found_end_state = FALSE;
	JAME_JTAG_STATE state = JAM_ILLEGAL_JTAG_STATE;
	JAME_JTAG_STATE wait_state = IDLE;
	JAME_JTAG_STATE end_state = IDLE;
	JAM_RETURN_TYPE status = JAMC_SYNTAX_ERROR;

	index = jam_skip_instruction_name(statement_buffer);

	do
	{
		/*
		*	Get next argument
		*/
		status = jam_find_argument(&statement_buffer[index],
			&expr_begin, &expr_end, &delimiter);

		if (status == JAMC_SUCCESS)
		{
			expr_begin += index;
			expr_end += index;
			delimiter += index;

			save_ch = statement_buffer[expr_end];
			statement_buffer[expr_end] = JAMC_NULL_CHAR;
			state = jam_get_jtag_state_from_name(&statement_buffer[expr_begin]);

			if (state == JAM_ILLEGAL_JTAG_STATE)
			{
				/* first argument was not a JTAG state name */
				index = expr_end - 1;
				while ((index > expr_begin) &&
					(!jam_isspace(statement_buffer[index])))
				{
					--index;
				}
				if ((index > expr_begin) &&
					(jam_isspace(statement_buffer[index])))
				{
					++index;

					if (jam_strcmp(&statement_buffer[index], "CYCLES") == 0)
					{
						statement_buffer[index] = JAMC_NULL_CHAR;
						status = jam_process_wait_cycles(
							&statement_buffer[expr_begin], wait_state);
						statement_buffer[index] = 'C';
					}
					else if (jam_strcmp(&statement_buffer[index], "USEC") == 0)
					{
						statement_buffer[index] = JAMC_NULL_CHAR;
						status = jam_process_wait_microseconds(
							&statement_buffer[expr_begin], wait_state);
						statement_buffer[index] = 'U';
					}
					else
					{
						status = JAMC_SYNTAX_ERROR;
					}

					found_condition = TRUE;
				}

				index = delimiter + 1;
			}
			else
			{
				/* argument was a JTAG state name */
				if ((!found_condition) && (!found_wait_state))
				{
					wait_state = state;
					found_wait_state = TRUE;
				}
				else if ((found_condition) && (!found_end_state))
				{
					end_state = state;
					found_end_state = TRUE;
				}
				else
				{
					status = JAMC_SYNTAX_ERROR;
				}

				index = delimiter + 1;
			}

			statement_buffer[expr_end] = save_ch;
		}
	}
	while ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] == JAMC_COMMA_CHAR));

	if ((status == JAMC_SUCCESS) &&
		(statement_buffer[delimiter] != JAMC_SEMICOLON_CHAR))
	{
		status = JAMC_SYNTAX_ERROR;
	}

	if ((status == JAMC_SUCCESS) && (!found_condition))
	{
		/* there must have been at least one condition argument */
		status = JAMC_SYNTAX_ERROR;
	}

	/*
	*	If end state was specified, go there
	*/
	if ((status == JAMC_SUCCESS) && (end_state != IDLE))
	{
		status = jam_goto_jtag_state(end_state);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

JAM_RETURN_TYPE jam_execute_statement
(
	char *statement_buffer,
	BOOL *done,
	BOOL *reuse_statement_buffer,
	int *exit_code
)

/*																			*/
/*	Description:	Processes a statement by calling the processing			*/
/*					sub-function which corresponds to the instruction type	*/
/*																			*/
/*	Returns:		JAMC_SUCCESS for success, else appropriate error code	*/
/*																			*/
/****************************************************************************/
{
	JAME_INSTRUCTION instruction_code = JAM_ILLEGAL_INSTR;
	JAM_RETURN_TYPE status = JAMC_SUCCESS;

	instruction_code = jam_get_instruction(statement_buffer);

	switch (instruction_code)
	{
	case JAM_BOOLEAN_INSTR:
		status = jam_process_boolean(statement_buffer);
		break;

	case JAM_CALL_INSTR:
		status = jam_process_call_or_goto(statement_buffer, TRUE);
		break;

	case JAM_CRC_INSTR:
		status = JAMC_SYNTAX_ERROR;
		break;

	case JAM_DRSCAN_INSTR:
		status = jam_process_drscan(statement_buffer);
		break;

	case JAM_DRSTOP_INSTR:
		status = jam_process_drstop(statement_buffer);
		break;

	case JAM_EXIT_INSTR:
		status = jam_process_exit(statement_buffer, done, exit_code);
		break;

	case JAM_EXPORT_INSTR:
		status = jam_process_export(statement_buffer);
		break;

	case JAM_FOR_INSTR:
		status = jam_process_for(statement_buffer);
		break;

	case JAM_GOTO_INSTR:
		status = jam_process_call_or_goto(statement_buffer, FALSE);
		break;

	case JAM_IF_INSTR:
		status = jam_process_if(statement_buffer, reuse_statement_buffer);
		break;

	case JAM_INTEGER_INSTR:
		status = jam_process_integer(statement_buffer);
		break;

	case JAM_IRSCAN_INSTR:
		status = jam_process_irscan(statement_buffer);
		break;

	case JAM_IRSTOP_INSTR:
		status = jam_process_irstop(statement_buffer);
		break;

	case JAM_LET_INSTR:
		status = jam_process_let(statement_buffer);
		break;

	case JAM_NEXT_INSTR:
		status = jam_process_next(statement_buffer);
		break;

	case JAM_NOTE_INSTR:
		/* ignore NOTE statements during execution */
		break;

	case JAM_PADDING_INSTR:
		status = jam_process_padding(statement_buffer);
		break;

	case JAM_POP_INSTR:
		status = jam_process_pop(statement_buffer);
		break;

	case JAM_PRINT_INSTR:
		status = jam_process_print(statement_buffer);
		break;

	case JAM_PUSH_INSTR:
		status = jam_process_push(statement_buffer);
		break;

	case JAM_REM_INSTR:
		/* ignore REM statements during execution */
		break;

	case JAM_RETURN_INSTR:
		status = jam_process_return(statement_buffer);
		break;

	case JAM_STATE_INSTR:
		status = jam_process_state(statement_buffer);
		break;

	case JAM_VECTOR_INSTR:
		status = jam_process_vector(statement_buffer);
		break;

	case JAM_VMAP_INSTR:
		status = jam_process_vmap(statement_buffer);
		break;

	case JAM_WAIT_INSTR:
		status = jam_process_wait(statement_buffer);
		break;

	default:
		status = JAMC_SYNTAX_ERROR;
		break;
	}

	return (status);
}

/****************************************************************************/
/*																			*/

long jam_get_line_of_position
(
	long position
)

/*																			*/
/*	Description:	Determines the line number in the input stream which	*/
/*					corresponds to the given position (offset) in the		*/
/*					stream.  This is used for error reporting.				*/
/*																			*/
/*	Returns:		line number, or zero if it could not be determined		*/
/*																			*/
/****************************************************************************/
{
	long line = 0L;
	long index = 0L;
	int ch;

	if (jam_seek(0L) == 0)
	{
		++line;	/* first line is line 1, not zero */

		for (index = 0; index < position; ++index)
		{
			ch = jam_getc();

			if (ch == JAMC_NEWLINE_CHAR)
			{
				++line;
			}
		}
	}

	return (line);
}

/****************************************************************************/
/*																			*/
JAM_RETURN_TYPE jam_execute
(
	char **init_list,
	char *workspace,
	long size,
	long *error_line,
	int *exit_code
)
/*																			*/
/*	Description:	This is the main entry point for executing an JAM		*/
/*					program.  It returns after execution has terminated.	*/
/*					The program data is not passed into this function,		*/
/*					but is accessed through the jam_getc() function.		*/
/*																			*/
/*	Return:			JAMC_SUCCESS for successful execution, otherwise one	*/
/*					of the error codes listed in <jamexprt.h>				*/
/*																			*/
/****************************************************************************/
{
	JAM_RETURN_TYPE status = JAMC_SUCCESS;
	char statement_buffer[JAMC_MAX_STATEMENT_LENGTH + 1];
	char label_buffer[JAMC_MAX_NAME_LENGTH + 1];
	BOOL done = FALSE;
	BOOL reuse_statement_buffer = FALSE;

	jam_init_list = init_list;
	jam_workspace = workspace;
	jam_workspace_size = size;
	jam_current_file_position = 0L;
	jam_current_statement_position = 0L;
	jam_next_statement_position = 0L;

	/*
	*	Initialize symbol table and stack
	*/
	status = jam_init_symbol_table();

	if (status == JAMC_SUCCESS)
	{
		status = jam_init_stack();
	}

	if (status == JAMC_SUCCESS)
	{
		status = jam_init_heap();
	}

	if (status == JAMC_SUCCESS)
	{
		status = jam_seek(0L);
	}

	/*
	*	Get program statements and execute them
	*/
	while ((!done) && (status == JAMC_SUCCESS))
	{
		if (!reuse_statement_buffer)
		{
			status = jam_get_statement
			(
				statement_buffer,
				label_buffer
			);

			if ((status == JAMC_SUCCESS)
				&& (label_buffer[0] != JAMC_NULL_CHAR))
			{
				status = jam_add_symbol
				(
					JAM_LABEL,
					label_buffer,
					0L,
					jam_current_statement_position
				);
			}
		}
		else
		{
			/* statement buffer will be reused -- clear the flag */
			reuse_statement_buffer = FALSE;
		}

		if (status == JAMC_SUCCESS)
		{
			status = jam_execute_statement
			(
				statement_buffer,
				&done,
				&reuse_statement_buffer,
				exit_code
			);
		}
	}

	if ((status != JAMC_SUCCESS) && (error_line != NULL))
	{
		*error_line = jam_get_line_of_position(
			jam_current_statement_position);
	}

	return (status);
}
