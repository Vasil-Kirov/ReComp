#include "IPC.h"
#include "Module.h"
#include "Platform.h"

u64 IPCWritePipe;
u64 IPCReadPipe;
slice<module*> IPCProgramModules = {};

void IPCSetModules(slice<module*> Modules)
{
	IPCProgramModules = Modules;
}

void IPCSendMessage(const void *Msg, u32 Size)
{
	PlatformWritePipe(IPCWritePipe, &Size, sizeof(u32));
	PlatformWritePipe(IPCWritePipe, Msg, Size);
}

void *IPCRecvMessage(u32 *OutSize)
{
	u32 Size = 0;
	PlatformReadPipe(IPCReadPipe, &Size, sizeof(u32));
	void *Data = VAlloc(Size);
	PlatformReadPipe(IPCReadPipe, Data, Size);
	if(OutSize)
		*OutSize = Size;
	return Data;
}

void IPCExecCMD(ipc_cmd Cmd, slice<u8> Data)
{
	IPCSendMessage(&Cmd, 1);
	IPCSendMessage(Data.Data, Data.Count);
}

ipc_packet IPCGetCMD()
{
	u32 Size = 0;
	u8 *Cmd = (u8 *)IPCRecvMessage(&Size);
	if(Size != 1)
		return {};
	u8 *Data = (u8 *)IPCRecvMessage(&Size);
	slice<u8> s = {.Data = Data, .Count = Size};
	return ipc_packet{(ipc_cmd)*Cmd, s};
}

string IPCReadSubstituteFile(string OriginalFile)
{
	IPCSendMessage((void *)OriginalFile.Data, (u32)OriginalFile.Size);
	u32 Size = 0;
	void *Data = IPCRecvMessage(&Size);
	return string {(const char *)Data, Size};
}

 __attribute__((noreturn))
void IPCListenAndServe()
{
	IPCSendMessage("0", 1);
	bool Running = true;
	while(Running)
	{
		ipc_packet Packet = IPCGetCMD();
		switch(Packet.CMD)
		{
			case ipc_cmd::NOP:
			{
			} break;
			case ipc_cmd::FIND_SYMBOL:
			{
			} break;
			case ipc_cmd::DO_COMPLETION:
			{
			} break;
			case ipc_cmd::QUIT:
			{
				Running = false;
			} break;
		}
	}
	exit(0);
}

