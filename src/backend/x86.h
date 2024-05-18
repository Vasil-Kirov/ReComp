#pragma once
#include "../IR.h"
#include "../optimizers/reallocator.h"

enum x86_registers {
	Register_A = 0b000,
	Register_B = 0b011,
	Register_C = 0b001,
	Register_D = 0b010,
	Register_SP = 0b100,
	Register_BP = 0b101,
	Register_SI = 0b110,
	Register_DI = 0b111,
};

struct stack_memory {
	u32 VirtualRegister;
	u32 Location;
};

struct code {
	stack_memory *Stack;
	u32 StackCount;
	u32 StackEnd;
	u8 *Data;
	u32 Size;
};

enum MOD {
	MOD_displacement_0   = 0b00,
	MOD_displacement_i8  = 0b01,
	MOD_displacement_i32 = 0b10,
	MOD_register         = 0b11,
};

enum REX {
	REX_none = 0b0100'0000,
	REX_W    = 0b0100'1000,
	REX_R    = 0b0100'0100,
	REX_X    = 0b0100'0010,
	REX_B    = 0b0100'0001,
};

// simple move encoding:
// OP_MOV (0x89) 0b(2 bits MOD)(3 bits src)(3 bits dst)
//

void x86Generate(ir *IR);
void GenerateInstruction(code *Code, const instruction *I, function *Fn, u32 InstructionIndex);


