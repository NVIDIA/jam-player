
/* # line 15 "jamexp.y" */

/* #include <stdio.h> */
#include "jamexprt.h"
#include "jamdefs.h"
#include "jamexp.h"
#include "jamsym.h"
#include "jamheap.h"
#include "jamarray.h"
#include "jamutil.h"
#include "jamytab.h"


/* ------------- LEXER DEFINITIONS -----------------------------------------*/
/****************************************************************************/
/*																			*/
/*	Operation of GET_FIRST_CH, GET_NEXT_CH, UNGET_CH, and DELETE_CH:		*/
/*																			*/
/*	Call GET_FIRST_CH to read a character from mdl_lexer_fp and put it into	*/
/*	jam_ch and jam_token_buffer.											*/
/*																			*/
/*		jam_ch = first char													*/
/*		jam_token_buffer[0] = first char									*/
/*		jam_token_buffer[1] = '\0';											*/
/*		jam_token_buffer[2] = ?												*/
/*		jam_token_buffer[3] = ?												*/
/*																			*/
/*	Call GET_NEXT_CH to read a character from jam_lexer_fp, put it in		*/
/*	jam_ch, and append it to jam_token_buffer.								*/
/*																			*/
/*		jam_ch = second char												*/
/*		jam_token_buffer[0] = first char									*/
/*		jam_token_buffer[1] = second char									*/
/*		jam_token_buffer[2] = '\0';											*/
/*		jam_token_buffer[3] = ?												*/
/*																			*/
/*	Call UNGET_CH remove the last character from the buffer but leave it in	*/
/*	jam_ch and set a flag.  (The next call to GET_FIRST_CH will use jam_ch	*/
/*	as the first char of the token and clear the flag.)						*/
/*																			*/
/*		jam_ch = second char												*/
/*		jam_token_buffer[0] = first char									*/
/*		jam_token_buffer[1] = '\0';											*/
/*		jam_token_buffer[2] = ?												*/
/*		jam_token_buffer[3] = ?												*/
/*																			*/
/*	Call DELETE_CH to remove the last character from the buffer.  Use this	*/
/*	macro to discard the quotes surrounding a string, for example.  Unlike	*/
/*	UNGET_CH, the deleted character will not be reused.						*/
/*																			*/
/****************************************************************************/

#define	MAX_BUFFER_LENGTH	256
#define END_OF_STRING		-1

#define BOOL int
#define TRUE 1
#define FALSE 0

#define	GET_FIRST_CH \
	jam_token_buffer_index = 0; \
	GET_NEXT_CH;

#define	GET_NEXT_CH \
	CH = jam_parse_string[jam_strptr++]; \
	jam_token_buffer [jam_token_buffer_index++] = CH; \
	if (jam_token_buffer_index >= MAX_BUFFER_LENGTH) { \
		--jam_token_buffer_index; \
		--jam_strptr; \
	} \
	jam_token_buffer [jam_token_buffer_index] = '\0';

#define	UNGET_CH \
	jam_strptr--; \
	jam_token_buffer[--jam_token_buffer_index] = '\0';

#define	DELETE_CH	jam_token_buffer [--jam_token_buffer_index] = '\0'
#define	CH			jam_ch


/****************************************************************************/
/*																			*/
/*	Operation of BEGIN_MACHINE, END_MACHINE, and ACCEPT:					*/
/*																			*/
/*	BEGIN_MACHINE and END_MACHINE should be at the beginning the end of an	*/
/*	integer function.  Inside the function, define states of the machine	*/
/*	with normal C labels, and jump to states with normal C goto statements.	*/
/*	Use ACCEPT(token) to return an integer value token to the calling		*/
/*	routine.																*/
/*																			*/
/*		int foo (void)														*/
/*		{																	*/
/*			BEGIN_MACHINE;													*/
/*																			*/
/*			start:															*/
/*				if (whatever) goto next;									*/
/*				else goto start;											*/
/*																			*/
/*			next:															*/
/*				if (done) ACCEPT (a_token_id);								*/
/*				else goto start;											*/
/*																			*/
/*			END_MACHINE;													*/
/*		}																	*/
/*																			*/
/*	Be sure that there is an ACCEPT() or goto at the end of every state.	*/
/*	Otherwise, control will "flow" from one state to the next illegally.	*/
/*																			*/
/****************************************************************************/

#define	BEGIN_MACHINE	{int ret

#define	ACCEPT(token)	{ret = (token); goto accept;}

#define	END_MACHINE		accept: jam_token = ret; \
						}

struct {
	char *string;
	int length;
	int token;
} jam_keyword_table[] = {
	{ "&&",		2,	AND_TOK },
	{ "||",		2,	OR_TOK },
	{ "==",		2,	EQUALITY_TOK },
	{ "!=",		2,	INEQUALITY_TOK },
	{ ">",		2,	GREATER_TOK },
	{ "<",		2,	LESS_TOK },
	{ ">=",		2,	GREATER_EQ_TOK },
	{ "<=",		2,	LESS_OR_EQ_TOK },
	{ "<<",		2,	LEFT_SHIFT_TOK },
	{ ">>",		2,	RIGHT_SHIFT_TOK },
	{ "OR",		2,	OR_TOK },
	{ "AND",	3,	AND_TOK },
	{ "ABS",	3,	ABS_TOK },
	{ "LOG2",	4,	LOG2_TOK },
	{ "SQRT",	4,	SQRT_TOK },
	{ "CEIL",	4,	CIEL_TOK },
	{ "FLOOR",	5,	FLOOR_TOK }
};

#define NUM_KEYWORDS ((int) \
	(sizeof(jam_keyword_table) / sizeof(jam_keyword_table[0])))

char		jam_ch = '\0';		/* next character from input file */
int			jam_strptr = 0;
int			jam_token = 0;
char		jam_token_buffer[MAX_BUFFER_LENGTH];
int			jam_token_buffer_index;
char		jam_parse_string[MAX_BUFFER_LENGTH];
long		jam_parse_value = 0;

#define YYMAXDEPTH 300  /* This fixes a stack depth problem on  */
                        /* all platforms.                       */

#define YYMAXTLIST 25   /* Max valid next tokens for any state. */
                        /* If there are more, error reporting   */
                        /* will be incomplete.                  */

enum OPERATOR_TYPE
{
	ADD = 0,
	SUB,
	UMINUS,
	MULT,
	DIV,
	MOD,
	NOT,
	AND,
	OR,
	BITWISE_NOT,
	BITWISE_AND,
	BITWISE_OR,
	BITWISE_XOR,
	LEFT_SHIFT,
	RIGHT_SHIFT,
	EQUALITY,
	INEQUALITY,
	GREATER_THAN,
	LESS_THAN,
	GREATER_OR_EQUAL,
	LESS_OR_EQUAL,
	ABS,
	LOG2,
	SQRT,
	CIEL,
	FLOOR,
	ARRAY
};

typedef enum OPERATOR_TYPE OPERATOR_TYPE;

typedef struct EXP_STACK
{
  OPERATOR_TYPE		child_otype;
  JAME_EXPRESSION_TYPE type;
  long				val;
  long				loper;		/* left and right operands for DIV */
  long				roper;		/* we save it for CEIL/FLOOR's use */
} EXPN_STACK;

#define YYSTYPE EXPN_STACK		/* must be a #define for yacc */

YYSTYPE jam_null_expression= {0,0,0,0,0};

JAM_RETURN_TYPE jam_return_code = JAMC_SUCCESS;

JAME_EXPRESSION_TYPE jam_expr_type = JAM_ILLEGAL_EXPR_TYPE;

#define NULL_EXP jam_null_expression  /* .. for 1 operand operators */

#define CALC(operator, lval, rval) jam_exp_eval((operator), (lval), (rval))

/* --- FUNCTION PROTOTYPES -------------------------------------------- */

int jam_yyparse();

#define AND_TOK 257
#define OR_TOK 258
#define EQUALITY_TOK 259
#define INEQUALITY_TOK 260
#define GREATER_TOK 261
#define LESS_TOK 262
#define GREATER_EQ_TOK 263
#define LESS_OR_EQ_TOK 264
#define LEFT_SHIFT_TOK 265
#define RIGHT_SHIFT_TOK 266
#define ABS_TOK 267
#define LOG2_TOK 268
#define SQRT_TOK 269
#define CIEL_TOK 270
#define FLOOR_TOK 271
#define VALUE_TOK 272
#define IDENTIFIER_TOK 273
#define ARRAY_TOK 274
#define ERROR_TOK 275
#define UNARY_MINUS 276
#define UNARY_PLUS 277
#ifndef YYSTYPE
#define YYSTYPE int
#endif
YYSTYPE jam_yylval, jam_yyval;
#define YYERRCODE 256

/* # line 311 "jamexp.y" */



/************************************************************************/
/*																   		*/

long jam_exponentiate(long x, long y)

/*	Calculate x^y in logarithmic time wrt y.					   		*/
/*																   		*/
{
	long retval = 1;
	long partial, exponent;

	partial = x;
	exponent = y;
	while (exponent > 0)
	{
		while ( ((exponent % 2) == 0) &&
				exponent != 0)
		{
			partial = partial * partial;
			exponent = exponent / 2;
		}
		exponent = exponent - 1;
		retval = retval * partial;
	}

	return(retval);
}


/************************************************************************/
/*																   		*/
long jam_square_root(long num)
{
	long sqrt = num;
	long a_squared = 0L;
	long b_squared = 0L;
	long two_ab = 0L;
	long square = 0L;
	int order = 0;

	if (num < 0L) sqrt = 0L;

	while (sqrt > 0L)
	{
		sqrt >>= 2L;
		++order;
	}

	while (order >= 0)
	{
		/* (a+b)^2 = a^2 + 2ab + b^2 */
		/* a is bit being tested, b is previous result */

		a_squared = 1L << (order << 1);

		two_ab = sqrt << (order + 1);

		/* b_squared starts out at zero */

		square = (a_squared + two_ab + b_squared);

		if (square <= num)
		{
			sqrt |= (1 << order);
			b_squared = square;
		}

		--order;
	}

	return (sqrt);
}


/************************************************************************/
/*																   		*/

YYSTYPE jam_exp_eval(OPERATOR_TYPE otype, YYSTYPE op1, YYSTYPE op2)

/*	Evaluate op1 OTYPE op2.  op1, op2 are operands, OTYPE is operator   */
/*																   		*/
/*	Some sneaky things are done to implement CEIL and FLOOR.	   		*/
/*																   		*/
/*	We do CEIL of LOG2 by default, and FLOOR of a DIVIDE by default.	*/
/*	Since we are lazy and we don't want to generate a parse tree,  		*/
/*	we use the parser's reduce actions to tell us when to perform  		*/
/*	an evaluation. But when CEIL and FLOOR are reduced, we know    		*/
/*	nothing about the expression tree beneath it (it's been reduced!)   */
/*																   		*/
/*	We keep this information around so we can calculate the CEIL or		*/
/*  FLOOR. We save value of the operand(s) or a divide in loper and		*/
/*  roper, then when CEIL/FLOOR get reduced, we just look at their      */
/*	values. 													   		*/
/*																   		*/
{
	YYSTYPE rtn;
	long	tmp_val;
	JAMS_SYMBOL_RECORD *symbol_rec;

	rtn.child_otype = 0;
	rtn.type = JAM_ILLEGAL_EXPR_TYPE;
	rtn.val = 0;
	rtn.loper = 0;
	rtn.roper = 0;

	switch (otype)
	{
		case UMINUS:
			if ((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR))
			{
				rtn.val = -1 * op1.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case ADD:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val + op2.val;
				rtn.type = JAM_INTEGER_EXPR;

				/* check for overflow */
				if (((op1.val > 0) && (op2.val > 0) && (rtn.val < 0)) ||
					((op1.val < 0) && (op2.val < 0) && (rtn.val > 0)))
				{
					jam_return_code = JAMC_INTEGER_OVERFLOW;
				}
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case SUB:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val - op2.val;
				rtn.type = JAM_INTEGER_EXPR;

				/* check for overflow */
				if (((op1.val > 0) && (op2.val < 0) && (rtn.val < 0)) ||
					((op1.val < 0) && (op2.val > 0) && (rtn.val > 0)))
				{
					jam_return_code = JAMC_INTEGER_OVERFLOW;
				}
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case MULT:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val * op2.val;
				rtn.type = JAM_INTEGER_EXPR;

				/* check for overflow */
				if ((op1.val != 0) && (op2.val != 0) &&
					(((rtn.val / op1.val) != op2.val) ||
					((rtn.val / op2.val) != op1.val)))
				{
					jam_return_code = JAMC_INTEGER_OVERFLOW;
				}
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case DIV:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				if (op2.val != 0)
				{
					rtn.val = op1.val / op2.val;
					rtn.loper = op1.val;
					rtn.roper = op2.val;
					rtn.child_otype = DIV;	/* Save info needed by CEIL */
					rtn.type = JAM_INTEGER_EXPR;
				}
				else
				{
					jam_return_code = JAMC_DIVIDE_BY_ZERO;
				}
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case MOD:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val % op2.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case NOT:
			if ((op1.type == JAM_BOOLEAN_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR))
			{
				rtn.val = (op1.val == 0) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case AND:
			if (((op1.type == JAM_BOOLEAN_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_BOOLEAN_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val && op2.val) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case OR:
			if (((op1.type == JAM_BOOLEAN_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_BOOLEAN_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val || op2.val) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case BITWISE_NOT:
			if ((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR))
			{
				rtn.val = ~ (unsigned long) op1.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case BITWISE_AND:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val & op2.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case BITWISE_OR:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val | op2.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case BITWISE_XOR:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val ^ op2.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case LEFT_SHIFT:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val << op2.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case RIGHT_SHIFT:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = op1.val >> op2.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case EQUALITY:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val == op2.val) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else if (((op1.type == JAM_BOOLEAN_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_BOOLEAN_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = ((op1.val && op2.val) || ((!op1.val) && (!op2.val)))
						? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case INEQUALITY:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val == op2.val) ? 0 : 1;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else if (((op1.type == JAM_BOOLEAN_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_BOOLEAN_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = ((op1.val && op2.val) || ((!op1.val) && (!op2.val)))
						? 0 : 1;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case GREATER_THAN:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val > op2.val) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case LESS_THAN:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val < op2.val) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case GREATER_OR_EQUAL:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val >= op2.val) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case LESS_OR_EQUAL:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				rtn.val = (op1.val <= op2.val) ? 1 : 0;
				rtn.type = JAM_BOOLEAN_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case ABS:
			if ((op1.type == JAM_INTEGER_EXPR) ||
				(op1.type == JAM_INT_OR_BOOL_EXPR))
			{
				rtn.val = (op1.val < 0) ? (0 - op1.val) : op1.val;
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case LOG2:
			if ((op1.type == JAM_INTEGER_EXPR) ||
				(op1.type == JAM_INT_OR_BOOL_EXPR))
			{
				if (op1.val > 0)
				{
					rtn.child_otype = LOG2;
					rtn.type = JAM_INTEGER_EXPR;
					rtn.loper = op1.val;
					tmp_val = op1.val;
					rtn.val = 0;

					while (tmp_val != 1)	/* ret_val = log2(left_val) */
					{
						tmp_val = tmp_val >> 1;
						++rtn.val;
					}

					/* if 2^(return_val) isn't the left_val, then the log */
					/* wasn't a perfect integer, so we increment it */
					if (jam_exponentiate(2, rtn.val) != op1.val)
					{
						++rtn.val;   /* Assume ceil of log2 */
					}
				}
				else
				{
					jam_return_code = JAMC_INTEGER_OVERFLOW;
				}
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case SQRT:
			if ((op1.type == JAM_INTEGER_EXPR) ||
				(op1.type == JAM_INT_OR_BOOL_EXPR))
			{
				if (op1.val >= 0)
				{
					rtn.child_otype = SQRT;
					rtn.type = JAM_INTEGER_EXPR;
					rtn.loper = op1.val;
					rtn.val = jam_square_root(op1.val);
				}
				else
				{
					jam_return_code = JAMC_INTEGER_OVERFLOW;
				}
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case CIEL:
			if ((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR))
			{
				if (op1.child_otype == DIV)
				{
					/* Below is TRUE if wasn't perfect divide */
					if ((op1.loper * op1.roper) != op1.val)
					{
						rtn.val = op1.val + 1; /* add 1 to get CEIL */
					}
					else
					{
						rtn.val = op1.val;
					}
				}
				else if (op1.child_otype == SQRT)
				{
					/* Below is TRUE if wasn't perfect square-root */
					if ((op1.val * op1.val) < op1.loper)
					{
						rtn.val = op1.val + 1; /* add 1 to get CEIL */
					}
					else
					{
						rtn.val = op1.val;
					}
				}
				else
				{
					rtn.val = op1.val;
				}
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case FLOOR:
			if (((op1.type == JAM_INTEGER_EXPR) || (op1.type == JAM_INT_OR_BOOL_EXPR)) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				if (op1.child_otype == LOG2)
				{
					if (jam_exponentiate(2, op1.val) != op1.loper)
					{
						rtn.val = op1.val - 1;
					}
					else
					{
						rtn.val = op1.val;
					}
				}
				else
				{
					rtn.val = op1.val;
				}
				rtn.type = JAM_INTEGER_EXPR;
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		case ARRAY:
			if ((op1.type == JAM_ARRAY_REFERENCE) &&
				((op2.type == JAM_INTEGER_EXPR) || (op2.type == JAM_INT_OR_BOOL_EXPR)))
			{
				symbol_rec = (JAMS_SYMBOL_RECORD *)op1.val;
				jam_return_code = jam_get_array_value(
					symbol_rec, op2.val, &rtn.val);

				if (jam_return_code == JAMC_SUCCESS)
				{
					switch (symbol_rec->type)
					{
					case JAM_INTEGER_ARRAY_WRITABLE:
					case JAM_INTEGER_ARRAY_INITIALIZED:
						rtn.type = JAM_INTEGER_EXPR;
						break;

					case JAM_BOOLEAN_ARRAY_WRITABLE:
					case JAM_BOOLEAN_ARRAY_INITIALIZED:
						rtn.type = JAM_BOOLEAN_EXPR;
						break;

					default:
						jam_return_code = JAMC_INTERNAL_ERROR;
						break;
					}
				}
			}
			else jam_return_code = JAMC_TYPE_MISMATCH;
			break;

		default:
			jam_return_code = JAMC_INTERNAL_ERROR;
			break;
	}

	return rtn;
}


/****************************************************************************/
/*																			*/

void jam_exp_lexer (void)

/*																			*/
/*	Description:	Lexical analyzer for expressions.                  		*/
/*																			*/
/*					Results are returned in the global variables jam_token.	*/
/*					and jam_token_buffer.                                	*/
/*					                            							*/
/*	References:		Compilers: Principles, Techniques and Tools by ASU      */
/*					(the Green Dragon book), section 3.4, Recognition of    */
/*					tokens.                                                 */
/*																			*/
/*	Returns:		Nothing													*/
/*																			*/
/****************************************************************************/
{
	BEGIN_MACHINE;

	start:
		GET_FIRST_CH;
		if (CH == '\0') ACCEPT(END_OF_STRING)			/* Fake an EOF! */
		else if (CH == ' ' || jam_iscntrl(CH)) goto start;  /* white space */
		else if (jam_isalpha(CH)) goto identifier;
   		else if (jam_isdigit(CH)) goto number;
		else if (CH == '&')
		{
			GET_NEXT_CH;
			if (CH == '&') ACCEPT(AND_TOK)
			else
			{
				UNGET_CH;
				ACCEPT('&')
			}
		}
		else if (CH == '|')
		{
			GET_NEXT_CH;
			if (CH == '|') ACCEPT(OR_TOK)
			else
			{
				UNGET_CH;
				ACCEPT('|')
			}
		}
		else if (CH == '=')
		{
			GET_NEXT_CH;
			if (CH == '=') ACCEPT(EQUALITY_TOK)
			else
			{
				UNGET_CH;
				ACCEPT('=')
			}
		}
		else if (CH == '!')
		{
			GET_NEXT_CH;
			if (CH == '=') ACCEPT(INEQUALITY_TOK)
			else
			{
				UNGET_CH;
				ACCEPT('!')
			}
		}
		else if (CH == '>')
		{
			GET_NEXT_CH;
			if (CH == '=') ACCEPT(GREATER_EQ_TOK)
			else if (CH == '>') ACCEPT(RIGHT_SHIFT_TOK)
			else
			{
				UNGET_CH;
				ACCEPT(GREATER_TOK)
			}
		}
		else if (CH == '<')
		{
			GET_NEXT_CH;
			if (CH == '=') ACCEPT(LESS_OR_EQ_TOK)
			else if (CH == '<') ACCEPT(LEFT_SHIFT_TOK)
			else
			{
				UNGET_CH;
				ACCEPT(LESS_TOK)
			}
		}
		else ACCEPT(CH)  /* single-chararcter token */

	number:
		GET_NEXT_CH;
		if (jam_isdigit(CH)) goto number;
		else if (jam_isalpha(CH) || CH == '_') goto identifier;
		else
		{
			UNGET_CH;
			ACCEPT(VALUE_TOK)
		}

	identifier:
		GET_NEXT_CH;
		if (jam_isalnum(CH) || CH == '_') goto identifier;
		else
		{
			UNGET_CH;
			ACCEPT(IDENTIFIER_TOK)
		}

	END_MACHINE;
}


/************************************************************************/
/*																   		*/

BOOL jam_constant_is_ok(char *string)

/*	This routine returns TRUE if the value represented by string is		*/
/*	a valid signed decimal number.								   		*/
/*																   		*/
{
	BOOL ok = TRUE;

	/* check for negative number */
	if ((string[0] == '-') && (jam_isdigit(string[1])))
	{
		++string;	/* skip over negative sign */
	}

	while (ok && (*string != '\0'))
	{
		if (!jam_isdigit(*string)) ok = FALSE;
		++string;
	}

	return (ok);
}


/************************************************************************/
/*																   		*/

BOOL jam_constant_value(char *string, long *value)

/*                                                                      */
/*		This routine converts a string constant into its binary			*/
/*		value.															*/
/*																		*/
/*      Returns TRUE for success, FALSE if the string could not be		*/
/*      converted.														*/
/*                                                                      */
{
	BOOL status = FALSE;

	if (jam_constant_is_ok(string))
	{
		if (string[0] == '-')
		{
			*value = 0 - jam_atol(&string[1]);
		}
		else
		{
			*value = jam_atol(string);
		}
		status = TRUE;
	}

	return (status);
}


/************************************************************************/
/*																   		*/

void jam_yyerror (char *msg)

/*																   		*/
/*	WARNING: When compiling for YACC 5.0 using err_skel.c,	     		*/
/*			 this function needs to be modified to be:  		   		*/
/*																   		*/
/*			 jam_yyerror(char *ms1, char *msg2) 					   		*/
/*																   		*/
/*	jam_yyerror() handles syntax error messages from the parser.	   		*/
/*	Since we don't care about anything else but reporting the error,	*/
/*	just flag the error in jam_return_code.						   		*/
/*																   		*/
{
	msg = msg; /* Avoid compiler warning about msg unused */
	jam_return_code = JAMC_SYNTAX_ERROR;
}


/************************************************************************/
/*																   		*/

int jam_yylex()

/*																   		*/
/*	This is the lexer function called by jam_yyparse(). It calls	   		*/
/*	jam_exp_lexer() to run as the DFA to return a token in jam_token	*/
/*																   		*/
{
	JAMS_SYMBOL_RECORD *symbol_rec = NULL;
	long val = 0L;
	JAME_EXPRESSION_TYPE type = JAM_ILLEGAL_EXPR_TYPE;
	int token_length;
	int i;

	jam_exp_lexer();

	token_length = jam_strlen(jam_token_buffer);

	if (token_length > 1)
	{
		for (i = 0; i < NUM_KEYWORDS; i++)
		{
			if (token_length == jam_keyword_table[i].length)
			{
				if (!jam_strcmp(jam_token_buffer, jam_keyword_table[i].string))
				{
					jam_token = jam_keyword_table[i].token;
				}
			}
		}
	}

	if (jam_token == VALUE_TOK)
	{
		if (jam_constant_value(jam_token_buffer, &val))
		{
			/* literal 0 and 1 may be interpreted as Integer or Boolean */
			if ((val == 0) || (val == 1))
			{
				type = JAM_INT_OR_BOOL_EXPR;
			}
			else
			{
				type = JAM_INTEGER_EXPR;
			}
		}
		else
		{
			jam_return_code = JAMC_SYNTAX_ERROR;
		}
	}
	else if (jam_token == IDENTIFIER_TOK)
	{
		symbol_rec = jam_get_symbol_record(jam_token_buffer);

		if (symbol_rec == NULL)
		{
			jam_return_code = JAMC_SYNTAX_ERROR;
		}
		else
		{
			switch (symbol_rec->type)
			{
			case JAM_INTEGER_SYMBOL:
				/* Success, swap token to be a VALUE */
				jam_token = VALUE_TOK;
				val = symbol_rec->value;
				type = JAM_INTEGER_EXPR;
				break;

			case JAM_BOOLEAN_SYMBOL:
				/* Success, swap token to be a VALUE */
				jam_token = VALUE_TOK;
				val = symbol_rec->value ? 1 : 0;
				type = JAM_BOOLEAN_EXPR;
				break;

			case JAM_INTEGER_ARRAY_WRITABLE:
			case JAM_BOOLEAN_ARRAY_WRITABLE:
			case JAM_INTEGER_ARRAY_INITIALIZED:
			case JAM_BOOLEAN_ARRAY_INITIALIZED:
				/* Success, swap token to be an ARRAY_TOK, */
				/* save pointer to symbol record in value field */
				jam_token = ARRAY_TOK;
				val = (long) symbol_rec;
				type = JAM_ARRAY_REFERENCE;
				break;

			default:
				jam_return_code = JAMC_SYNTAX_ERROR;
				break;
			}
		}
	}

	jam_yylval.val = val;
	jam_yylval.type = type;
	jam_yylval.child_otype = 0;
	jam_yylval.loper = 0;
	jam_yylval.roper = 0;

	return jam_token;
}


/************************************************************************/
/*																   		*/

JAM_RETURN_TYPE jam_evaluate_expression
(
	char *expression,
	long *result,
	JAME_EXPRESSION_TYPE *result_type
)

/*																   		*/
/*	THIS IS THE ENTRY POINT INTO THE EXPRESSION EVALUATOR.  	   		*/
/*																   		*/
/*	s = a string representing the expression to be evaluated.      		*/
/*		(e.g. "2+2+PARAMETER")  								   		*/
/*																   		*/
/*	status = for returning TRUE if evaluation was successful.      		*/
/*			 FALSE if not.  									   		*/
/*																   		*/
/*	This routine sets up the global variables and then calls jam_yyparse()  */
/*	to do the parsing. The reduce actions of the parser evaluate the	*/
/*	expression. 												   		*/
/*																   		*/
/*	RETURNS: Value of the expression if success. 0 if FAIL. 	   		*/
/*																   		*/
/*	Note: One should not rely on the return val to det.  success/fail   */
/*		  since it is possible for, say, "2-2" to be success and   		*/
/*		  return 0. 											   		*/
/*																   		*/
{
	jam_strcpy(jam_parse_string, expression);
	jam_strptr = 0;
	jam_token_buffer_index = 0;
	jam_return_code = JAMC_SUCCESS;

	jam_yyparse();

	if (jam_return_code == JAMC_SUCCESS)
	{
		if (result != 0) *result = jam_parse_value;
		if (result_type != 0) *result_type = jam_expr_type;
	}

	return (jam_return_code);
}
const int jam_yyexca[] = {
  -1, 1,
  0, -1,
  -2, 0,
  0,
};

#define YYNPROD 32
#define YYLAST 441

const int jam_yyact[] = {
       7,      43,      42,      19,      20,      41,       1,       4,
      17,      15,       5,      16,       6,      18,      19,      40,
      39,       0,      38,      17,       0,       0,      19,      20,
      18,       0,      73,      17,      15,       0,      16,       0,
      18,       0,      19,      20,       0,       0,      72,      17,
      15,       0,      16,       0,      18,      19,      20,       0,
       0,      71,      17,      15,       0,      16,       0,      18,
       0,      19,      20,      74,      22,      70,      17,      15,
       0,      16,       0,      18,      19,      20,       0,       0,
      69,      17,      15,       0,      16,      19,      18,      22,
       0,       0,      17,      15,       0,      16,       0,      18,
       0,       0,      21,      22,       0,       8,       0,      19,
      20,       0,       0,      62,      17,      15,      22,      16,
       0,      18,       0,      19,      20,      21,       0,       0,
      17,      15,      22,      16,       0,      18,      19,      20,
       0,      21,       0,      17,      15,      22,      16,       0,
      18,      19,      20,       0,      21,       0,      17,      15,
       0,      16,       0,      18,      19,      20,       0,       0,
      21,      17,      15,       0,      16,       0,      18,       0,
      22,      19,      20,      21,       0,       0,      17,      15,
      19,      16,       0,      18,      22,      17,      15,       0,
      16,       0,      18,      19,       0,       0,       0,      22,
      17,      15,       0,      16,       0,      18,      21,      19,
       0,       0,      22,       0,      17,      15,       0,      16,
       0,      18,      21,       0,       0,      22,       0,       0,
       0,       0,       0,       0,       0,      21,       0,       0,
       0,       0,       0,       0,       0,       0,       0,       0,
      21,       0,       0,       0,       0,       0,       0,      23,
      24,      27,      28,      29,      30,      31,      32,      25,
      26,       0,       9,      10,      11,      12,      13,       3,
       0,      14,      23,      24,      27,      28,      29,      30,
      31,      32,      25,      26,       0,       0,      23,      24,
      27,      28,      29,      30,      31,      32,      25,      26,
       0,      23,      24,      27,      28,      29,      30,      31,
      32,      25,      26,       0,       0,      23,      24,      27,
      28,      29,      30,      31,      32,      25,      26,       0,
      23,      24,      27,      28,      29,      30,      31,      32,
      25,      26,       0,      27,      28,      29,      30,      31,
      32,      25,      26,       0,       0,       0,       0,       0,
       0,       0,       0,      23,      24,      27,      28,      29,
      30,      31,      32,      25,      26,       0,       0,      23,
      24,      27,      28,      29,      30,      31,      32,      25,
      26,       0,      23,       0,      27,      28,      29,      30,
      31,      32,      25,      26,       0,       0,       0,      27,
      28,      29,      30,      31,      32,      25,      26,       0,
       0,       0,      27,      28,      29,      30,      31,      32,
      25,      26,       0,       0,       0,       0,       0,      27,
      28,      29,      30,      31,      32,      25,      26,       0,
      29,      30,      31,      32,      25,      26,       0,       0,
       0,       0,       0,       0,       2,       0,       0,      25,
      26,      33,      34,      35,      36,      37,       0,       0,
       0,       0,       0,       0,      44,      45,      46,      47,
      48,      49,      50,      51,      52,      53,      54,      55,
      56,      57,      58,      59,      60,      61,       0,       0,
       0,       0,       0,      63,      64,      65,      66,      67,
      68,
};

const int jam_yypact[] = {
     -33,   -1000,      70,   -1000,     -33,     -33,     -33,     -33,
     -33,     -22,     -24,     -25,     -35,     -38,     -90,     -33,
     -33,     -33,     -33,     -33,     -33,     -33,     -33,     -33,
     -33,     -33,     -33,     -33,     -33,     -33,     -33,     -33,
     -33,      58,   -1000,   -1000,   -1000,   -1000,     -33,     -33,
     -33,     -33,     -33,     -33,     -23,     -23,   -1000,   -1000,
   -1000,      40,     103,     116,      92,      81,     146,     146,
     123,     123,     134,     134,     134,     134,   -1000,      31,
      20,       8,      -3,     -15,     -34,   -1000,   -1000,   -1000,
   -1000,   -1000,   -1000,
};

const int jam_yypgo[] = {
       0,       6,     396,
};

const int jam_yyr1[] = {
       0,       1,       2,       2,       2,       2,       2,       2,
       2,       2,       2,       2,       2,       2,       2,       2,
       2,       2,       2,       2,       2,       2,       2,       2,
       2,       2,       2,       2,       2,       2,       2,       2,
};

const int jam_yyr2[] = {
       0,       1,       1,       3,       2,       2,       2,       2,
       3,       3,       3,       3,       3,       3,       3,       3,
       3,       3,       3,       3,       3,       3,       3,       3,
       3,       3,       4,       4,       4,       4,       4,       4,
};

const int jam_yychk[] = {
   -1000,      -1,      -2,     272,      40,      43,      45,      33,
     126,     267,     268,     269,     270,     271,     274,      43,
      45,      42,      47,      37,      38,     124,      94,     257,
     258,     265,     266,     259,     260,     261,     262,     263,
     264,      -2,      -2,      -2,      -2,      -2,      40,      40,
      40,      40,      40,      91,      -2,      -2,      -2,      -2,
      -2,      -2,      -2,      -2,      -2,      -2,      -2,      -2,
      -2,      -2,      -2,      -2,      -2,      -2,      41,      -2,
      -2,      -2,      -2,      -2,      -2,      41,      41,      41,
      41,      41,      93,
};

const int jam_yydef[] = {
       0,      -2,       1,       2,       0,       0,       0,       0,
       0,       0,       0,       0,       0,       0,       0,       0,
       0,       0,       0,       0,       0,       0,       0,       0,
       0,       0,       0,       0,       0,       0,       0,       0,
       0,       0,       4,       5,       6,       7,       0,       0,
       0,       0,       0,       0,       8,       9,      10,      11,
      12,      13,      14,      15,      16,      17,      18,      19,
      20,      21,      22,      23,      24,      25,       3,       0,
       0,       0,       0,       0,       0,      26,      27,      28,
      29,      30,      31,
};

/****************************************************************************/
/*																			*/
/*	Module:			jamycskl.c												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	LALR parser driver skeleton file -- used by YACC		*/
/*																			*/
/****************************************************************************/


#ifndef INITIALIZE
#define INITIALIZE
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 200 /* default stack depth */
#endif

#ifndef jam_yyerrok
#define jam_yyerrok ((int) 0)
#endif

#define YYFLAG -1000
#define YYERROR goto jam_yyerrlab
#define YYACCEPT return(0)
#define YYABORT return(1)

YYSTYPE jam_yyv[YYMAXDEPTH];
int token = -1; /* input token */
int errct = 0;  /* error count */
int errfl = 0;  /* error flag */

jam_yyparse()
{ int jam_yys[YYMAXDEPTH];
  int jam_yyj, jam_yym;
  YYSTYPE *jam_yypvt;
  int jam_yystate, *jam_yyps, jam_yyn;
  
  const int *jam_yyxi;

  YYSTYPE *jam_yypv;

  jam_yystate = 0;
  token = -1;
  errct = 0;
  errfl = 0;
  jam_yyps= &jam_yys[-1];
  jam_yypv= &jam_yyv[-1];


 jam_yystack:    /* put a state and value onto the stack */

  if( ++jam_yyps> &jam_yys[YYMAXDEPTH] ) { jam_yyerror( "yacc stack overflow" ); return(1); }
    *jam_yyps = jam_yystate;
    ++jam_yypv;
    *jam_yypv = jam_yyval;

  jam_yynewstate:

    jam_yyn = jam_yypact[jam_yystate];

    if( jam_yyn<= YYFLAG ) goto jam_yydefault; /* simple state */

    if( token<0 ) if( (token=jam_yylex())<0 ) token=0;
    if( (jam_yyn += token)<0 || jam_yyn >= YYLAST ) goto jam_yydefault;

    if( jam_yychk[ jam_yyn=jam_yyact[ jam_yyn ] ] == token ){ /* valid shift */
      token = -1;
      jam_yyval = jam_yylval;
      jam_yystate = jam_yyn;
      if( errfl > 0 ) --errfl;
      goto jam_yystack;
    }

 jam_yydefault:

    if( (jam_yyn=jam_yydef[jam_yystate]) == -2 ) {
      if( token<0 ) if( (token=jam_yylex())<0 ) token = 0;
      /* look through exception table */

      for( jam_yyxi=jam_yyexca; (*jam_yyxi!= (-1)) || (jam_yyxi[1]!=jam_yystate) ; jam_yyxi += 2 ) ; /* VOID */

      while( *(jam_yyxi+=2) >= 0 ){
        if( *jam_yyxi == token ) break;
      }
      if( (jam_yyn = jam_yyxi[1]) < 0 ) return(0);   /* accept */
    }

    if( jam_yyn == 0 ){ /* error */

      switch( errfl ){
      	case 0:   /* brand new error */
          jam_yyerror( "syntax error" );
      	  /* jam_yyerrlab: */
          ++errct;

       	case 1:
	  case 2: /* incompletely recovered error ... try again */
	    errfl = 3;

	    /* find a state where "error" is a legal shift action */

	    while ( jam_yyps >= jam_yys ) {
	      jam_yyn = jam_yypact[*jam_yyps] + YYERRCODE;
	      if( jam_yyn>= 0 && jam_yyn < YYLAST && jam_yychk[jam_yyact[jam_yyn]] == YYERRCODE ){
                jam_yystate = jam_yyact[jam_yyn];  /* simulate a shift of "error" */
	        goto jam_yystack;
	      }
	      jam_yyn = jam_yypact[*jam_yyps];
	      /* the current jam_yyps has no shift onn "error", pop stack */
	      --jam_yyps;
	      --jam_yypv;
	    }

	    /* there is no state on the stack with an error shift ... abort */

	jam_yyabort:
	    return(1);
	    case 3:  /* no shift yet; clobber input char */

	    if( token == 0 ) goto jam_yyabort; /* don't discard EOF, quit */
	      token = -1;
	      goto jam_yynewstate;   /* try again in the same state */
	    }

	  }

          /* reduction by production jam_yyn */

	  jam_yyps -= jam_yyr2[jam_yyn];
	  jam_yypvt = jam_yypv;
	  jam_yypv -= jam_yyr2[jam_yyn];
	  jam_yyval = jam_yypv[1];
	  jam_yym=jam_yyn;
	  /* consult goto table to find next state */
	  jam_yyn = jam_yyr1[jam_yyn];
	  jam_yyj = jam_yypgo[jam_yyn] + *jam_yyps + 1;
	  if( jam_yyj>=YYLAST || jam_yychk[ jam_yystate = jam_yyact[jam_yyj] ] != -jam_yyn ) jam_yystate = jam_yyact[jam_yypgo[jam_yyn]];
	    switch(jam_yym){
	    	
case 1:
/* # line 275 "jamexp.y" */
{jam_parse_value = jam_yypvt[-0].val; jam_expr_type = jam_yypvt[-0].type;} break;
case 3:
/* # line 280 "jamexp.y" */
{jam_yyval = jam_yypvt[-1];} break;
case 4:
/* # line 281 "jamexp.y" */
{jam_yyval = jam_yypvt[-0];} break;
case 5:
/* # line 282 "jamexp.y" */
{jam_yyval = CALC(UMINUS, jam_yypvt[-0], NULL_EXP);} break;
case 6:
/* # line 283 "jamexp.y" */
{jam_yyval = CALC(NOT, jam_yypvt[-0], NULL_EXP);} break;
case 7:
/* # line 284 "jamexp.y" */
{jam_yyval = CALC(BITWISE_NOT, jam_yypvt[-0], NULL_EXP);} break;
case 8:
/* # line 285 "jamexp.y" */
{jam_yyval = CALC(ADD, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 9:
/* # line 286 "jamexp.y" */
{jam_yyval = CALC(SUB, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 10:
/* # line 287 "jamexp.y" */
{jam_yyval = CALC(MULT, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 11:
/* # line 288 "jamexp.y" */
{jam_yyval = CALC(DIV, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 12:
/* # line 289 "jamexp.y" */
{jam_yyval = CALC(MOD, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 13:
/* # line 290 "jamexp.y" */
{jam_yyval = CALC(BITWISE_AND, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 14:
/* # line 291 "jamexp.y" */
{jam_yyval = CALC(BITWISE_OR, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 15:
/* # line 292 "jamexp.y" */
{jam_yyval = CALC(BITWISE_XOR, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 16:
/* # line 293 "jamexp.y" */
{jam_yyval = CALC(AND, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 17:
/* # line 294 "jamexp.y" */
{jam_yyval = CALC(OR, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 18:
/* # line 295 "jamexp.y" */
{jam_yyval = CALC(LEFT_SHIFT, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 19:
/* # line 296 "jamexp.y" */
{jam_yyval = CALC(RIGHT_SHIFT, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 20:
/* # line 297 "jamexp.y" */
{jam_yyval = CALC(EQUALITY, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 21:
/* # line 298 "jamexp.y" */
{jam_yyval = CALC(INEQUALITY, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 22:
/* # line 299 "jamexp.y" */
{jam_yyval = CALC(GREATER_THAN, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 23:
/* # line 300 "jamexp.y" */
{jam_yyval = CALC(LESS_THAN, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 24:
/* # line 301 "jamexp.y" */
{jam_yyval = CALC(GREATER_OR_EQUAL, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 25:
/* # line 302 "jamexp.y" */
{jam_yyval = CALC(LESS_OR_EQUAL, jam_yypvt[-2], jam_yypvt[-0]);} break;
case 26:
/* # line 303 "jamexp.y" */
{jam_yyval = CALC(ABS, jam_yypvt[-1], NULL_EXP);} break;
case 27:
/* # line 304 "jamexp.y" */
{jam_yyval = CALC(LOG2, jam_yypvt[-1], NULL_EXP);} break;
case 28:
/* # line 305 "jamexp.y" */
{jam_yyval = CALC(SQRT, jam_yypvt[-1], NULL_EXP);} break;
case 29:
/* # line 306 "jamexp.y" */
{jam_yyval = CALC(CIEL, jam_yypvt[-1], NULL_EXP);} break;
case 30:
/* # line 307 "jamexp.y" */
{jam_yyval = CALC(FLOOR, jam_yypvt[-1], NULL_EXP);} break;
case 31:
/* # line 308 "jamexp.y" */
{jam_yyval = CALC(ARRAY, jam_yypvt[-3], jam_yypvt[-1]);} break;
	    }
	    goto jam_yystack;  /* stack new state and value */

	}
