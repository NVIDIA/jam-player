/****************************************************************************/
/*																			*/
/*	Module:			jamdata.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Encodes binary information from a packed binary			*/
/*					representation (in a file) to any of the four Boolean	*/
/*					array data formats supported by the Jam language.		*/
/*					Also decodes data represented in those formats into a	*/
/*					binary file.  The supported formats are BIN, HEX, RLC,	*/
/*					and ACA.  Output is to standard output stream by		*/
/*					default, or to a file if an output filename is given.	*/
/*																			*/
/*	Usage:			jamdata -d -e -f<format> <input-file> [<output-file>]	*/
/*					-d : decode Jam data into a file						*/
/*					-e : encode a file into Jam data						*/
/*					-f : format may be BIN, HEX, RLC, or ACA.				*/
/*					-h : help message										*/
/*																			*/
/****************************************************************************/

#ifndef NO_ALTERA_STDIO
#define NO_ALTERA_STDIO
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef int BOOL;
#define TRUE 1
#define FALSE 0

/* line length for text output */
#define JAM_LINE_LENGTH     72

/* ACA definitions */
#define	DATA_BLOB_LENGTH	3
#define	SHORT_BITS			16
#define	CHAR_BITS			8
#define	MATCH_DATA_LENGTH	8192
#define	HASH_TABLE_SIZE		4001
#define	MAX_MATCH_LENGTH	255

/* RLC definitions */
#define JAM_RUN_LENGTH		16


/* This macro should be modified for any change to DATA_BLOB_LENGTH. */
#define	COMPARE_DATA(a, b)	((a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]))

typedef	unsigned char	DATA_BLOB[DATA_BLOB_LENGTH];

/* Hash table entry. */
typedef struct S_TCA_HASH_ENTRY
{
	long					offset;		/* Offset of data in input. */
	DATA_BLOB				data;		/* Actual data. */
	struct S_TCA_HASH_ENTRY	*next_hash;	/* Next entry with same hash value. */
	struct S_TCA_HASH_ENTRY	*next_data;	/* Entry with data 1 byte offset forward. */
} S_TCA_HASH_ENTRY;

struct 
{
	S_TCA_HASH_ENTRY	*head;
	S_TCA_HASH_ENTRY	*tail;
} jam_hash_table[HASH_TABLE_SIZE] = { 0 };

S_TCA_HASH_ENTRY	*jam_match_point = NULL;	/* Pointer to input data being matched. */
S_TCA_HASH_ENTRY	*jam_match_data = NULL;		/* Point beyond which data will be matched to jam_match_point. */


/****************************************************************************/
/*																			*/

long jam_write_aca(char *buffer, long length, short data, short bits)

/*																			*/
/*	Description:	Write a value into buffer in "bits" bits.				*/
/*					The location of the last data written to buffer is		*/
/*					stored internally to this function.						*/
/*																			*/
/*	Returns:		length of data in buffer, -1 if overrun.				*/
/*																			*/
/****************************************************************************/
{
	BOOL			result = FALSE;
	static long		index = 0L;
	static short	bits_used = 0;

	/* If buffer is NULL then initialize. */
	if (buffer == NULL)
	{
		index = 0L;
		bits_used = 0;
	}
	else
	{
		result = TRUE;
		while (result && bits > 0)
		{
			buffer[index] = (char) (buffer[index] | (data << bits_used));

			if (bits < (CHAR_BITS - bits_used))
			{
				bits_used = (short) (bits_used + bits);
				bits = 0;
			}
			else
			{
				/* Check for buffer overflow. */
				if (++index >= length) result = FALSE;
				else
				{
					data = (short) (data >> (CHAR_BITS - bits_used));
					bits = (short) (bits - (CHAR_BITS - bits_used));
					bits_used = 0;
				}
			}
		}
	}

	return (!result ? -1L : (bits_used == 0 ? index : index + 1));
}

/****************************************************************************/
/*																			*/

unsigned int jam_hash(DATA_BLOB	data)

/*																			*/
/*	Description:	Generate a hash index for a data blob.					*/
/*																			*/
/*	Returns:		Index into hash table.									*/
/*																			*/
/****************************************************************************/
{
	unsigned int	result = 0x5a5a;
	int				i = 0;

	for (i = 0; i < DATA_BLOB_LENGTH; ++i)
	{
		result = (result << (result & 0xf)) | data[i];
		if (data[i] & 0x1) result ^= 0xa5a5;
	}

	result = result % HASH_TABLE_SIZE;

	return (result);
}

/****************************************************************************/
/*																			*/

void jam_prune_hash_table_index(unsigned int i, long oldest) 

/*																			*/
/*	Description:	Remove entries from the hash table that are too old to	*/
/*					be used for matching.									*/
/*																			*/
/*	Returns:		Nothing.												*/
/*																			*/
/****************************************************************************/
{
	S_TCA_HASH_ENTRY	*e = NULL;	
	
	e = jam_hash_table[i].head;
	while (e != NULL && e->offset < oldest)
	{
		jam_hash_table[i].head = e->next_hash;
		if (jam_hash_table[i].tail == e) jam_hash_table[i].tail = NULL;
		free((void *) e);
		e = jam_hash_table[i].head;
	}
}

/****************************************************************************/
/*																			*/

void jam_add_hash_entry(S_TCA_HASH_ENTRY *entry)

/*																			*/
/*	Description:	Add a new entry to the hash table. As part of the add	*/
/*					process, any entries with the same hash index that		*/
/*					are too old are removed.								*/
/*																			*/
/*	Returns:		Nothing.												*/
/*																			*/
/****************************************************************************/
{
	unsigned int i;

	/* Get hash index for data. */
	i = jam_hash(entry->data);

	/* Prune entries with this hash index that are too old. */
	if (jam_match_data != NULL) jam_prune_hash_table_index(i, jam_match_data->offset);

	/* Add new entry. */
	if (jam_hash_table[i].tail != NULL) jam_hash_table[i].tail->next_hash = entry;
	if (jam_hash_table[i].head == NULL) jam_hash_table[i].head = entry;
	jam_hash_table[i].tail = entry;
}

/****************************************************************************/
/*																			*/

S_TCA_HASH_ENTRY *jam_get_hash_entry
(
	int		ch,		/* Next character from input stream.. */
	BOOL	init,	/* If TRUE, function will initialize and return. */
	BOOL	*end	/* Returns TRUE if end of input data stream. */
)

/*																			*/
/*	Description:	Construct a hash entry. This is done by packaging		*/
/*					up DATA_BLOB_LENGTH sets of bytes. All DATA_BLOB_LENGTH	*/
/*					sets of bytes are stored in the hash table at some		*/
/*					point in time. Setting ch to -1 DATA_BLOB_LENGTH		*/
/*					times indicates the end of the input data stream		*/
/*																			*/
/*	Returns:		Pointer to hash table entry, NULL if error or end of	*/
/*					input.													*/
/*																			*/
/****************************************************************************/
{
	static int				prev_data[DATA_BLOB_LENGTH] = { 0 };
	static long				count = 0L;
	static int				next_offset = 0;
	static S_TCA_HASH_ENTRY	*prev_result = NULL;
	static int				end_count = 0;
	int						i;
	S_TCA_HASH_ENTRY		*result = NULL;

	if (end != NULL) *end = FALSE;
	
	/* Initialize if required. */
	if (init)
	{
		prev_result = NULL;
		end_count = 0;
		count = 0L;
		next_offset = 0;
		for (i = 0; i < DATA_BLOB_LENGTH; ++i) prev_data[i] = 0;
	}		
	else
	{
		if (ch == -1) ++end_count;

		/* DATA_BLOB_LENGTH -1s in a row means end of file. */
		if (end != NULL && end_count >= DATA_BLOB_LENGTH) *end = TRUE;
		else
		{
			prev_data[next_offset++] = ch;
			next_offset = next_offset % DATA_BLOB_LENGTH;
			count++;
	
			if (count >= DATA_BLOB_LENGTH)
			{
				if ((result = (S_TCA_HASH_ENTRY *)
					calloc(1, sizeof (S_TCA_HASH_ENTRY))) != NULL) 
				{
					result->offset = count - DATA_BLOB_LENGTH;
					for (i = 0; i < DATA_BLOB_LENGTH; ++i)
					{
						result->data[i] = (unsigned char) prev_data[(next_offset + i) % DATA_BLOB_LENGTH];
					}
					if (prev_result != NULL) prev_result->next_data = result;
					prev_result = result;
					result->next_hash = NULL;
				}
			}
		}
	}

	return (result);
	
}

/****************************************************************************/
/*																			*/

BOOL jam_aca_read_ahead(char *buffer, long length)

/*																			*/
/*	Description:	Read ahead at least MAX_MATCH_LENGTH bytes from input	*/
/*					stream and store in hash table.							*/
/*																			*/
/*	Returns:		TRUE if successful, FALSE if out of memory.				*/
/*																			*/
/****************************************************************************/
{
	S_TCA_HASH_ENTRY	*entry = NULL;
	BOOL				done = FALSE;
	BOOL				end = FALSE;
	int					ch;
	static long			index = 0L;

	if (buffer == NULL) index = 0L;
	else
	{
		while (!end && !done)
		{
			/* Get next byte from input. */
			if (index < length) ch = (int) (buffer[index++] & 0xFF);
			else ch = -1;

			/* Package up bytes into BLOB. */
			if ((entry = jam_get_hash_entry(ch, FALSE, &end)) != NULL)
			{
				/* Add to hash table. */
				jam_add_hash_entry(entry);

				/* Initialize if necessary. */
				if (jam_match_point == NULL) jam_match_point = entry;
				if (jam_match_data == NULL) jam_match_data = entry;

				/* Advance the start of the match data if necessary. Will only find match */
				/* in previous MATCH_DATA_LENGTH characters. */
				while ((jam_match_point->offset - jam_match_data->offset) > MATCH_DATA_LENGTH) jam_match_data = jam_match_data->next_data;

				if (entry->offset >= (jam_match_point->offset + MAX_MATCH_LENGTH)) done = TRUE;
			}
		}
	}

	return (entry != NULL || end);
}

/****************************************************************************/
/*																			*/

BOOL jam_aca_find_match(long *offset, int *length)

/*																			*/
/*	Description:	Look for a match for the current data to the previous	*/
/*					data. Absolute offset and length of matched data are	*/
/*					returned.												*/
/*																			*/
/*	Returns:		TRUE if successful, FALSE if out of memory.				*/
/*																			*/
/****************************************************************************/
{
	BOOL				found = TRUE;
	BOOL				still_matching = FALSE;
	S_TCA_HASH_ENTRY	*start = NULL;
	S_TCA_HASH_ENTRY	*entry = NULL;
	S_TCA_HASH_ENTRY	*matching = NULL;
	unsigned int		i;
	long				current_offset;
	int					current_length;

	*offset = 0L;
	*length = 0;

	i = jam_hash(jam_match_point->data);

	if (jam_hash_table[i].head != NULL)
	{
		start = jam_hash_table[i].head;

		while (found)
		{
			found = FALSE;
			while (start != NULL && !found)
			{
				if ((start->offset >= jam_match_data->offset) && COMPARE_DATA(start->data, jam_match_point->data)) 
				{
					found = TRUE;
				}
				else start = start->next_hash;
			}

			if (found)
			{
				current_offset = start->offset;
				current_length = DATA_BLOB_LENGTH;

				entry = start;
				start = start->next_hash;
				matching = jam_match_point;


				still_matching = TRUE;
				while (still_matching)
				{ 
					i = 0;
					while (matching != NULL && entry->offset < jam_match_point->offset && i < 3) 
					{
						entry = entry->next_data;
						matching = matching->next_data;
						++i;
					}
	
					if (i < 3 || matching == NULL || !COMPARE_DATA(entry->data, matching->data)) still_matching = FALSE;
					else
					{
						current_length += DATA_BLOB_LENGTH;
						if (current_length > *length)
						{
							*length = current_length;
							*offset = current_offset;

							/* Terminate if longest possible match found. */
							if (*length >= MAX_MATCH_LENGTH) found = FALSE;
						}
					}
				}

			}
		}
	}

	if (*length > 0)
	{
		if ((*offset + *length) >= jam_match_point->offset)
		{
			*length = (int) (jam_match_point->offset - *offset);
		}

		if (*length > MAX_MATCH_LENGTH) *length = MAX_MATCH_LENGTH;
	}

	return (*length != 0);
}

/****************************************************************************/
/*																			*/

short jam_aca_bits_required(short n)

/*																			*/
/*	Description:	Calculate the minimum number of bits required to		*/
/*					represent n.											*/
/*																			*/
/*	Returns:		Number of bits.											*/
/*																			*/
/****************************************************************************/
{
	short	result = SHORT_BITS;

	if (n == 0) result = 1;
	else
	{
		/* Look for the highest non-zero bit position */
		while ((n & (1 << (SHORT_BITS - 1))) == 0)
		{
			n = (short) (n << 1);
			--result;
		}
	}

	return (result);
}

/****************************************************************************/
/*																			*/

void jam_free_aca_hash_table(void)

/*																			*/
/*	Description:	Frees the memory of elements stored in hash table		*/
/*																			*/
/*	Returns:		nothing													*/
/*																			*/
/****************************************************************************/
{
	int i;
	S_TCA_HASH_ENTRY *e, *next;

	for (i = 0; i < HASH_TABLE_SIZE; ++i)
	{
		e = jam_hash_table[i].head;

		while (e != NULL)
		{
			next = e->next_hash;
			free((void *) e);
			e = next;
		}

		jam_hash_table[i].head = NULL;
	}
}

/****************************************************************************/
/*																			*/

long jam_compress_aca
(
	char *in, 
	long in_length, 
	char *out, 
	long out_length
)

/*																			*/
/*	Description:	Compress data in "in" and write result to "out".		*/
/*																			*/
/*	Returns:		Length of compressed data. -1 if:						*/
/*						1) out_length is too small							*/
/*						2) Internal error in the code						*/
/*																			*/
/****************************************************************************/
{
	long	result = 0L;
	long	offset;
	int		length;
	int		i;

	/* Initialize. */
	jam_match_point = NULL;
	jam_match_data = NULL;
	for (i = 0; i < HASH_TABLE_SIZE; ++i)
	{
		jam_hash_table[i].head = NULL;
		jam_hash_table[i].tail = NULL;
	}
	jam_get_hash_entry(0, TRUE, NULL);
	jam_write_aca(NULL, 0, 0, 0);
	jam_aca_read_ahead(NULL, 0);
	for (i = 0; i < out_length; ++i) out[i] = 0;

	/* Write data length. */
	for (i = 0; i < (int) sizeof (in_length); ++i) jam_write_aca(out, out_length, (short) ((in_length >> (i * CHAR_BITS)) & 0xFF), CHAR_BITS);

	jam_aca_read_ahead(in, in_length);

	while (jam_match_point != NULL && result != -1L)
	{
		if (jam_aca_find_match(&offset, &length))
		{
			/* Write matched data. */
			result = jam_write_aca(out, out_length, 1, 1);	/* Indicates offset/length follows. */
			result = jam_write_aca(out, out_length, (short) (jam_match_point->offset - offset), jam_aca_bits_required((short) (jam_match_point->offset > MATCH_DATA_LENGTH ? MATCH_DATA_LENGTH : jam_match_point->offset)));
			result = jam_write_aca(out, out_length, (short) (length), CHAR_BITS);

			/* Advance jam_match_point. */
			offset = jam_match_point->offset + length;
		}
		else
		{
			/* Write literal data. */
			result = jam_write_aca(out, out_length, 0, 1);	/* Indicates literal data follows. */
			for (i = 0; i < DATA_BLOB_LENGTH; ++i) result = jam_write_aca(out, out_length, (short) jam_match_point->data[i], CHAR_BITS);

			/* Advance jam_match_point. */
			offset = jam_match_point->offset + DATA_BLOB_LENGTH;
		}

		while (jam_match_point != NULL && jam_match_point->offset < offset) jam_match_point = jam_match_point->next_data;

		jam_aca_read_ahead(in, in_length);	
	}

	jam_free_aca_hash_table();

	return (result);
}

/****************************************************************************/
/*																			*/

short jam_bits_required(short n)

/*																			*/
/*	Description:	Calculate the minimum number of bits required to		*/
/*					represent n.											*/
/*																			*/
/*	Returns:		Number of bits.											*/
/*																			*/
/****************************************************************************/
{
	short	result = SHORT_BITS;

	if (n == 0) result = 1;
	else
	{
		/* Look for the highest non-zero bit position */
		while ((n & (1 << (SHORT_BITS - 1))) == 0)
		{
			n = (short) (n << 1);
			--result;
		}
	}

	return (result);
}

/****************************************************************************/
/*																			*/

short jam_read_packed(char *buffer, long length, short bits)

/*																			*/
/*	Description:	Read the next value from the input array "buffer".		*/
/*					Read only "bits" bits from the array. The amount of		*/
/*					bits that have already been read from "buffer" is		*/
/*					stored internally to this function.					 	*/
/*																			*/
/*	Returns:		Up to 16 bit value. -1 if buffer overrun.				*/
/*																			*/
/****************************************************************************/
{
	short			result = -1;
	static long		index = 0L;
	static short	bits_avail = 0;
	short			shift = 0;

	/* If buffer is NULL then initialize. */
	if (buffer == NULL)
	{
		index = 0;
		bits_avail = CHAR_BITS;
	}
	else
	{
		result = 0;
		while (result != -1 && bits > 0)
		{
			result = (short) (result | (((buffer[index] >> (CHAR_BITS - bits_avail)) & (0xFF >> (CHAR_BITS - bits_avail))) << shift));

			if (bits <= bits_avail)
			{
				result = (short) (result & (0xFFFF >> (SHORT_BITS - (bits + shift))));
				bits_avail = (short) (bits_avail - bits);
				bits = 0;
			}
			else
			{
				/* Check for buffer overflow. */
				if (++index >= length) result = -1;
				else
				{
					shift = (short) (shift + bits_avail);
					bits = (short) (bits - bits_avail);
					bits_avail = CHAR_BITS;
				}
			}
		}
	}

	return (result);
}

/****************************************************************************/
/*																			*/

long jam_uncompress_aca
(
	char *in, 
	long in_length, 
	char *out, 
	long out_length
)

/*																			*/
/*	Description:	Uncompress data in "in" and write result to	"out".		*/
/*																			*/
/*	Returns:		Length of uncompressed data. -1 if:						*/
/*						1) out_length is too small							*/
/*						2) Internal error in the code						*/
/*						3) in doesn't contain ACA compressed data.			*/
/*																			*/
/****************************************************************************/
{
	long	i, j, data_length = 0L;
	short	offset, length;
	
	jam_read_packed(NULL, 0, 0);
	for (i = 0; i < out_length; ++i) out[i] = 0;

	/* Read number of bytes in data. */
	for (i = 0; i < sizeof (in_length); ++i) 
	{
		data_length = data_length | ((long) jam_read_packed(in, in_length, CHAR_BITS) << (long) (i * CHAR_BITS));
	}

	if (data_length > out_length) data_length = -1L;
	else
	{
		i = 0;
		while (i < data_length)
		{
			/* A 0 bit indicates literal data. */
			if (jam_read_packed(in, in_length, 1) == 0)
			{
				for (j = 0; j < DATA_BLOB_LENGTH; ++j)
				{
					if (i < data_length)
					{
						out[i] = (char) jam_read_packed(in, in_length, CHAR_BITS);
						i++;
					}
				}
			}
			else
			{
				/* A 1 bit indicates offset/length to follow. */
				offset = jam_read_packed(in, in_length, jam_bits_required((short) (i > MATCH_DATA_LENGTH ? MATCH_DATA_LENGTH : i)));
				length = jam_read_packed(in, in_length, CHAR_BITS);

				for (j = 0; j < length; ++j)
				{
					if (i < data_length)
					{
						out[i] = out[i - offset];
						i++;
					}
				}
			}
		}
	}

	return (data_length);
}

/****************************************************************************/
/*																			*/

void jam_put_char
(
	int ch,
	int *position,
	FILE *fp
)

/*																			*/
/*	Description:	Writes a text character to output stream, with a line	*/
/*					break if the line length has reached the maximum.		*/
/*																			*/
/*	Returns:		nothing													*/
/*																			*/
/****************************************************************************/
{
	fputc(ch, fp);

	++(*position);
	if ((*position) >= JAM_LINE_LENGTH)
	{
		fputc('\n', fp);
		(*position) = 0;
	}
}

/****************************************************************************/
/*																			*/

void jam_put_6bit_char
(
	int value,
	int *position,
	FILE *fp
)

/*																			*/
/*	Description:	Writes a text character corresponding to the 6-bit		*/
/*					integer value specified.  Adds a line break if the		*/
/*					line length has reached the maximum.					*/
/*																			*/
/*	Returns:		nothing													*/
/*																			*/
/****************************************************************************/
{
	int ch = 0;

	if ((value >= 0) && (value <= 9)) ch = (value + '0');
	else if ((value >= 10) && (value <= 35)) ch = (value + 'A' - 10);
	else if ((value >= 36) && (value <= 61)) ch = (value + 'a' - 36);
	else if (value == 62) ch = '_';
	else if (value == 63) ch = '@';

	fputc(ch, fp);

	++(*position);
	if ((*position) >= JAM_LINE_LENGTH)
	{
		fputc('\n', fp);
		(*position) = 0;
	}
}

/****************************************************************************/
/*																			*/

BOOL jam_decode_bin(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Decodes boolean data in BIN format, writing decoded		*/
/*					information to the output file (a binary file).			*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	char data = 0;
	int mask = 1;
	long index = 0L;

	while (index < in_length)
	{
		if (in_buffer[index] == '0')
		{
			/* bit is a zero -- skip over it */
			mask <<= 1;
		}
		else if (in_buffer[index] == '1')
		{
			/* set a single bit */
			data |= mask;
			mask <<= 1;
		}
		/* else ignore the character */

		if (mask == 0x100)
		{
			fwrite(&data, 1, 1, fp);
			mask = 1;
			data = 0;
		}

		++index;
	}

	if (mask != 1)
	{
		fwrite(&data, 1, 1, fp);
	}

	return (TRUE);
}

/****************************************************************************/
/*																			*/

BOOL jam_decode_hex(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Decodes boolean data in HEX format, writing decoded		*/
/*					information to the output file (a binary file).			*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	char data = 0;
	int value = 0;
	int nibble = 0;
	BOOL valid = FALSE;
	long index = 0L;

	while (index < in_length)
	{
		if ((in_buffer[index] >= '0') && (in_buffer[index] <= '9'))
		{
			value = in_buffer[index] - '0';
			valid = TRUE;
		}
		else if ((in_buffer[index] >= 'A') && (in_buffer[index] <= 'F'))
		{
			value = in_buffer[index] - ('A' - 10);
			valid = TRUE;
		}
		else if ((in_buffer[index] >= 'a') && (in_buffer[index] <= 'f'))
		{
			value = in_buffer[index] - ('a' - 10);
			valid = TRUE;
		}
		else valid = FALSE;

		if (valid)
		{
			if (nibble == 1) data |= (value << 4);
			else data |= value;
			++nibble;

			if (nibble == 2)
			{
				fwrite(&data, 1, 1, fp);
				nibble = 0;
				data = 0;
			}
		}

		++index;
	}

	if (nibble != 0)
	{
		fwrite(&data, 1, 1, fp);
	}

	return (TRUE);
}

typedef enum
{
	JAM_CONSTANT_ZEROS,
	JAM_CONSTANT_ONES,
	JAM_RANDOM

} JAME_RLC_BLOCK_TYPE;

/****************************************************************************/
/*																			*/

int jam_6bit_char(int ch)

/*																			*/
/*	Description:	Gets the numeric value corresponding to the encoded		*/
/*					text character specified.								*/
/*																			*/
/*	Returns:		integer value, or -1 if character was not valid			*/
/*																			*/
/****************************************************************************/
{
	int result = 0;

	if ((ch >= '0') && (ch <= '9')) result = (ch - '0');
	else if ((ch >= 'A') && (ch <= 'Z')) result = (ch + 10 - 'A');
	else if ((ch >= 'a') && (ch <= 'z')) result = (ch + 36 - 'a');
	else if (ch == '_') result = 62;
	else if (ch == '@') result = 63;
	else result = -1;	/* illegal character */

	return (result);
}

/****************************************************************************/
/*																			*/

BOOL jam_decode_rlc(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Decodes boolean data in RLC (run-length compressed)		*/
/*					format.  Writes decoded data to (binary) output file.	*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	char data = 0;
	char ch = 0;
	int count_index = 0;
	int count_size = 0;
	int value = 0;
	int mask = 1;
	long index = 0L;
	long bit = 0L;
	long count = 0L;
	JAME_RLC_BLOCK_TYPE block_type = JAM_CONSTANT_ZEROS;
	BOOL status = TRUE;

	/* eliminate white space and line feeds */
	for (index = 0L; index < in_length; ++index)
	{
		ch = in_buffer[index];
		if (((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'Z')) ||
			((ch >= 'a') && (ch <= 'z')) || (ch == '@') || (ch == '_'))
		{
			if (count < index) in_buffer[count] = ch;
			++count;
		}
	}
	in_length = count;
	count = 0L;
	index = 0L;

	while (status && (index < in_length))
	{
		/*
		*	Get block key character
		*/
		ch = in_buffer[index];
		++index;

		if ((ch >= 'A') && (ch <= 'E'))
		{
			block_type = JAM_CONSTANT_ZEROS;
			count_size = (ch + 1 - 'A');
		}
		else if ((ch >= 'I') && (ch <= 'M'))
		{
			block_type = JAM_CONSTANT_ONES;
			count_size = (ch + 1 - 'I');
		}
		else if ((ch >= 'Q') && (ch <= 'U'))
		{
			block_type = JAM_RANDOM;
			count_size = (ch + 1 - 'Q');
		}
		else
		{
			/* error: invalid key char */
			fprintf(stderr, "Error: invalid RLC block key character\n");
			status = FALSE;
		}

		/*
		*	Get count characters
		*/
		if (status)
		{
			count = 0L;
			for (count_index = 0; status && (count_index < count_size);
				++count_index)
			{
				count <<= 6;
				value = jam_6bit_char(in_buffer[index]);
				if (value == -1)
				{
					status = FALSE;
					fprintf(stderr, "Error: invalid RLC data character\n");
				}
				else
				{
					count |= value;
				}
				++index;
			}
		}

		if (status)
		{
			switch (block_type)
			{
			case JAM_CONSTANT_ZEROS:
				for (bit = 0; bit < count; bit++)
				{
					/* add zeros to array */
					mask <<= 1;
					if (mask == 0x100)
					{
						fwrite(&data, 1, 1, fp);
						mask = 1;
						data = 0;
					}
				}
				break;

			case JAM_CONSTANT_ONES:
				for (bit = 0; bit < count; bit++)
				{
					/* add ones to array */
					data |= mask;
					mask <<= 1;
					if (mask == 0x100)
					{
						fwrite(&data, 1, 1, fp);
						mask = 1;
						data = 0;
					}
				}
				break;

			case JAM_RANDOM:
				for (bit = 0; bit < count; bit++)
				{
					/* add random data to array */
					value = jam_6bit_char(in_buffer[index + (bit / 6)]);
					if (value == -1)
					{
						status = FALSE;
						fprintf(stderr, "Error: invalid RLC data character\n");
					}
					else
					{
						if (value & (1 << (bit % 6))) data |= mask;
						mask <<= 1;
						if (mask == 0x100)
						{
							fwrite(&data, 1, 1, fp);
							mask = 1;
							data = 0;
						}
					}
				}
				index = index + (int)((count / 6) + ((count % 6) ? 1 : 0));
				break;

			default:
				fprintf(stderr, "Error: invalid RLC block key character\n");
				status = FALSE;
				break;
			}
		}
	}

	if (status && (mask != 1))
	{
		fwrite(&data, 1, 1, fp);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

BOOL jam_decode_aca(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Decodes boolean data in ACA (advanced compression alg.)	*/
/*					format.  Writes decoded data to (binary) output file.	*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	char ch = 0;
	int bit = 0;
	unsigned int byte = 0;
	int value = 0;
	long index = 0L;
	long count = 0L;
	long out_length = 0L;
	long uncompressed_length = 0L;
	long address = 0L;
	char *out_buffer = NULL;
	BOOL status = TRUE;

	/* eliminate white space and line feeds */
	for (index = 0L; index < in_length; ++index)
	{
		ch = in_buffer[index];
		if (((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'Z')) ||
			((ch >= 'a') && (ch <= 'z')) || (ch == '@') || (ch == '_'))
		{
			if (count < index) in_buffer[count] = ch;
			++count;
		}
	}
	in_length = count;
	count = 0L;

	/* convert 6-bit encoded characters to binary -- in the same buffer */
	for (index = 0L; status && (index < in_length); ++index)
	{
		value = jam_6bit_char(in_buffer[index]);
		in_buffer[index] = 0;

		if (value == -1)
		{
			status = FALSE;
		}
		else
		{
			for (bit = 0; bit < 6; ++bit)
			{
				if (value & (1 << (bit % 6)))
				{
					in_buffer[address >> 3] |= (1L << (address & 7));
				}
				else
				{
					in_buffer[address >> 3] &=
						~(unsigned int) (1 << (address & 7));
				}
				++address;
			}
		}
	}

	if (status)
	{
		/* get uncompressed data length */
		jam_read_packed(NULL, 0, 0);
		for (byte = 0; byte < sizeof (in_length); ++byte) 
		{
			out_length = out_length | ((long) jam_read_packed(in_buffer,
				in_length, CHAR_BITS) << (long) (byte * CHAR_BITS));
		}

		if ((out_buffer = (char *) calloc(1, (size_t) out_length)) == NULL)
		{
			status = FALSE;
			fprintf(stderr, "Error: can't allocate memory (%d Kbytes)\n",
				(int) (out_length / 1024L));
		}
		else
		{
			/* uncompress the data */
			uncompressed_length = jam_uncompress_aca(
				in_buffer, (address >> 3) + ((address & 7) ? 1 : 0),
				out_buffer, out_length);

			if (uncompressed_length != out_length)
			{
				fprintf(stderr, "Error: ACA decompression failed\n");
				status = FALSE;
			}
			else
			{
				/* write out uncompressed data */
				fwrite(out_buffer, 1, (size_t) out_length, fp);
			}

			free((void *) out_buffer);
		}
	}

	return (status);
}

/****************************************************************************/
/*																			*/

BOOL jam_encode_bin(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Converts binary data to BIN format, writes to output	*/
/*					stream (text characters)								*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	long bit = 0L;
	long bit_count = in_length * 8L;
	int position = 0;

	for (bit = 0L; bit < bit_count; ++bit)
	{
		fputc((in_buffer[bit >> 3] & (1 << (bit & 7))) ? '1' : '0', fp);

		++position;
		if (position >= JAM_LINE_LENGTH)
		{
			fputc('\n', fp);
			position = 0;
		}
	}

	if (position != 0) fputc('\n', fp);

	return (TRUE);
}

/****************************************************************************/
/*																			*/

BOOL jam_encode_hex(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Converts binary data to HEX format, writes to output	*/
/*					stream (text characters)								*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	long bit = 0L;
	long bit_count = in_length * 8L;
	int ch = 0;
	int position = 0;

	for (bit = 0L; bit < bit_count; ++bit)
	{
		ch |= (in_buffer[bit >> 3] & (1 << (bit & 7))) ? (1 << (bit & 3)) : 0;

		if ((bit & 3) == 3)
		{
			fputc((ch >= 10) ? (ch + 'A' - 10) : (ch + '0'), fp);
			ch = 0;

			++position;
			if (position >= JAM_LINE_LENGTH)
			{
				fputc('\n', fp);
				position = 0;
			}
		}
	}

	if (bit & 3)
	{
		fputc((ch >= 10) ? (ch + 'A' - 10) : (ch + '0'), fp);
		++position;
	}

	if (position != 0) fputc('\n', fp);

	return (TRUE);
}

/****************************************************************************/
/*																			*/

BOOL jam_encode_rlc(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Converts binary data to RLC format, writes to output	*/
/*					stream (text characters)								*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	long bit = 0L;
	long i = 0L;
	long j = 0L;
	long mask = 0L;
	long rand_bit = 0L;
	long bit_count = 0L;
	long constant_block_length = 0L;
	long random_block_length = 0L;
	long end_constant_block = 0L;
	long start_random_block = 0L;
	BOOL constant_block = FALSE;
	int ch = 0;
	int run_value = 0;
	int count_size = 0;
	int count_pos = 0;
	int position = 0;
	int bbit = 0;

	bit_count = in_length * 8L;

	for (bit = 0L; bit < bit_count; ++bit)
	{
		/* check for constant block */
		constant_block = TRUE;
		run_value = (in_buffer[bit >> 3] & (1 << (bit & 7))) ? 1 : 0;
		end_constant_block = bit + 1L;
		while (constant_block && (end_constant_block < bit_count))
		{
			if (((in_buffer[end_constant_block >> 3] &
				(1 << (end_constant_block & 7))) ? 1 : 0) != run_value)
			{
				constant_block = FALSE;	/* end of constant block */
			}
			else
			{
				++end_constant_block;
			}
		}

		constant_block_length = end_constant_block - bit;

		if (constant_block_length >= JAM_RUN_LENGTH)
		{
			/* start_random_block < bit < end_constant_block */

			random_block_length = bit - start_random_block;

			/*
			*	Write random block
			*/
			if (random_block_length > 0L)
			{
				/* calculate how many count characters are required */
				count_size = 0;
				mask = random_block_length;
				while (mask != 0)
				{
					++count_size;
					mask >>= 6;
				}

				/* write block header */
				jam_put_char('Q' + count_size - 1, &position, fp);
				for (count_pos = count_size - 1;
					count_pos >= 0; --count_pos)
				{
					jam_put_6bit_char((random_block_length >>
						(count_pos * 6)) & 0x3f, &position, fp);
				}

				/* write block data */
				ch = 0;
				bbit = 0;
				for (rand_bit = start_random_block; rand_bit < bit;
					++rand_bit, ++bbit)
				{
					ch |= (in_buffer[rand_bit >> 3] &
						(1 << (rand_bit & 7))) ? (1 << (bbit % 6)) : 0;

					if ((bbit % 6) == 5)
					{
						jam_put_6bit_char(ch, &position, fp);
						ch = 0;
					}
				}

				if (bbit % 6) jam_put_6bit_char(ch, &position, fp);
			}

			/*
			*	Write constant block
			*/

			/* calculate how many count characters are required */
			count_size = 0;
			mask = constant_block_length;
			while (mask != 0)
			{
				++count_size;
				mask >>= 6;
			}

			jam_put_char('A' + (run_value << 3) + count_size - 1, &position, fp);

			for (count_pos = count_size - 1; count_pos >= 0; --count_pos)
			{
				jam_put_6bit_char((constant_block_length >>
					(count_pos * 6)) & 0x3f, &position, fp);
			}

			start_random_block = end_constant_block;
			bit = end_constant_block - 1;
		}
	}

	/*
	*	Now write the remainder as a random block
	*/
	random_block_length = bit_count - start_random_block;

	if (random_block_length > 0)
	{
		/* calculate how many count characters are required */
		count_size = 0;
		j = random_block_length;
		while (j != 0)
		{
			++count_size;
			j >>= 6;
		}

		/* write block header */
		jam_put_char('Q' + count_size - 1, &position, fp);
		for (count_pos = count_size - 1; count_pos >= 0; --count_pos)
		{
			jam_put_6bit_char((random_block_length >>
				(count_pos * 6)) & 0x3f, &position, fp);
		}

		/* write block data */
		ch = 0;
		i = 0L;
		for (j = start_random_block; j < bit; ++j, ++i)
		{
			ch |= (in_buffer[j >> 3] & (1 << (j & 7)))
				? (1 << (i % 6)) : 0;

			if ((i % 6) == 5)
			{
				jam_put_6bit_char(ch, &position, fp);
				ch = 0;
			}
		}

		if (i % 6) jam_put_6bit_char(ch, &position, fp);
	}

	if (position != 0) fputc('\n', fp);

	return (TRUE);	/* always succeeds */
}

/****************************************************************************/
/*																			*/

BOOL jam_encode_aca(char *in_buffer, long in_length, FILE *fp)

/*																			*/
/*	Description:	Converts binary data to ACA format, writes to output	*/
/*					stream (text characters)								*/
/*																			*/
/*	Returns:		TRUE for success, FALSE for failure						*/
/*																			*/
/****************************************************************************/
{
	long out_length = 0L;
	long bit = 0L;
	char *out_buffer = NULL;
	int ch = 0;
	int position = 0;
	BOOL status = FALSE;

	if ((out_buffer = (char *) calloc(1, (size_t) in_length)) == NULL)
	{
		fprintf(stderr, "Error: can't allocate memory (%d Kbytes)\n",
			(int) (in_length / 1024L));
	}
	else
	{
		out_length = jam_compress_aca(
			in_buffer, in_length, out_buffer, in_length);

		if ((out_length <= 0) || (out_length > in_length))
		{
			fprintf(stderr, "Error: ACA compression failed\n");
		}
		else
		{
			/* write out the compressed data */
			for (bit = 0L; bit < (out_length * 8L); ++bit)
			{
				ch |= (out_buffer[bit >> 3] & (1 << (bit & 7)))
					? (1 << (bit % 6)) : 0;

				if ((bit % 6) == 5)
				{
					jam_put_6bit_char(ch, &position, fp);
					ch = 0;
				}
			}

			if (bit % 6) jam_put_6bit_char(ch, &position, fp);
			if (position != 0) fputc('\n', fp);

			status = TRUE;
		}

		free((void *) out_buffer);
	}

	return (status);
}

/****************************************************************************/
/*																			*/

int main(int argc, char **argv)

/*																			*/
/*	Description:	Main function.  Processes command-line arguments, calls	*/
/*					appropriate data conversion function.					*/
/*																			*/
/*	Returns:		Exit code is 0 for success, 1 for failure				*/
/*																			*/
/****************************************************************************/
{
	int exit_code = 1;
	int format = 0;
	BOOL error = FALSE;
	BOOL help = FALSE;
	BOOL status = FALSE;
	BOOL encode = FALSE;
	BOOL decode = FALSE;
	char *in_filename = NULL;
	char *out_filename = NULL;
	char *in_buffer = NULL;
	char *format_option = NULL;
	long in_length = 0L;
	FILE *in_fp = NULL;
	FILE *out_fp = NULL;
	struct stat sbuf;
	int arg = 0;

	fprintf(stderr, "Jam Data Conversion Utility Version 1.0\nCopyright (C) 1997 Altera Corporation\n\n");

	for (arg = 1; (arg < argc) && !error; ++arg)
	{
		if (argv[arg][0] == '-')
		{
			if ((argv[arg][1] == 'd') || (argv[arg][1] == 'D'))
			{
				decode = TRUE;
			}
			else if ((argv[arg][1] == 'e') || (argv[arg][1] == 'E'))
			{
				encode = TRUE;
			}
			else if ((argv[arg][1] == 'f') || (argv[arg][1] == 'F'))
			{
				if (format_option == NULL)
				{
					format_option = &argv[arg][2];

					if (stricmp(format_option, "BIN") == 0)
					{
						format = 'B';
					}
					else if (stricmp(format_option, "HEX") == 0)
					{
						format = 'H';
					}
					else if (stricmp(format_option, "RLC") == 0)
					{
						format = 'R';
					}
					else if (stricmp(format_option, "ACA") == 0)
					{
						format = 'A';
					}
					else
					{
						fprintf(stderr, "Error: unknown format option: \"%s\"\n",
							argv[arg]);
						error = TRUE;
					}
				}
				else
				{
					fprintf(stderr, "Error: too many format options specified\n");
					error = TRUE;
				}
			}
			else if ((argv[1][1] == 'h') || (argv[1][1] == 'H'))
			{
				help = TRUE;
			}
			else
			{
				fprintf(stderr, "Error: illegal option: \"%s\"\n", argv[arg]);
				error = TRUE;
			}

			if (encode && decode)
			{
				fprintf(stderr, "Error: cannot encode and decode\n");
				error = TRUE;
			}
		}
		else if (in_filename == NULL)
		{
			/* first argument without '-' is input filename (mandatory) */
			in_filename = argv[arg];
		}
		else if (out_filename == NULL)
		{
			/* second argument without '-' is output filename (optional) */
			out_filename = argv[arg];
		}
		else
		{
			/* too many filenames */
			help = TRUE;
		}
	}

	if ((!help) && format_option == NULL)
	{
		fprintf(stderr, "Error: no data format specifed\n");
		error = TRUE;
	}

	if ((!help) && in_filename == NULL)
	{
		fprintf(stderr, "Error: no input file specifed\n");
		error = TRUE;
	}

	if ((!help) && decode && (out_filename == NULL))
	{
		fprintf(stderr, "Error: output filename must be specified for decode\n");
		error = TRUE;
	}

	if (error)
	{
		/*
		*	If error message was already given, add an extra line-feed before
		*	the usage message
		*/
		fprintf(stderr, "\n");
		help = TRUE;
	}

	if (help)
	{
		fprintf(stderr, "Usage:  jamdata [-h] [-d] [-e] -f<format> <input-file> [<output-file>]\n");
		fprintf(stderr, "        -h : help message\n");
		fprintf(stderr, "        -d : decode Jam data into a file\n");
		fprintf(stderr, "        -e : encode a file into Jam data\n");
		fprintf(stderr, "        -f : format may be BIN, HEX, RLC, or ACA.\n");
	}
	else
	{
		/* get length of file */
		if (stat(in_filename, &sbuf) != 0)
		{
			fprintf(stderr, "Can't open file: %s\n", in_filename);
		}
		else
		{
			in_length = sbuf.st_size;

			if ((in_buffer = (char *) calloc(1, (size_t) in_length)) == NULL)
			{
				fprintf(stderr, "Error: can't allocate memory (%d Kbytes)\n",
					(int) (in_length / 1024L));
			}
			else
			{
				if ((in_fp = fopen(in_filename, "rb")) == NULL)
				{
					fprintf(stderr, "Can't open file: %s\n", in_filename);
				}
				else
				{
					if (fread(in_buffer, 1, (size_t) in_length, in_fp) !=
						(size_t) in_length)
					{
						fclose(in_fp);
						fprintf(stderr, "Error reading file: %s\n", in_filename);
					}
					else
					{
						fclose(in_fp);

						if (out_filename == NULL)
						{
							out_fp = stdout;
						}
						else if (decode)
						{
							/* for decode, open output file in binary mode */
							if ((out_fp = fopen(out_filename, "wb")) == NULL)
							{
								fprintf(stderr, "Can't open file: %s\n", out_filename);
							}
						}
						else
						{
							/* for encode, open output file in text mode */
							if ((out_fp = fopen(out_filename, "wt")) == NULL)
							{
								fprintf(stderr, "Can't open file: %s\n", out_filename);
							}
						}
					}

					if (out_fp != NULL)
					{
						if (decode)
						{
							/*
							*	Call appropriate output function, depending
							*	on format selected
							*/
							if (format == 'B')
							{
								status = jam_decode_bin(in_buffer, in_length,
									out_fp);
							}
							else if (format == 'H')
							{
								status = jam_decode_hex(in_buffer, in_length,
									out_fp);
							}
							else if (format == 'R')
							{
								status = jam_decode_rlc(in_buffer, in_length,
									out_fp);
							}
							else if (format == 'A')
							{
								status = jam_decode_aca(in_buffer, in_length,
									out_fp);
							}

							if (status) exit_code = 0; /* success */
						}
						else	/* assume encode */
						{
							/*
							*	Call appropriate output function, depending
							*	on format selected
							*/
							if (format == 'B')
							{
								status = jam_encode_bin(in_buffer, in_length,
									out_fp);
							}
							else if (format == 'H')
							{
								status = jam_encode_hex(in_buffer, in_length,
									out_fp);
							}
							else if (format == 'R')
							{
								status = jam_encode_rlc(in_buffer, in_length,
									out_fp);
							}
							else if (format == 'A')
							{
								status = jam_encode_aca(in_buffer, in_length,
									out_fp);
							}

							if (status) exit_code = 0; /* success */
						}

						/* don't close stdout! */
						if (out_filename != NULL) fclose(out_fp);
					}
				}

				free((void *) in_buffer);
			}
		}
	}

	return (exit_code);
}
