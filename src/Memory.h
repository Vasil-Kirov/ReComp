/* date = January 17th 2022 2:57 pm */

#ifndef _MEMORY_H
#define _MEMORY_H
#include "Basic.h"

enum AllocIndex
{
	PERM_INDEX = 0,
	STR_INDEX = 1
};

typedef struct _ap_memory
{
	u16 ChunkIndex;
	void *Start;
	void *End;
	void *Current;
	u64 ChunkSize;
	u64 MaxSize;
} ap_memory;

struct scratch_arena
{
	ap_memory Arena;
	void *Allocate(u64 Size);
	scratch_arena();
	~scratch_arena();
};

b32
InitializeMemory();

void *
AllocateMemory(u64 Size, i8 Index);

void *ToArena(void *, u64 Size, i8 Index);

void
ResetCompileMemory();

#define AllocatePermanent(SIZE)  AllocateMemory((SIZE), PERM_INDEX)
#define AllocateString(SIZE) (char *)AllocateMemory((SIZE), STR_INDEX)
#define NewType(Type) (Type *)AllocatePermanent(sizeof(Type))
#define DupeType(Data, Type) (Type *)ToArena((void *)&Data, sizeof(Type), PERM_INDEX)


#endif //_MEMORY_H
