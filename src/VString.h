#pragma once
#include "Dynamic.h"
#include <string.h>
#include <stddef.h>
#include <stdarg.h>

size_t CStrLen(const char *CString);

struct string
{
	const char *Data;
	size_t Size;

	bool operator==(const char *B) const
	{
		auto BLen = CStrLen(B);
		if(this->Size != BLen)
			return false;

		return memcmp(this->Data, B, BLen) == 0;
	}
	bool operator==(const string& B) const
	{
		if(this->Size != B.Size)
			return false;

		return memcmp(this->Data, B.Data, B.Size) == 0;
	}

	bool operator!=(const string& B) const
	{
		if(this->Size != B.Size)
			return true;

		return memcmp(this->Data, B.Data, B.Size) != 0;
	}

};

struct string_builder
{
	dynamic<char> Data;
	size_t Size;
	void operator+=(const string& B);
	void operator+=(const char *String);
	void operator+=(char C);
};

struct split
{
	string first;
	string second;
};

#define STR_LIT(LIT) MakeString(LIT, sizeof(LIT) - 1)

string_builder MakeBuilder();

split SplitAt(string S, char c);

string MakeString(string_builder Builder, void *Memory);
string MakeString(void *Memory, const char *CString, size_t Size);
string MakeString(string_builder Builder);
string MakeString(const char *CString, size_t Size);
string MakeString(const char *CString);
string SliceString(string S, int from, int to);
bool StringsMatchNoCase(const string &a, const string &b);
bool StringStartsWith(const string &a, const string b);
bool StringEndsWith(const string &a, const string b);

void PushBuilder(string_builder *Builder, const char *Data);
void PushBuilder(string_builder *Builder, char C);
void PushBuilderFormated(string_builder *Builder, const char *Format, ...);

string MakeStringSlice(const char *Ptr, size_t Size);


