#pragma once
#include "VString.h"
#include "Dynamic.h"

enum command_flags
{
	CommandFlag_time = BIT(0),
	CommandFlag_ir   = BIT(1),
	CommandFlag_link = BIT(2),
	CommandFlag_llvm = BIT(3),
	CommandFlag_dumpinfo = BIT(4),
	CommandFlag_nothread = BIT(5),
};

enum compile_flags
{
    CF_Debug       	= 0b00000000001,
    CF_SanAdress   	= 0b00000000010,
    CF_SanMemory   	= 0b00000000100,
    CF_SanThread   	= 0b00000001000,
    CF_SanUndefined	= 0b00000010000,
    CF_NoStdLib		= 0b00000100000,
    CF_SharedLib	= 0b00001000000,
	CF_CrossAndroid = 0b00010000000,
	CF_NoLink	    = 0b00100000000,
	CF_NoTypeTable	= 0b01000000000,
	Reserved2		= 0b10000000000,
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

