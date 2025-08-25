
#ifndef _ERRORS_H
#define _ERRORS_H
#include "Basic.h"
#include "VString.h"

struct range
{
	i32 StartLine;
	i32 EndLine;
	i16 StartChar;
	i16 EndChar;
};

struct error_info
{
	const string *Data;
	const char *FileName;
	range Range;
};


#if 0
void
raise_semantic_error(File_Contents *f, const char *error_msg, struct _Token_Iden token);

void
raise_parsing_unexpected_token(const char *expected_tok, File_Contents *f);
#endif

void
RaiseError(b32 Abort, error_info ErrorInfo, const char *_ErrorMessage, ...);

void
SetBonusMessage(string S);

string
GetBonusMessage();

void
GetErrorSegments(error_info ErrorInfo, string *OutFirst, string *OutHighlight, string *OutThird);

string
GetInfoRegionWhole(error_info Info);

void
CountError();

bool
HasErroredOut();

extern string BonusErrorMessage;

#endif // _ERRORS_H
