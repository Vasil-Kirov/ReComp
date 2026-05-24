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

extern uint IPCAtStage;

 __attribute__((noreturn))
void IPCListenAndServe();
void IPCSetModules(slice<module*> Modules);
void IPCSendMessage(const void *Msg, u32 Size);
string IPCReadSubstituteFile(string OriginalFile);
extern uint IPCAtStage;


