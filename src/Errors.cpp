#include "Errors.h"
#include "Basic.h"
#include "Log.h"

void AdvanceString(string *String)
{
	String->Data++;
	String->Size--;
}

b32 IsBetween(i64 A, i64 L, i64 R)
{
	return A > L && A < R;
}

string GetErrorSegment(error_info ErrorInfo)
{
	string_builder Builder = MakeBuilder();
	string Code = *ErrorInfo.Data;
	i64 Line = 1;
	while(Line != ErrorInfo.Line)
	{
		if(Code.Data[0] == '\n')
			Line++;
		AdvanceString(&Code);
	}
	char c = ' ';
	do {
		c = Code.Data[0];
		PushBuilder(&Builder, c);

		AdvanceString(&Code);
	} while(c != '\n' && c != 0);
	return MakeString(Builder);
#if 0
	i64 Character = 1;
	while(Character + 7 < ErrorInfo.Character)
	{
		Character++;
		AdvanceString(&Code);
	}

	while(*Code.Data != 0 && Line - 1 <= ErrorInfo.Line)
	{
		if(Character - 7 >= ErrorInfo.Character)
		{
			while(Code.Data[0] != '\n' && Code.Data[0] != '\0') AdvanceString(&Code);

			// @NOTE: Change this so we don't go back into the same branch
			Character = 0;
			continue;
		}
		if(Character == 1 && !IsBetween(ErrorInfo.Character, 0, 4))
		{
			PushBuilder(&Builder, '>');
		}
		else if(Line - 1 == ErrorInfo.Line && IsBetween(Character, ErrorInfo.Character - 2, ErrorInfo.Character + 2))
		{
			PushBuilder(&Builder, '^');
		}
		else
		{
			PushBuilder(&Builder, Code.Data[0]);
		}
		if(Code.Data[0] == '\n')
		{
			Line++;
			Character = 0;
		}
		Character++;
		AdvanceString(&Code);
	}
	return MakeString(Builder);
#endif
}

void
RaiseError(error_info ErrorInfo, const char *_ErrorMessage, ...)
{
	string ErrorMessage = MakeString(_ErrorMessage);
	char FinalFormat[4096] = {0};

	va_list Args;
	va_start(Args, _ErrorMessage);
	
	vsnprintf_s(FinalFormat, 4096, ErrorMessage.Data, Args);
	
	va_end(Args);
	
	string ErrorSegment = GetErrorSegment(ErrorInfo);
	LFATAL("%s (%d, %d):\n%s\n\n%s",
			ErrorInfo.FileName, ErrorInfo.Line, ErrorInfo.Character, FinalFormat, ErrorSegment.Data);
}

