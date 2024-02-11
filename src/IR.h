#pragma once
#include "Parser.h"

enum op
{
	OP_NOP,
	OP_ADD,
	OP_SUB,
	OP_DIV,
	OP_MUL,
	OP_MOD,
};

struct instruction
{
	union {
		struct {
			u32 Left;
			u32 Right;
		};
		u64 BigRegister;
	};
	u32 Result;
	u32 Type;
	op Op;
};

struct basic_block
{
	instruction *Code;
	u32 InstructionCount;
};

struct ir_local
{
	const string *Name;
	u32 Register;
};

struct function
{
	const string *Name;
	basic_block *Blocks;
	u32 BlockCount;
	ir_local *Locals;
	u32 LocalCount;
};

struct block_builder
{
	basic_block *CurrentBlock;
	function *Function;
	u32 LastRegister;
};

struct ir
{
	function *Functions;
};

ir BuildIR(node **Nodes);



