
#ifndef _ERRORS_H
#define _ERRORS_H
#include "Basic.h"
#include "VString.h"

struct error_info
{
	const string *Data;
	const char *FileName;
	i64 Line;
	i64 Character;
};


#if 0
void
raise_semantic_error(File_Contents *f, const char *error_msg, struct _Token_Iden token);

void
raise_parsing_unexpected_token(const char *expected_tok, File_Contents *f);
#endif

void
RaiseError(error_info ErrorInfo, const char *ErrorMessage, ...);

void
SetBonusMessage(string S);

extern string BonusErrorMessage;

#endif // _ERRORS_H
