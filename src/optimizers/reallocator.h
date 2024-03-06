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
	int StartPoint;
	int EndPoint;
	int SpillAt;
	int Register;
};

struct register_tracker
{
	life_span **Registers;
	u32 UsedRegisters;
};

void AllocateRegisters(ir *IR, register_list List);

