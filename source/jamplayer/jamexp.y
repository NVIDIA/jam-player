/****************************************************************************/
/*																			*/
/*	Module:			jamexp.y												*/
/*																			*/
/*					Copyright (C) Altera Corporation 1997					*/
/*																			*/
/*	Description:	This is a YACC file describing the grammar of			*/
/*					arithmetic and logical expressions in JAM.  It is		*/
/*					converted by YACC into 'C' source code to implement		*/
/*					a parser for the JAM expression grammar.				*/
/*																			*/
/****************************************************************************/

%{

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

int yyparse();

%}



/* --- TOKENS --------------------------------------------------------- */

%token AND_TOK
%token OR_TOK
%token EQUALITY_TOK
%token INEQUALITY_TOK
%token GREATER_TOK
%token LESS_TOK
%token GREATER_EQ_TOK
%token LESS_OR_EQ_TOK
%token LEFT_SHIFT_TOK
%token RIGHT_SHIFT_TOK
%token ABS_TOK
%token LOG2_TOK
%token SQRT_TOK
%token CIEL_TOK
%token FLOOR_TOK
%token VALUE_TOK
%token IDENTIFIER_TOK
%token ARRAY_TOK
%token ERROR_TOK

/* --- PRECEDENCES (from lowest to highest) --------------------------- */

%left	OR_TOK
%left	AND_TOK
%left	'|'
%left	'^'
%left	'&'
%left	EQUALITY_TOK INEQUALITY_TOK
%left	GREATER_TOK LESS_TOK GREATER_EQ_TOK LESS_OR_EQ_TOK
%left	LEFT_SHIFT_TOK RIGHT_SHIFT_TOK
%left	'+' '-'
%left	'*' '/' '%'
%left	'!' '~' UNARY_MINUS UNARY_PLUS

%start exp_start

%%

exp_start
	:	expn		{jam_parse_value = $1.val; jam_expr_type = $1.type;}
	;

expn
	:	VALUE_TOK
	|	'(' expn ')'				{$$ = $2;}
	|	'+' expn  %prec UNARY_PLUS	{$$ = $2;}
	|	'-' expn  %prec UNARY_MINUS	{$$ = CALC(UMINUS, $2, NULL_EXP);}
	|	'!' expn					{$$ = CALC(NOT, $2, NULL_EXP);}
	|	'~' expn					{$$ = CALC(BITWISE_NOT, $2, NULL_EXP);}
	|	expn '+' expn				{$$ = CALC(ADD, $1, $3);}
	|	expn '-' expn				{$$ = CALC(SUB, $1, $3);}
	|	expn '*' expn				{$$ = CALC(MULT, $1, $3);}
	|	expn '/' expn				{$$ = CALC(DIV, $1, $3);}
	|	expn '%' expn				{$$ = CALC(MOD, $1, $3);}
	|	expn '&' expn				{$$ = CALC(BITWISE_AND, $1, $3);}
	|	expn '|' expn				{$$ = CALC(BITWISE_OR, $1, $3);}
	|	expn '^' expn				{$$ = CALC(BITWISE_XOR, $1, $3);}
	|	expn AND_TOK expn			{$$ = CALC(AND, $1, $3);}
	|	expn OR_TOK  expn			{$$ = CALC(OR, $1, $3);}
	|	expn LEFT_SHIFT_TOK expn	{$$ = CALC(LEFT_SHIFT, $1, $3);}
	|	expn RIGHT_SHIFT_TOK expn	{$$ = CALC(RIGHT_SHIFT, $1, $3);}
	|	expn EQUALITY_TOK expn		{$$ = CALC(EQUALITY, $1, $3);}
	|	expn INEQUALITY_TOK expn	{$$ = CALC(INEQUALITY, $1, $3);}
	|	expn GREATER_TOK expn		{$$ = CALC(GREATER_THAN, $1, $3);}
	|	expn LESS_TOK expn			{$$ = CALC(LESS_THAN, $1, $3);}
	|	expn GREATER_EQ_TOK expn	{$$ = CALC(GREATER_OR_EQUAL, $1, $3);}
	|	expn LESS_OR_EQ_TOK expn	{$$ = CALC(LESS_OR_EQUAL, $1, $3);}
	|	ABS_TOK '(' expn ')'		{$$ = CALC(ABS, $3, NULL_EXP);}
	|	LOG2_TOK '(' expn ')'		{$$ = CALC(LOG2, $3, NULL_EXP);}
	|	SQRT_TOK '(' expn ')'		{$$ = CALC(SQRT, $3, NULL_EXP);}
	|	CIEL_TOK '(' expn ')'		{$$ = CALC(CIEL, $3, NULL_EXP);}
	|	FLOOR_TOK '(' expn ')'		{$$ = CALC(FLOOR, $3, NULL_EXP);}
	|	ARRAY_TOK '[' expn ']'		{$$ = CALC(ARRAY, $1, $3);}
	;

%%


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

void yyerror (char *msg)

/*																   		*/
/*	WARNING: When compiling for YACC 5.0 using err_skel.c,	     		*/
/*			 this function needs to be modified to be:  		   		*/
/*																   		*/
/*			 yyerror(char *ms1, char *msg2) 					   		*/
/*																   		*/
/*	yyerror() handles syntax error messages from the parser.	   		*/
/*	Since we don't care about anything else but reporting the error,	*/
/*	just flag the error in jam_return_code.						   		*/
/*																   		*/
{
	msg = msg; /* Avoid compiler warning about msg unused */
	jam_return_code = JAMC_SYNTAX_ERROR;
}


/************************************************************************/
/*																   		*/

int yylex()

/*																   		*/
/*	This is the lexer function called by yyparse(). It calls	   		*/
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

	yylval.val = val;
	yylval.type = type;
	yylval.child_otype = 0;
	yylval.loper = 0;
	yylval.roper = 0;

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
/*	This routine sets up the global variables and then calls yyparse()  */
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

	yyparse();

	if (jam_return_code == JAMC_SUCCESS)
	{
		if (result != 0) *result = jam_parse_value;
		if (result_type != 0) *result_type = jam_expr_type;
	}

	return (jam_return_code);
}
