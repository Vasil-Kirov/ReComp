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
};

#define STR_LIT(LIT) MakeString(LIT, sizeof(LIT) - 1)

string MakeString(string_builder Builder);
string MakeString(const char *CString, size_t Size);
string MakeString(const char *CString);

void PushBuilder(string_builder *Builder, const char *Data);
void PushBuilder(string_builder *Builder, char C);

size_t CStrLen(const char *CString);

