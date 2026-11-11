#include "VString.h"
#include "Memory.h"
#include <ctype.h>
#include <immintrin.h>
#include <optional>
#include <pmmintrin.h>


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
	if(Size == 0)
		return string{"", 0};

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
	if(Size == 0)
		return STR_LIT("");

	string Result;

	memcpy(Memory, CString, Size);

	Result.Data = (const char *)Memory;
	Result.Size = Size;
	return Result;
}

string MakeString(string_builder Builder, void *Memory)
{
	string Result = MakeString(Memory, Builder.Data.Data, Builder.Size);
	Builder.Data.Free();
	return Result;
}

string MakeString(string_builder Builder)
{
	string Result = MakeString(Builder.Data.Data, Builder.Size);
	Builder.Data.Free();
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

bool StringContains(const string &a, char c)
{
	for(size_t i = 0; i < a.Size; ++i)
		if(a.Data[i] == c)
			return true;
	return false;
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


int UTF8GetCodepointSize(const char *cp) {
	const u8 SIZE4 = 0b1111'0000;
	const u8 SIZE3 = 0b1110'0000;
	const u8 SIZE2 = 0b1100'0000;
	if ((*cp & SIZE4) == SIZE4) return 4;
	if ((*cp & SIZE3) == SIZE3) return 3;
	if ((*cp & SIZE2) == SIZE2) return 2;
	return 1;
}

const char *UTF8Advance (const char *Position) {
    int Size = UTF8GetCodepointSize(Position);
    return &Position[Size];
}

u32 UTF8ExtractCodepoint(const char *cp, int Size) {
    switch (Size) {
		case 1: {
			return (u32) *cp;
		}
		case 2: {
			u8 First = *cp & 0b0001'1111;
			u8 Second = *(cp+1) & 0b0011'1111;
			return ((u32)First) << 6 | (u32)Second;
		}
		case 3: {
			u8 First = *cp & 0b0000'1111;
			u8 Second = *(cp+1) & 0b0011'1111;
			u8 Third = *(cp+2) & 0b0011'1111;
			return ((u32)First) << 12 | ((u32)Second) << 6 | (u32)Third;
		}
		case 4: {
			u8 First = *cp & 0b0000'0111;
			u8 Second = *(cp+1) & 0b0011'1111;
			u8 Third = *(cp+2) & 0b0011'1111;
			u8 Forth = *(cp+3) & 0b0011'1111;
			return ((u32)First) << 18 | ((u32)Second) << 12 | ((u32)Third) << 6 | (u32)Forth ;
		}
	};
	return 0;
}

u32 UTF8Deref (const char *s) {
    int Size = UTF8GetCodepointSize(s);
    return UTF8ExtractCodepoint(s, Size);
}

std::optional<slice<string>> Split(string S, u32 Delim, bool NullTerm=false, bool HandleQuotes=false)
{
	dynamic<string> r = {};
	const char *p = S.Data;
	while (p - S.Data < S.Size) {
		u32 CP = UTF8Deref(p);
		size_t At = p - S.Data;
		p = UTF8Advance(p);

		if (HandleQuotes && CP == '"') {
			while ((p - S.Data < S.Size) && UTF8Deref(p) != '"') {
				p = UTF8Advance(p);
			}
			if (p - S.Data >= S.Size) {
				return std::nullopt;
			}
			p = UTF8Advance(p);
			continue;
		}

		if (CP == Delim) {
			SliceString(S, 0, At);
			if (NullTerm)
				r.Push(MakeString(S.Data, At));
			else
				r.Push((string){S.Data, At});
			if (At+1 >= S.Size)
				return SliceFromArray(r);
			S = SliceString(S, p-S.Data, 0);
		}
	}
	if (S != "") {
		if (NullTerm)
			r.Push(MakeString(S.Data, S.Size));
		else
			r.Push(S);
	}
	return SliceFromArray(r);
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

string QuickBuild(const char *fmt, ...)
{
	char Buffer[8192] = {};

	va_list Args;
	va_start(Args, fmt);
	
	vsnprintf(Buffer, 8192, fmt, Args);
	
	va_end(Args);

	return MakeString(Buffer);
}

