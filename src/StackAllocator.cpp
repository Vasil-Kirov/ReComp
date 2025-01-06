#include "StackAllocator.h"


stack_alloc::stack_alloc() : Tracker(1024), At(0), DoZeroOut(false)
{
	InitArenaMem(&Arena, GB(16), MB(32));
}

void stack_alloc::Free()
{
	FreeArena(&Arena);
}
//stack_alloc::~stack_alloc()
//{
//	FreeArena(&Arena);
//}

void *stack_alloc::Push(size_t Size)
{
	Tracker[At++] = Arena.Current;
	return ArenaAllocate(&Arena, Size, !DoZeroOut);
}

void stack_alloc::Pop()
{
	Assert(At > 0);
	Arena.Current = Tracker[--At];
}

