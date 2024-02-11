#pragma once
#include "Basic.h"
#include "String.h"
#include "Errors.h"

enum token_type
{
	T_PTR   = '*',
	T_DECL  = ':',
	T_STARTSCOPE = '{',
	T_ENDSCOPE   = '}',
	T_EQ    = '=',
	T_EOF   = -1,
	T_ID    = -2,
	T_IF    = -3,
	T_FOR   = -4,
	T_NUM   = -5,
	T_STR   = -6,
	T_NEQ   = -7,
	T_GEQ   = -8,
	T_LEQ   = -9,
	T_EQEQ  = -10,
	T_ARR   = -11,
	T_PPLUS = -12,
	T_MMIN  = -13,
	T_LOR   = -14,
	T_LAND  = -15,
	T_SLEFT = -16,
	T_SRIGHT= -17,
	T_PEQ   = -18,
	T_MEQ   = -19,
	T_TEQ   = -20,
	T_DEQ   = -21,
	T_MODEQ = -22,
//	T_SLEQ  = -23, // Lexer doesn't support 3 character combinations right now
//	T_SREQ  = -24,
	T_ANDEQ = -25,
	T_XOREQ = -26,
	T_OREQ  = -27,
	T_FN    = -28,
	T_CONST = -29,
	T_SHADOW= -30,
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


token *StringToTokens(string String, error_info ErrorInfo);

token GetNextToken(string *String, error_info *ErrorInfo);

const char* GetTokenName(token_type Token);

void InitializeLexer();

