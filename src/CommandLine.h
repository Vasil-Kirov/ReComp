#pragma once
#include "VString.h"
#include "Dynamic.h"

enum command_flags
{
	CommandFlag_time = BIT(0),
};

struct command_line
{
	string BuildFile;
	slice<string> ImportDLLs;
	uint Flags;
};


