#pragma once
#include "Dynamic.h"
#include "Memory.h"


struct stack_alloc
{
	ap_memory Arena;
	array<void *> Tracker;
	int At;
	b32 DoZeroOut;
	stack_alloc();
	//~stack_alloc();
	void *Push(size_t Size);
	void Pop();
	void Free();
};


