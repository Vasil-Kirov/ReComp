#pragma once
#include "Basic.h"
#include "VString.h"
#include "Errors.h"
#include <Dynamic.h>

enum token_type
{
	T_PTR   = '*',
	T_ADDROF= '&',
	T_DECL  = ':',
	T_STARTSCOPE = '{',
	T_ENDSCOPE   = '}',
	T_OPENPAREN  = '(',
	T_CLOSEPAREN = ')',
	T_OPENBRACKET  = '[',
	T_CLOSEBRACKET = ']',
	T_CAST  = '@',
	T_EQ    = '=',
	T_LESS  = '<',
	T_GREAT = '>',
	T_COMMA = ',',
	T_DOT   = '.',
	T_SEMICOL= ';',
	T_EOF   = -1,
	T_ID    = -2,
	T_IF    = -3,
	T_ELSE  = -4,
	T_FOR   = -5,
	T_VAL   = -6,
	T_STR   = -7,
	T_NEQ   = -8,
	T_GEQ   = -9,
	T_LEQ   = -10,
	T_EQEQ  = -11,
	T_ARR   = -12,
	T_PPLUS = -13,
	T_MMIN  = -14,
	T_LOR   = -15,
	T_LAND  = -16,
	T_SLEFT = -17,
	T_SRIGHT= -18,
	T_PEQ   = -19,
	T_MEQ   = -20,
	T_TEQ   = -21,
	T_DEQ   = -22,
	T_MODEQ = -23,
//	T_SLEQ  = -24, // Lexer doesn't support 3 character combinations right now
//	T_SREQ  = -25,
	T_ANDEQ = -26,
	T_XOREQ = -27,
	T_OREQ  = -28,
	T_FN    = -29,
	T_CONST = -30,
	T_SHADOW= -31,
	T_RETURN= -32,
	T_AUTOCAST= -34,
	T_FOREIGN = -34,
	T_CSTR   = -35,
	T_STRUCT = -36,
	T_IMPORT = -37,
	T_AS     = -38,
	T_PUBLIC = -39,
	T_PRIVATE= -40,
	T_SIZE   = -41,
	T_IN     = -42,
};

struct token
{
	token_type Type;
	string *ID;
	error_info ErrorInfo;
};

struct keyword
{
	string ID;
	token_type Type;
};


file StringToTokens(string String, error_info ErrorInfo);

token GetNextToken(string *String, error_info *ErrorInfo);

const char* GetTokenName(token_type Token);

void InitializeLexer();

