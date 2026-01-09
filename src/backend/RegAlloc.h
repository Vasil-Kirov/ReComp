#pragma once
#include <IR.h>

enum op_flags : u8
{
	OpFlag_SpillEverything = BIT(0),
	OpFlag_LeftIsFixed     = BIT(1),
	OpFlag_RightIsFixed    = BIT(2),
	OpFlag_DstIsFixed      = BIT(3),
	OpFlag_DstIsUnused     = BIT(4),
	OpFlag_ComplexArgOp    = BIT(5),
};

struct op_reg_usage
{
	u8 ResultsFixedCount;
	u8 Flags;

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

struct fixed_register_interval
{
	uint Register;
	uint Start;
	uint End;
	u32 VirtualReg;
};

enum RegisterIR {
	RA,
	RB,
	RC,
	RD,
	RSI,
	RDI,
	R8,
	R9
};

/*
 * x64 registers
 *
 * rax = 0
 * rbx = 1
 * rcx = 2
 * rdx = 3
 * rsi = 4
 * rdi = 5
 * r8 = 6
 * r9 = 7
 * r10 = 8
 * r11 = 9
 * r12 = 10
 * rbp = 11
 * rsp = 12
 *
 *
 */

