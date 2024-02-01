#pragma once
#include "Basic.h"

struct string
{
	const char *Data;
	size_t Size;

	bool operator==(const string& B)
	{
		if(this->Size != B.Size)
			return false;

		return strncmp(this->Data, B.Data, this->Size) == 0;
	}

};

struct string_builder
{
	char *Data;
	size_t Size;
};


string MakeString(string_builder Builder);
string MakeString(const char *CString, size_t Size);
string MakeString(const char *CString);

void PushBuilder(string_builder *Builder, const char *Data);
void PushBuilder(string_builder *Builder, char C);

size_t CStrLen(const char *CString);

