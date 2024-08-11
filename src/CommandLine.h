#pragma once
#include "VString.h"
#include "Dynamic.h"

enum command_flags
{
	CommandFlag_time = BIT(0),
	CommandFlag_ir   = BIT(1),
	CommandFlag_link = BIT(2),
};

struct command_line
{
	string BuildFile;
	slice<string> ImportDLLs;
	slice<string> LinkArgs;
	uint Flags;
};


