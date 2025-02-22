#include "Errors.h"
#include "Basic.h"
#include "Log.h"
#include "Platform.h"
#include <atomic>
#include <mutex>

string BonusErrorMessage = {};
bool DumpingInfo = false;
std::atomic<uint> Errors = {};

void SetBonusMessage(string S)
{
	BonusErrorMessage = S;
}

string GetBonusMessage()
{
	return BonusErrorMessage;
}

void AdvanceString(string *String)
{
	String->Data++;
	String->Size--;
}

b32 IsBetween(i64 A, i64 L, i64 R)
{
	return A > L && A < R;
}

void
CountError()
{
	++Errors;
}

void GetErrorSegments(error_info ErrorInfo, string *OutFirst, string *OutHighlight, string *OutThird)
{
	string_builder Builder = MakeBuilder();
	string Code = *ErrorInfo.Data;
	i64 AtLine = 1;
	while(AtLine < ErrorInfo.Range.StartLine)
	{
		if(Code.Data[0] == '\n')
			AtLine++;
		AdvanceString(&Code);
	}

	char AtChar = 1;

	while(AtChar < ErrorInfo.Range.StartChar)
	{
		char c = Code.Data[0];
		Assert(c != '\n');
		PushBuilder(&Builder, c);

		AdvanceString(&Code);
		AtChar++;
	}
	if(Builder.Size == 0)
		*OutFirst = STR_LIT("");
	else
	{
		*OutFirst = MakeString(Builder);
		Builder = MakeBuilder();
	}

	while(AtChar < ErrorInfo.Range.EndChar || AtLine < ErrorInfo.Range.EndLine)
	{
		char c = Code.Data[0];
		if(c == '\n')
		{
			AtChar = 0; // At the end of the loop it becomes 1
			AtLine++;
		}
		PushBuilder(&Builder, c);

		AdvanceString(&Code);
		AtChar++;
	}

	if(Builder.Size == 0)
		*OutHighlight = STR_LIT("");
	else
	{
		*OutHighlight = MakeString(Builder);
		Builder = MakeBuilder();
	}

	char c = 0;
	do {
		if(Code.Size == 0)
			break;

		c = Code.Data[0];
		PushBuilder(&Builder, c);

		AdvanceString(&Code);

	} while(c != '\n');

	if(Builder.Size == 0)
		*OutThird = STR_LIT("");
	else
	{
		*OutThird = MakeString(Builder);
		Builder = MakeBuilder();
	}
}

bool
HasErroredOut()
{
	return Errors != 0;
}

void
RaiseError(b32 Abort, error_info ErrorInfo, const char *_ErrorMessage, ...)
{
	std::mutex ErrorMutex;
	CountError();

	ErrorMutex.lock();
	string ErrorMessage = MakeString(_ErrorMessage);
	char *FinalFormat = (char *)VAlloc(LOG_BUFFER_SIZE);

	va_list Args;
	va_start(Args, _ErrorMessage);
	
	vsnprintf(FinalFormat, LOG_BUFFER_SIZE, ErrorMessage.Data, Args);
	
	va_end(Args);
	
	string FirstPart;
	string Highlight;
	string AfterPart;
	GetErrorSegments(ErrorInfo, &FirstPart, &Highlight, &AfterPart);

	if(DumpingInfo)
	{
		void WriteStringError(const char *FileName, int LineNumber, const char *ErrorMsg);
		WriteStringError(ErrorInfo.FileName, ErrorInfo.Range.StartLine, FinalFormat);
		VFree(FinalFormat);
		return;
	}

	
	LogCompilerError("\nError: %s (%d, %d):\n%s\n\n%s",
			ErrorInfo.FileName, ErrorInfo.Range.StartLine, ErrorInfo.Range.StartChar, FinalFormat, FirstPart.Data);
	PlatformOutputString(Highlight, LOG_ERROR);
	LogCompilerError("%s\n", AfterPart.Data);

	if(BonusErrorMessage.Size > 0)
	{
		PlatformOutputString(STR_LIT("Note:\n"), LOG_INFO);
		PlatformOutputString(BonusErrorMessage, LOG_INFO);
		PlatformOutputString(STR_LIT("\n"), LOG_INFO);
	}

	VFree(FinalFormat);

	ErrorMutex.unlock();
	if(Abort || Errors > 4)
		exit(1);
}


