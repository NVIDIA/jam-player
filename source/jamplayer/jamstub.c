/****************************************************************************/
/*																			*/
/*	Module:			jamstub.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	Main source file for stand-alone JAM test utility.		*/
/*																			*/
/*					Supports Altera ByteBlaster hardware download cable		*/
/*					on Windows 95 and Windows NT operating systems.			*/
/*					(A device driver is required for Windows NT.)			*/
/*																			*/
/****************************************************************************/

#ifndef NO_ALTERA_STDIO
#define NO_ALTERA_STDIO
#endif

/*
*	PORT defines the target platform -- should be DOS, WINDOWS, or UNIX
*
*	PORT = DOS     means a 16-bit DOS console-mode application
*
*	PORT = WINDOWS means a 32-bit WIN32 console-mode application for
*	               Windows 95 or Windows NT.  On NT this will use the
*	               DeviceIoControl() API to access the Parallel Port.
*
*	PORT = UNIX    means no ByteBlaster access.
*/
#ifndef DOS
#define DOS 1
#endif
#ifndef WINDOWS
#define WINDOWS 2
#endif
#ifndef UNIX
#define UNIX 3
#endif
#ifndef PORT
#define PORT WINDOWS
#endif

#if ( _MSC_VER >= 800 )
#pragma warning(disable:4115)
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#pragma warning(disable:4514)
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <malloc.h>
#include <time.h>
#include <conio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#if PORT == DOS
#include <bios.h>
#endif

#include "jamexprt.h"

#if PORT == WINDOWS
#define PGDC_IOCTL_GET_DEVICE_INFO_PP 0x00166A00L
#define PGDC_IOCTL_READ_PORT_PP       0x00166A04L
#define PGDC_IOCTL_WRITE_PORT_PP      0x0016AA08L
#define PGDC_IOCTL_PROCESS_LIST_PP    0x0016AA1CL
#define PGDC_READ_INFO                0x0a80
#define PGDC_READ_PORT                0x0a81
#define PGDC_WRITE_PORT               0x0a82
#define PGDC_PROCESS_LIST             0x0a87
#define PGDC_HDLC_NTDRIVER_VERSION    2
#define PORT_IO_BUFFER_SIZE           256
#endif

#if PORT == WINDOWS
#pragma intrinsic (inp, outp)
#endif

/************************************************************************
*
*	Global variables
*/

/* file buffer for JAM input file */
char *file_buffer = NULL;
long file_pointer = 0L;
long file_length = 0L;

/* delay count for one millisecond delay */
int one_ms_delay = 0;

#if PORT == WINDOWS
/* variables to manage cached I/O under Windows NT */
BOOL windows_nt = FALSE;
int port_io_count = 0;
HANDLE nt_device_handle = INVALID_HANDLE_VALUE;
struct PORT_IO_LIST_STRUCT
{
	USHORT command;
	USHORT data;
} port_io_buffer[PORT_IO_BUFFER_SIZE];
#endif

#if PORT == WINDOWS || PORT == DOS
BOOL jtag_hardware_initialized = FALSE;
BOOL specified_lpt_addr = FALSE;
int lpt_port = 1;
WORD lpt_addr = 0x3bc;
WORD lpt_addr_table[3] = { 0x3bc, 0x378, 0x278 };
#endif

/* function prototypes to allow forward reference */
extern void delay_loop(int count);

#if PORT == WINDOWS
extern void flush_ports(void);
BOOL initialize_nt_driver(void);
#endif

#if PORT == WINDOWS || PORT == DOS
void write_byteblaster(int port, int data);
int read_byteblaster(int port);
void initialize_jtag_hardware(void);
void close_jtag_hardware(void);
#endif

BOOL verbose = FALSE;

/************************************************************************
*
*	Customized interface functions for JAM interpreter I/O:
*
*	jam_getc()
*	jam_seek()
*	jam_jtag_io()
*	jam_message()
*	jam_delay()
*/

int jam_getc(void)
{
	int ch = EOF;

	if (file_pointer < file_length) ch = (int) file_buffer[file_pointer++];

	return (ch);
}

int jam_seek(long offset)
{
	int return_code = EOF;

	if ((offset >= 0L) && (offset < file_length))
	{
		file_pointer = offset;
		return_code = 0;
	}

	return (return_code);
}

#define TMS_BIT  0x01
#define TDI_BIT  0x02
#define READ_TDO 0x04

int jam_jtag_io(int tms_tdi)
{
#if PORT == WINDOWS || PORT == DOS
	int data = ((tms_tdi & TDI_BIT) << 5) | ((tms_tdi & TMS_BIT) << 1);
	int tdo = 0;

	if (!jtag_hardware_initialized)
	{
		initialize_jtag_hardware();
		jtag_hardware_initialized = TRUE;
	}

	write_byteblaster(0, data);

	if (tms_tdi & READ_TDO)
	{
		tdo = (read_byteblaster(1) & 0x80) ? 0 : 1;
	}

	write_byteblaster(0, data | 0x01);

	write_byteblaster(0, data);

	return (tdo);
#else
	tms_tdi = tms_tdi;

	return (0);
#endif
}

void jam_message(char *message_text)
{
	puts(message_text);
	fflush(stdout);
}

void jam_export(char *key, long value)
{
	if (verbose)
	{
		printf("Export: key = \"%s\", value = %ld\n", key, value);
		fflush(stdout);
	}
}

void jam_delay(long microseconds)
{
#if PORT == WINDOWS
	/* if Windows NT, flush I/O cache buffer before delay loop */
	if (windows_nt && (port_io_count > 0)) flush_ports();
#endif

	delay_loop((int) (microseconds *
		((one_ms_delay / 1000L) + ((one_ms_delay % 1000L) ? 1 : 0))));
}

int jam_vector_map
(
	int signal_count,
	char **signals
)
{
	/* ByteBlaster cannot support VMAP instruction */
	signal_count = signal_count;
	signals = signals;
	return (signal_count);
}

int jam_vector_io
(
	int signal_count,
	long *dir_vect,
	long *data_vect,
	long *capture_vect
)
{
	/* ByteBlaster cannot support VECTOR instruction */
	signal_count = signal_count;
	dir_vect = dir_vect;
	data_vect = data_vect;
	capture_vect = capture_vect;
	return (-1);
}

/************************************************************************
*
*	get_tick_count() -- Get system tick count in milliseconds
*
*	for DOS, use BIOS function _bios_timeofday()
*	for WINDOWS use GetTickCount() function
*	for UNIX use clock() system function
*/
DWORD get_tick_count()
{
    DWORD tick_count = 0L;

#if PORT == WINDOWS
	tick_count = GetTickCount();
#elif PORT == DOS
    _bios_timeofday(_TIME_GETCLOCK, (long *)&tick_count);
#else
	/* assume clock() function returns microseconds */
	tick_count = (DWORD) (clock() / 1000L);
#endif

    return (tick_count);
}

#define DELAY_SAMPLES 10
#define DELAY_CHECK_LOOPS 10000

void calibrate_delay()
{
	int sample = 0;
	int count = 0;
	DWORD tick_count1 = 0L;
	DWORD tick_count2 = 0L;

	one_ms_delay = 0;

#if PORT == WINDOWS || PORT == DOS
	for (sample = 0; sample < DELAY_SAMPLES; ++sample)
	{
		count = 0;
		tick_count1 = get_tick_count();
		while ((tick_count2 = get_tick_count()) == tick_count1) {};
		do { delay_loop(DELAY_CHECK_LOOPS); count++; } while
			((tick_count1 = get_tick_count()) == tick_count2);
		one_ms_delay += (int)((DELAY_CHECK_LOOPS * (DWORD)count) /
			(tick_count1 - tick_count2));
	}

	one_ms_delay /= DELAY_SAMPLES;
#else
	one_ms_delay = 1000;
#endif
}

char *error_text[] =
{
	"success",
	"out of memory",
	"file access error",
	"syntax error",
	"unexpected end of file",
	"undefined symbol",
	"redefined symbol",
	"integer overflow",
	"divide by zero",
	"CRC mismatch",
	"internal error",
	"bounds error",
	"type mismatch",
	"assignment to constant object",
	"NEXT statement unexpected",
	"POP statement unexpected",
	"RETURN statement unexpected",
	"illegal symbolic name",
	"instruction or feature is not supported"
};

#define MAX_ERROR_CODE (int)((sizeof(error_text)/sizeof(error_text[0]))+1)

/************************************************************************/

int main(int argc, char **argv)
{
	BOOL help = FALSE;
	BOOL error = FALSE;
	char *filename = NULL;
	long offset = 0L;
	long error_line = 0L;
	JAM_RETURN_TYPE crc_result = JAMC_SUCCESS;
	JAM_RETURN_TYPE exec_result = JAMC_SUCCESS;
	unsigned short expected_crc = 0;
	unsigned short actual_crc = 0;
	char key[33] = {0};
	char value[257] = {0};
	int exit_status = 0;
	int arg = 0;
	int exit_code = 0;
	time_t start_time = 0;
	time_t end_time = 0;
	int time_delta = 0;
	char *workspace = NULL;
	char *init_list[10];
	int init_count = 0;
	FILE *fp = NULL;
	struct stat sbuf;

#if PORT == DOS
	long workspace_size = 65500;	/* slightly less than 64 Kbytes */
#else
	long workspace_size = 256 * 1024;	/* default 256 Kbytes */
#endif

	verbose = FALSE;

	init_list[0] = NULL;

	/* print out the version string and copyright message */
	fprintf(stderr, "Jam Language Interpreter Version 1.0\nCopyright (C) 1997 Altera Corporation\n\n");

	for (arg = 1; arg < argc; arg++)
	{
#if PORT == UNIX
		if (argv[arg][0] == '-')
#else
		if ((argv[arg][0] == '-') || (argv[arg][0] == '/'))
#endif
		{
			switch(toupper(argv[arg][1]))
			{
			case 'D':				/* initialization list */
				if (argv[arg][2] == '"')
				{
					init_list[init_count] = &argv[arg][3];
				}
				else
				{
					init_list[init_count] = &argv[arg][2];
				}
				init_list[++init_count] = NULL;
				break;

			case 'P':				/* set LPT port address */
#if PORT == WINDOWS || PORT == DOS
				if (sscanf(&argv[arg][2], "%d", &lpt_port) != 1) error = TRUE;
				if ((lpt_port < 1) || (lpt_port > 3)) error = TRUE;
				if (error)
				{
					if (sscanf(&argv[arg][2], "%x", &lpt_port) == 1)
					{
						if ((lpt_port == 0x3bc) ||
							(lpt_port == 0x378) ||
							(lpt_port == 0x278))
						{
							error = FALSE;
							specified_lpt_addr = TRUE;
							lpt_addr = (WORD) lpt_port;
							lpt_port = 1;
						}
					}
				}
#endif
				break;
			case 'M':				/* set memory size */
				if (sscanf(&argv[arg][2], "%ld", &workspace_size) != 1)
					error = TRUE;
				break;
			case 'H':				/* help */
				help = TRUE;
				break;
			case 'V':				/* verbose */
				verbose = TRUE;
				break;
			default:
				error = TRUE;
				break;
			}
		}
		else
		{
			/* it's a filename */
			if (filename == NULL)
			{
				filename = argv[arg];
			}
			else
			{
				/* error -- we already found a filename */
				error = TRUE;
			}
		}

		if (error)
		{
			printf("Illegal argument: \"%s\"\n", argv[arg]);
			help = TRUE;
			error = FALSE;
		}
	}

	if (help || (filename == NULL))
	{
		printf("Usage: jam [-h] [-v] [-d<var=val>] [-p<port>] [-m<memsize>] <filename>\n");
		exit_status = 1;
	}
	else if ((workspace = (char *) malloc((size_t) workspace_size)) == NULL)
	{
		printf("Error: can't allocate memory (%d Kbytes)\n",
			(int) (workspace_size / 1024L));
		exit_status = 1;
	}
	else if (access(filename, 0) != 0)
	{
		printf("Error: can't access file \"%s\"\n", filename);
		exit_status = 1;
	}
	else
	{
		/* get length of file */
		if (stat(filename, &sbuf) == 0) file_length = sbuf.st_size;

		if ((fp = fopen(filename, "rb")) == NULL)
		{
			printf("Error: can't open file \"%s\"\n", filename);
			exit_status = 1;
		}
		else
		{
			/*
			*	Read entire file into a buffer
			*/
			file_buffer = (char *) malloc((size_t) file_length);

			if (file_buffer == NULL)
			{
				printf("Error: can't allocate memory (%d Kbytes)\n",
					(int) (file_length / 1024L));
				exit_status = 1;
			}
			else
			{
				if (fread(file_buffer, 1, (size_t) file_length, fp) !=
					(size_t) file_length)
				{
					printf("Error reading file \"%s\"\n", filename);
					exit_status = 1;
				}
			}

			fclose(fp);
		}

		if (exit_status == 0)
		{
			/*
			*	Get Operating System type
			*/
#if PORT == WINDOWS
			windows_nt = !(GetVersion() & 0x80000000);
#endif

			/*
			*	Calibrate the delay loop function
			*/
			calibrate_delay();

			/*
			*	Check CRC
			*/
			crc_result = jam_check_crc(&expected_crc, &actual_crc);

			if (verbose || (crc_result == JAMC_CRC_ERROR))
			{
				switch (crc_result)
				{
				case JAMC_SUCCESS:
					printf("CRC matched: CRC value = %04X\n", actual_crc);
					break;

				case JAMC_CRC_ERROR:
					printf("CRC mismatch: expected %04X, actual %04X\n",
						expected_crc, actual_crc);
					break;

				case JAMC_UNEXPECTED_END:
					printf("Expected CRC not found, actual CRC value = %04X\n",
						actual_crc);
					break;

				default:
					printf("CRC function returned error code %d\n", crc_result);
					break;
				}
			}

			/*
			*	Dump out NOTE fields
			*/
			if (verbose)
			{
				while (jam_get_note(&offset, key, value, 256) == 0)
				{
					printf("NOTE \"%s\" = \"%s\"\n", key, value);
				}
			}

			/*
			*	Execute the JAM program
			*/
			time(&start_time);
			exec_result = jam_execute(init_list, workspace, workspace_size,
				&error_line, &exit_code);
			time(&end_time);

			if (exec_result == JAMC_SUCCESS)
			{
				if (verbose)
				{
					printf("Successful conclusion: exit code = %d\n",
						exit_code);
				}
			}
			else if (exec_result < MAX_ERROR_CODE)
			{
				printf("Error on line %ld: %s.\nProgram terminated.\n",
					error_line, error_text[exec_result]);
			}
			else
			{
				printf("Unknown error code %ld\n", exec_result);
			}

			/*
			*	Print out elapsed time
			*/
			if (verbose)
			{
				time_delta = (int) (end_time - start_time);
				printf("Elapsed time = %02u:%02u:%02u\n",
					time_delta / 3600,			/* hours */
					(time_delta % 3600) / 60,	/* minutes */
					time_delta % 60);			/* seconds */
			}
		}
	}

#if PORT == WINDOWS || PORT == DOS
	if (jtag_hardware_initialized) close_jtag_hardware();
#endif

	if (workspace != NULL) free(workspace);
	if (file_buffer != NULL) free(file_buffer);

	return (exit_status);
}

#if PORT == WINDOWS || PORT == DOS
void initialize_jtag_hardware()
{
#if PORT == WINDOWS
	if (windows_nt)
	{
		initialize_nt_driver();
	}
	else if (!specified_lpt_addr)
	{
		lpt_addr = lpt_addr_table[lpt_port - 1];
	}
#else /* PORT == DOS */
	/*
	*	Read word at specific memory address to get the LPT port address
	*/
	WORD *bios_address = (WORD *) 0x00400008;

	if (!specified_lpt_addr)
	{
		lpt_addr = bios_address[lpt_port - 1];

		if ((lpt_port != 0x3bc) &&
			(lpt_port != 0x378) &&
			(lpt_port != 0x278))
		{
			lpt_addr = lpt_addr_table[lpt_port - 1];
		}
	}
#endif

	/* set AUTO-FEED low to enable ByteBlaster */
	write_byteblaster(2, 0xea);
}

void close_jtag_hardware()
{
	/* set AUTO-FEED high to disable ByteBlaster */
	write_byteblaster(2, 0xe8);

#if PORT == WINDOWS
	if (windows_nt && (nt_device_handle != INVALID_HANDLE_VALUE))
	{
		if (port_io_count > 0) flush_ports();

		CloseHandle(nt_device_handle);
	}
#endif
}

#if PORT == WINDOWS
/**************************************************************************/
/*                                                                        */

BOOL initialize_nt_driver()

/*                                                                        */
/*  Uses CreateFile() to open a connection to the Windows NT device       */
/*  driver.                                                               */
/*                                                                        */
/**************************************************************************/
{
	BOOL status = FALSE;

	ULONG buffer[1];
	ULONG returned_length = 0;
	char nt_lpt_str[] = { '\\', '\\', '.', '\\',
		'A', 'L', 'T', 'L', 'P', 'T', '1', '\0' };


	nt_lpt_str[10] = (char) ('1' + (lpt_port - 1));

	nt_device_handle = CreateFile(
		nt_lpt_str,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

    if (nt_device_handle != INVALID_HANDLE_VALUE)
	{
		if (DeviceIoControl(
			nt_device_handle,			/* Handle to device */
			PGDC_IOCTL_GET_DEVICE_INFO_PP,	/* IO Control code */
			(ULONG *)NULL,					/* Buffer to driver. */
			0,								/* Length of buffer in bytes. */
			&buffer,						/* Buffer from driver. */
			sizeof(ULONG),					/* Length of buffer in bytes. */
			&returned_length,				/* Bytes placed in data_buffer. */
			NULL))							/* Wait for operation to complete */
		{
			if (returned_length == sizeof(ULONG))
			{
				if (buffer[0] == PGDC_HDLC_NTDRIVER_VERSION)
				{
					status = TRUE;
				}
			}
		}

		if (!status)
		{
			CloseHandle(nt_device_handle);
			nt_device_handle = INVALID_HANDLE_VALUE;
		}
	}

	return (status);
}
#endif

/**************************************************************************/
/*                                                                        */

void write_byteblaster
(
	int port,
	int data
)

/*                                                                        */
/**************************************************************************/
{
#if PORT == WINDOWS
	BOOL status = FALSE;

	int returned_length = 0;
	int buffer[2];


	if (windows_nt)
	{
		/*
		*	On Windows NT, access hardware through device driver
		*/
		if (port == 0)
		{
			port_io_buffer[port_io_count].data = (USHORT) data;
			port_io_buffer[port_io_count].command = PGDC_WRITE_PORT;
			++port_io_count;

			if (port_io_count >= PORT_IO_BUFFER_SIZE) flush_ports();
		}
		else
		{
			if (port_io_count > 0) flush_ports();

			buffer[0] = port;
			buffer[1] = data;

			status = DeviceIoControl(
				nt_device_handle,			/* Handle to device */
				PGDC_IOCTL_WRITE_PORT_PP,	/* IO Control code for write */
				(ULONG *)&buffer,			/* Buffer to driver. */
				2 * sizeof(int),			/* Length of buffer in bytes. */
				(ULONG *)NULL,				/* Buffer from driver.  Not used. */
				0,							/* Length of buffer in bytes. */
				(ULONG *)&returned_length,	/* Bytes returned.  Should be zero. */
				NULL);						/* Wait for operation to complete */

			if ((!status) || (returned_length != 0))
			{
				fprintf(stderr, "I/O error:  Cannot access ByteBlaster hardware\n");
				CloseHandle(nt_device_handle);
				exit(1);
			}
		}
	}
	else
#endif
	{
		/*
		*	On Windows 95, access hardware directly
		*/
		outp((WORD)(port + lpt_addr), (WORD)data);
	}
}

/**************************************************************************/
/*                                                                        */

int read_byteblaster
(
	int port
)

/*                                                                        */
/**************************************************************************/
{
	int data = 0;

#if PORT == WINDOWS

	BOOL status = FALSE;

	int returned_length = 0;


	if (windows_nt)
	{
		/* flush output cache buffer before reading from device */
		if (port_io_count > 0) flush_ports();

		/*
		*	On Windows NT, access hardware through device driver
		*/
		status = DeviceIoControl(
			nt_device_handle,			/* Handle to device */
			PGDC_IOCTL_READ_PORT_PP,	/* IO Control code for Read */
			(ULONG *)&port,				/* Buffer to driver. */
			sizeof(int),				/* Length of buffer in bytes. */
			(ULONG *)&data,				/* Buffer from driver. */
			sizeof(int),				/* Length of buffer in bytes. */
			(ULONG *)&returned_length,	/* Bytes placed in data_buffer. */
			NULL);						/* Wait for operation to complete */

		if ((!status) || (returned_length != sizeof(int)))
		{
			fprintf(stderr, "I/O error:  Cannot access ByteBlaster hardware\n");
			CloseHandle(nt_device_handle);
			exit(1);
		}
	}
	else
#endif
	{
		/*
		*	On Windows 95, access hardware directly
		*/
		data = inp((WORD)(port + lpt_addr));
	}

	return (data & 0xff);
}

#if PORT == WINDOWS
void flush_ports(void)
{
	ULONG n_writes = 0L;
	BOOL status;

	status = DeviceIoControl(
		nt_device_handle,			/* handle to device */
		PGDC_IOCTL_PROCESS_LIST_PP,	/* IO control code */
		(LPVOID)port_io_buffer,		/* IN buffer (list buffer) */
		port_io_count * sizeof(struct PORT_IO_LIST_STRUCT),/* length of IN buffer in bytes */
		(LPVOID)port_io_buffer,	/* OUT buffer (list buffer) */
		port_io_count * sizeof(struct PORT_IO_LIST_STRUCT),/* length of OUT buffer in bytes */
		&n_writes,					/* number of writes performed */
		0);							/* wait for operation to complete */

	if ((!status) || ((port_io_count * sizeof(struct PORT_IO_LIST_STRUCT)) != n_writes))
	{
		fprintf(stderr, "I/O error:  Cannot access ByteBlaster hardware\n");
		CloseHandle(nt_device_handle);
		exit(1);
	}

	port_io_count = 0;
}
#endif /* PORT == WINDOWS */
#endif /* PORT == WINDOWS || PORT == DOS */

#if !defined (DEBUG)
#pragma optimize ("ceglt", off)
#endif

void delay_loop(int count)
{
	while (count != 0) count--;
}
