#pragma once
#include "Parser.h"

enum op
{
	OP_NOP,
	OP_CONST,
	OP_ADD,
	OP_SUB,
	OP_DIV,
	OP_MUL,
	OP_MOD,
	OP_LOAD,
	OP_ALLOC,
	OP_STORE,
	OP_CAST, // @TODO: Actual casting, this is just for dissasembly
	OP_RET,
	OP_IF,
	OP_JMP,
	OP_NEQ,
	OP_GEQ,
	OP_LEQ,
	OP_EQEQ,
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
	u32 ID;
	b32 HasTerminator;
};

struct ir_local
{
	const string *Name;
	u32 Register;
	u32 Type;
};

struct reg_allocation;

struct function
{
	const string *Name;
	basic_block *Blocks;
	u32 BlockCount;
	ir_local *Locals;
	reg_allocation *Allocated;
	u32 LocalCount;
	u32 LastRegister;
	u32 Type;
};

struct block_builder
{
	basic_block *CurrentBlock;
	function *Function;
	u32 LastRegister;
};

struct ir
{
	function *Functions; // Dynamic Array
};

ir BuildIR(node **Nodes);
string Dissasemble(function *Fn);
instruction Instruction(op Op, u64 Val, u32 Type, block_builder *Builder);
instruction Instruction(op Op, u32 Left, u32 Right, u32 Type, block_builder *Builder);
u32 PushInstruction(block_builder *Builder, instruction I);
u32 BuildIRFromExpression(block_builder *Builder, node *Node, b32 IsLHS);



