#pragma once
#include "../x64CodeWriter.h"
#include "Dynamic.h"
#include "RegAlloc.h"

struct vreg_location {
	b32 Spilled;
	union {
		uint StackOffset;
		uint PhyReg;
	};
};

struct code {
	uint StackEnd;
	assembler Asm;
	array<vreg_location> Locations;
};

// simple move encoding:
// OP_MOV (0x89) 0b(2 bits MOD)(3 bits src)(3 bits dst)
//


void InitX86OpUsage();
extern op_reg_usage OpUsagex86[OP_COUNT];

