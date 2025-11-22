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
    CF_Debug       	= 0b000000000001,
    CF_SanAdress   	= 0b000000000010,
    CF_SanMemory   	= 0b000000000100,
    CF_SanThread   	= 0b000000001000,
    CF_SanUndefined	= 0b000000010000,
    CF_NoLibC		= 0b000000100000,
    CF_SharedLib	= 0b000001000000,
	CF_CrossAndroid = 0b000010000000,
	CF_NoLink	    = 0b000100000000,
	CF_Standalone	= 0b001000000000,
	CF_DebugInfo	= 0b010000000000,
	CF_DisableAssert= 0b100000000000,
};

enum arch
{
	Arch_x86_64	= 0b00000,
	Arch_x86	= 0b00001,
	Arch_arm32	= 0b00010,
	Arch_arm64	= 0b00100,
	Arch_Wasm32	= 0b01000,
	Arch_Wasm64	= 0b10000,
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

bool ShouldOutputIR(string MName);

