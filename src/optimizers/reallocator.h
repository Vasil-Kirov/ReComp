#pragma once
#include "../IR.h"


struct register_list
{
	int *Registers;
	u32 RegisterCount;
};

struct life_span
{
	u32 VirtualRegister;
	u32 Type;
	u32 StartPoint;
	u32 EndPoint;
};

struct register_tracker
{
	u32 *Registers;
	u32 UsedRegisters;
};

struct out_life_span
{
	u32 VirtualRegister;
	u32 Type;
	int Register;
	int SpillAt;
};

struct reg_allocation
{
	out_life_span *Spans;
	int Count;
};

void AllocateRegisters(ir *IR, register_list List);

