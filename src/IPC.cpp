#include "IPC.h"
#include "Platform.h"

u64 IPCWritePipe;
u64 IPCReadPipe;

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

string IPCReadSubstituteFile(string OriginalFile)
{
	IPCSendMessage((void *)OriginalFile.Data, (u32)OriginalFile.Size);
	u32 Size = 0;
	void *Data = IPCRecvMessage(&Size);
	return string {(const char *)Data, Size};
}

