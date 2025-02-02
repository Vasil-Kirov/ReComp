#include "Memory.h"
#include "Basic.h"
#include "Threading.h"
#include "VString.h"
#include "Log.h"
#include "Platform.h"

std::mutex MemMutex;

static ap_memory MemoryAllocators[2];

#define PERM_SIZE GB(4)
#define STR_SIZE  MB(64)

#define PERM_CHUNK KB(4) * 4096
#define STR_CHUNK  KB(4) * 256

void
InitArenaMem(ap_memory *Memory, u64 Size, u64 ChunkSize)
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
	InitArenaMem(&MemoryAllocators[PERM_INDEX], PERM_SIZE, PERM_CHUNK);
	InitArenaMem(&MemoryAllocators[STR_INDEX],  STR_SIZE,  STR_CHUNK);
	MemoryAllocators[PERM_INDEX].Name = "permanent";
	MemoryAllocators[STR_INDEX].Name = "string";
	return true;
}

void *ToArena(void *Data, u64 Size, i8 Index)
{
	void *Result = AllocateMemory(Size, Index);
	memcpy(Result, Data, Size);
	return Result;
}

void *
ArenaAllocate(ap_memory *Arena, u64 Size, b32 NoZeroOut)
{
	Arena->Current = Align16(Arena->Current);
	void *Result = Arena->Current;
	Arena->Current = (char *)Arena->Current + Size;
	while((char *)Arena->Current >= (char *)Arena->End)
	{
		if(Arena->ChunkIndex * Arena->ChunkSize > Arena->MaxSize)
		{
			// @NOTE: use fprintf because my logger needs to allocate memoroy
			fprintf(stderr, "MEMORY OVERFLOW WHEN ALLOCATING %s MEMORY\n", Arena->Name);
			PrintStacktrace();
			exit(1);
		}
		PlatformAllocateReserved((u8 *)Arena->Start + Arena->ChunkIndex * Arena->ChunkSize, Arena->ChunkSize);
		Arena->ChunkIndex++;
		Arena->End = (u8 *)Arena->End + Arena->ChunkSize;
	}

	if(!NoZeroOut)
		memset(Result, 0, Size);
	return Result;
}

void *
AllocateMemory(u64 Size, i8 Index)
{
	MemMutex.lock();

	void *Result = ArenaAllocate(&MemoryAllocators[Index], Size);

	MemMutex.unlock();
	return Result;
}

scratch_arena::scratch_arena()
{
	InitArenaMem(&Arena, MB(1), KB(1));
	Arena.Name = "scratch";
}

scratch_arena::~scratch_arena()
{
	PlatformFreeMemory(Arena.Start, Arena.MaxSize);
	Arena = {};
}

void *scratch_arena::Allocate(u64 Size)
{
	return ArenaAllocate(&Arena, Size);
}

void FreeArena(ap_memory *Arena)
{
	size_t Size = (u8 *)Arena->End - (u8 *)Arena->Start;
	PlatformFreeMemory(Arena->Start, Size);
	Arena->Start = NULL;
	Arena->End = NULL;
	Arena->Current = NULL;
	Arena->MaxSize = 0;
	Arena->Name = NULL;
}

void FreeAllArenas()
{
	for(int i = 0; i < ARR_LEN(MemoryAllocators); ++i)
	{
		FreeArena(&MemoryAllocators[i]);
	}
}

