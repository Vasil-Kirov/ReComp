#include "VString.h"
#include "Memory.h"
#include <immintrin.h>
#include <pmmintrin.h>


int _mm_testc_si128 (__m128i a, __m128i b);

__attribute__((no_sanitize("address")))
size_t
CStrLen(const char *str)
{
	// @NOTE: Causes a crash otherwise
	if (str[0] == 0)
		return 0;

	size_t result = 0;

	const __m128i zeros = _mm_setzero_si128();
	__m128i* mem = (__m128i*)str;

	for (/**/; /**/; mem++, result += 16) 
	{

		const __m128i data = _mm_loadu_si128(mem);
		const __m128i cmp  = _mm_cmpeq_epi8(data, zeros);

		if (!_mm_testc_si128(zeros, cmp)) 
		{
			int mask = _mm_movemask_epi8(cmp);

			return result + __builtin_ctz(mask);
		}
	}
}

// @TODO: This is really slow but maybe it doesn't matter
string_builder MakeBuilder()
{
	string_builder Builder;
	Builder.Data = ArrCreate(char);
	Builder.Size = 0;
	return Builder;
}

void PushBuilder(string_builder *Builder, const char *Data)
{
	for(int I = 0; Data[I] != 0; ++I)
	{
		ArrPush(Builder->Data, Data[I]);
		Builder->Size++;
	}
}

void PushBuilder(string_builder *Builder, char C)
{
	ArrPush(Builder->Data, C);
	Builder->Size++;
}

void PushBuilderFormated(string_builder *Builder, const char *Format, ...)
{
	char ToPush[4096] = {0};
	va_list Args;
	va_start(Args, Format);
	
	vsnprintf(ToPush, 4096, Format, Args);
	
	va_end(Args);
	PushBuilder(Builder, ToPush);
}

string MakeString(const char *CString, size_t Size)
{
	string Result;

	char *Data = AllocateString(Size + 1);
	memcpy(Data, CString, Size);

	Result.Data = Data;
	Result.Size = Size;
	return Result;
}

string MakeString(string_builder Builder)
{
	string Result = MakeString(Builder.Data, Builder.Size);
	ArrFree(Builder.Data);
	return Result;
}

string MakeString(const char *CString)
{
	return MakeString(CString, CStrLen(CString));
}

void string_builder::operator+=(const string& B)
{
	PushBuilder(this, B.Data);
}

void string_builder::operator+=(const char *String)
{
	PushBuilder(this, String);
}

void string_builder::operator+=(char C)
{
	PushBuilder(this, C);
}

string SliceString(string S, int From, int To)
{
	if(From < 0)
		From = (S.Size + From);
	if(To <= 0)
		To = (S.Size + To);
	string Result = {
		.Data = S.Data + From,
		.Size = (size_t)(To - From),
	};

	return Result;
}

bool StringsMatchNoCase(const string &a, const string &b)
{
	if(a.Size != b.Size)
		return false;

	for(int i = 0; i < a.Size; ++i)
	{
		if(tolower(a.Data[i]) != tolower(b.Data[i]))
			return false;
	}
	return true;
}

