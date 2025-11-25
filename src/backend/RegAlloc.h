#pragma once
#include <IR.h>

enum op_flags : u8
{
	OpFlag_SpillEverything = BIT(0),
	OpFlag_LeftIsFixed     = BIT(1),
	OpFlag_RightIsFixed    = BIT(2),
	OpFlag_DstIsFixed      = BIT(3),
	OpFlag_LeftIsUnused    = BIT(4),
	OpFlag_RightIsUnused   = BIT(5),
	OpFlag_DstIsUnused     = BIT(6),
	OpFlag_ComplexArgOp    = BIT(7),
};

struct op_reg_usage
{
	u8 ResultsFixedCount;
	op_flags Flags;

	uint LeftFixed;
	uint RightFixed;
	uint ResultsFixed[2];
};

struct reg_allocator
{
	slice<op_reg_usage> RegUsage; // Indexed with OP_...
	slice<uint> FnArgsRegisters;
	uint PhysicalRegs;
};

struct fixed_register_internval
{
	uint Register;
	uint Start;
	uint End;
};

struct lifespan
{
	uint Start;
	uint End;
};

struct tracker
{
	u32 VirtualRegister;
	lifespan Lifespan;
};

