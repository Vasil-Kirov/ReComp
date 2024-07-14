#pragma once
#include "Basic.h"

struct string
{
	const char *Data;
	size_t Size;

	bool operator==(const string& B) const
	{
		if(this->Size != B.Size)
			return false;

		return memcmp(this->Data, B.Data, B.Size) == 0;
	}

};

struct string_builder
{
	char *Data;
	size_t Size;
	void operator+=(const string& B);
	void operator+=(const char *String);
	void operator+=(char C);
};

#define STR_LIT(LIT) MakeString(LIT, sizeof(LIT) - 1)

string_builder MakeBuilder();

string MakeString(string_builder Builder);
string MakeString(const char *CString, size_t Size);
string MakeString(const char *CString);

void PushBuilder(string_builder *Builder, const char *Data);
void PushBuilder(string_builder *Builder, char C);
void PushBuilderFormated(string_builder *Builder, const char *Format, ...);

size_t CStrLen(const char *CString);

