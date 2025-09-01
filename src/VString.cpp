#include "VString.h"
#include "Memory.h"
#include <ctype.h>
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

string_builder MakeBuilder()
{
	string_builder Builder;
	Builder.Data = {};
	Builder.Size = 0;
	return Builder;
}

void PushBuilder(string_builder *Builder, const char *Data)
{
	for(int I = 0; Data[I] != 0; ++I)
	{
		Builder->Data.Push(Data[I]);
		Builder->Size++;
	}
}

void PushBuilder(string_builder *Builder, char C)
{
	Builder->Data.Push(C);
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

string MakeStringSlice(const char *Ptr, size_t Size)
{
	string Result;
	Result.Data = Ptr;
	Result.Size = Size;
	return Result;
}

string MakeString(const char *CString, size_t Size)
{
	Assert(CString);

	string Result;

	char *Data = AllocateString(Size + 1);
	memcpy(Data, CString, Size);

	Result.Data = Data;
	Result.Size = Size;
	return Result;
}

string MakeString(void *Memory, const char *CString, size_t Size)
{
	string Result;

	memcpy(Memory, CString, Size);

	Result.Data = (const char *)Memory;
	Result.Size = Size;
	return Result;
}

string MakeString(string_builder Builder, void *Memory)
{
	string Result = MakeString(Memory, Builder.Data.Data, Builder.Size);
	VFree(Builder.Data.Data);
	Builder.Data.Count = 0;
	return Result;
}

string MakeString(string_builder Builder)
{
	string Result = MakeString(Builder.Data.Data, Builder.Size);
	VFree(Builder.Data.Data);
	Builder.Data.Count = 0;
	return Result;
}

string MakeString(const char *CString)
{
	return MakeString(CString, CStrLen(CString));
}

void string_builder::printf(const char *fmt, ...)
{
	char ToPush[4096] = {0};
	va_list Args;
	va_start(Args, fmt);
	
	vsnprintf(ToPush, 4096, fmt, Args);
	
	va_end(Args);
	*this += ToPush;
}

void string_builder::operator+=(const string& B)
{
	for(int I = 0; I < B.Size; ++I)
	{
		this->Data.Push(B.Data[I]);
		this->Size++;
	}
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

bool StringStartsWith(const string &a, const string b)
{
	if(b.Size > a.Size)
		return false;

	return memcmp(a.Data, b.Data, b.Size) == 0;
}

bool StringEndsWith(const string &a, const string b)
{
	if(b.Size > a.Size)
		return false;

	int Offset = a.Size - b.Size;
	return memcmp(a.Data + Offset, b.Data, b.Size) == 0;
}

split SplitAt(string S, char c)
{
	for(int i = 0; i < S.Size; ++i)
	{
		if(S.Data[i] == c)
		{
			string First = SliceString(S, 0, i);
			string Second = SliceString(S, i+1, 0);
			return { First, Second };
		}
	}
	return {};
}

i32 DistanceBetweenStrings(const string &A, const string &B, int Threshold)
{
	const i32 FAILED = INT32_MAX;

	long long Len1 = (long long)A.Size;
	long long Len2 = (long long)B.Size;
	if(abs(Len1 - Len2) > Threshold)
		return FAILED;

	string S1 = A;
	string S2 = B;
	if(Len1 > Len2)
	{
		SWAP(S1, S2);
		SWAP(Len1, Len2);
	}

    int maxi = Len1;
    int maxj = Len2;

	scratch_arena Scratch = {};

    i32 *current = (int *)Scratch.Allocate(sizeof(i32) * (maxi+1));
    i32 *minus1  = (int *)Scratch.Allocate(sizeof(i32) * (maxi+1));
    i32 *minus2  = (int *)Scratch.Allocate(sizeof(i32) * (maxi+1));
    i32 *dSwap;

    for (i32 i = 0; i <= maxi; i++) { current[i] = i; }

    i32 jm1 = 0, im1 = 0, im2 = -1;

    for (i32 j = 1; j <= maxj; j++) {
        // Rotate
        dSwap = minus2;
        minus2 = minus1;
        minus1 = current;
        current = dSwap;

        // Initialize
        i32 min_distance = INT32_MAX;
        current[0] = j;
        im1 = 0;
        im2 = -1;

        for (i32 i = 1; i <= maxi; i++) {

            i32 cost = S1.Data[im1] == S2.Data[jm1] ? 0 : 1;

            i32 del = current[im1] + 1;
            i32 ins = minus1[i] + 1;
            i32 sub = minus1[im1] + cost;

            //Fastest execution for min value of 3 integers
            int vmin = (del > ins) ? (ins > sub ? sub : ins) : (del > sub ? sub : del);

            if (i > 1 && j > 1 && S1.Data[im2] == S2.Data[jm1] && S1.Data[im1] == S2.Data[j - 2])
                vmin = MIN(vmin, minus2[im2] + cost);

            current[i] = vmin;
            if (vmin < min_distance) { min_distance = vmin; }
            im1++;
            im2++;
        }
        jm1++;
        if (min_distance > Threshold) 
			return INT32_MAX;
    }

    int result = current[maxi];
    return (result > Threshold) ? INT32_MAX : result;

}

