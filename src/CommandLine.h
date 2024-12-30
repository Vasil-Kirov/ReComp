#pragma once
#include "VString.h"
#include "Dynamic.h"

enum command_flags
{
	CommandFlag_time = BIT(0),
	CommandFlag_ir   = BIT(1),
	CommandFlag_link = BIT(2),
	CommandFlag_llvm = BIT(3),
};

enum compile_flags
{
    CF_Debug       	= 0b0000000001,
    CF_SanAdress   	= 0b0000000010,
    CF_SanMemory   	= 0b0000000100,
    CF_SanThread   	= 0b0000001000,
    CF_SanUndefined	= 0b0000010000,
    CF_NoStdLib		= 0b0000100000,
    CF_SharedLib	= 0b0001000000,
	CF_CrossAndroid = 0b0010000000,
	CF_NoLink	    = 0b0100000000,
	CF_DumpInfo		= 0b1000000000,
};

enum arch
{
	Arch_x86_64	= 0b000,
	Arch_x86	= 0b001,
	Arch_arm32	= 0b010,
	Arch_arm64	= 0b100,
};

struct command_line
{
	string BuildFile;
	string SingleFile;
	slice<string> ImportDLLs;
	dynamic<string> LinkArgs; // dynamic so that more can be added later by the compile function
	slice<string> IRModules;
	uint Flags;
};

bool ShouldOutputIR(string MName, command_line CommandLine);

