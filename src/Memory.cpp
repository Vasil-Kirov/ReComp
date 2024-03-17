#include "Memory.h"
#include "Threading.h"
#include "String.h"
#include "Log.h"

typedef struct _ap_memory
{
	u16 ChunkIndex;
	void *Start;
	void *End;
	void *Current;
	u64 ChunkSize;
	u64 MaxSize;
} ap_memory;

static ap_memory MemoryAllocators[2];

#define PERM_SIZE GB(4)
#define STR_SIZE  MB(64)

#define PERM_CHUNK MB(128)
#define STR_CHUNK  MB(1)

void
InitAPMem(ap_memory *Memory, u64 Size, u64 ChunkSize)
{
	Memory->Start = PlatformReserveMemory(Size);
	if(Memory->Start == NULL)
	{
		string E;
		E.Data = "Couldn't allocate memory!";
		E.Size = sizeof("Couldn't allocate memory!") - 1;
		PlatformOutputString(E, LOG_FATAL);
		exit(1);
	}
	PlatformAllocateReserved(Memory->Start, ChunkSize);
	
	Memory->End = (u8 *)Memory->Start + ChunkSize; 
	Memory->Current = Memory->Start;
	Memory->ChunkIndex = 1;
	Memory->ChunkSize = ChunkSize;
	Memory->MaxSize = Size;
}


// @Note: I just want to initialize this before everything else
b32
InitializeMemory()
{
	InitAPMem(&MemoryAllocators[PERM_INDEX], PERM_SIZE, PERM_CHUNK);
	InitAPMem(&MemoryAllocators[STR_INDEX],  STR_SIZE,  STR_CHUNK);
	return true;
}

void *
AllocateMemory(u64 Size, i8 Index)
{
	LockMutex();

	void *Result = MemoryAllocators[Index].Current;
	MemoryAllocators[Index].Current = (char *)MemoryAllocators[Index].Current + Size;
	while((char *)MemoryAllocators[Index].Current > (char *)MemoryAllocators[Index].End)
	{
		if(MemoryAllocators[Index].ChunkIndex * MemoryAllocators[Index].ChunkSize > MemoryAllocators[Index].MaxSize)
		{
			const char *NAME[2] = { "permanent", "string" };
			LFATAL("MEMORY OVERFLOW when allocating %s memory", NAME[Index]);
		}
		PlatformAllocateReserved((u8 *)MemoryAllocators[Index].Start + MemoryAllocators[Index].ChunkIndex * MemoryAllocators[Index].ChunkSize, MemoryAllocators[Index].ChunkSize);
		MemoryAllocators[Index].ChunkIndex++;
		MemoryAllocators[Index].End = (u8 *)MemoryAllocators[Index].End + MemoryAllocators[Index].ChunkSize;
	}

	UnlockMutex();
	memset(Result, 0, Size);
	return Result;
}

