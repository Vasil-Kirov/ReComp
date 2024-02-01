/* date = January 17th 2022 2:57 pm */

#ifndef _MEMORY_H
#define _MEMORY_H
#include "Basic.h"

enum AllocIndex
{
	PERM_INDEX = 0,
	STR_INDEX = 1
};

void
InitializeMemory();

void *
AllocateMemory(u64 Size, i8 Index);

void
ResetCompileMemory();

#define AllocatePermanent(SIZE)  AllocateMemory((SIZE), PERM_INDEX)
#define AllocateString(SIZE) (char *)AllocateMemory((SIZE), STR_INDEX)
#define NewType(Type) (Type *)AllocatePermanent(sizeof(Type))


#endif //_MEMORY_H
