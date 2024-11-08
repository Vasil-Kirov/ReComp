#pragma once
#include <IR.h>

struct reg_reserve_instruction
{
	op Op;
	slice<uint> regs;
};

struct reg_allocator
{
	slice<reg_reserve_instruction> reserved;
	uint reg_count;
};

struct lifespan
{
	uint Start;
	uint End;
	b32 Started;
};

struct tracker
{
	u32 VirtualRegister;
	lifespan Lifespan;
};

