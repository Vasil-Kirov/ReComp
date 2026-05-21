#pragma once
#include "Module.h"
#include <VString.h>

enum class ipc_cmd : u8
{
	NOP,
	FIND_SYMBOL,
	DO_COMPLETION,
	QUIT,
};

struct ipc_packet
{
	ipc_cmd CMD;
	slice<u8> Data;
};

void IPCSendMessage(const void *Msg, u32 Size);
string IPCReadSubstituteFile(string OriginalFile);


