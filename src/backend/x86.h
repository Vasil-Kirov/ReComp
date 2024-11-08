#pragma once
#include "../x64CodeWriter.h"
#include "Dynamic.h"

struct stack_memory {
	u32 VirtualRegister;
	u32 Location;
};

struct vreg_location {
	b32 Spilled;
	union {
		int StackIdx;
		uint PhyReg;
	};
};

struct code {
	dynamic<stack_memory> Stack;
	u32 StackEnd;
	assembler Asm;
	array<vreg_location> Locations;
};

// simple move encoding:
// OP_MOV (0x89) 0b(2 bits MOD)(3 bits src)(3 bits dst)
//


