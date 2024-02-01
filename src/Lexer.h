#pragma once
#include "Basic.h"
#include "String.h"
#include "Errors.h"

enum token_type
{
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
	T_SRIGHT =-17,
};

struct token
{
	error_info ErrorInfo;
	token_type Type;
	string *ID;
};

struct keyword
{
	string ID;
	token_type Type;
};


token *StringToTokens(string String, error_info ErrorInfo);

token GetNextToken(string *String, error_info *ErrorInfo);

void InitializeLexer();

