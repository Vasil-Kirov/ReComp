#pragma once
#include "Module.h"
#include "Parser.h"
#include "Stack.h"

enum op
{
	OP_NOP,
	OP_CONST,
	OP_CONSTINT,
	OP_ARG,
	OP_ADD,
	OP_SUB,
	OP_DIV,
	OP_MUL,
	OP_MOD,
	OP_SL,
	OP_SR,
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_LOAD,
	OP_ALLOC,
	OP_STORE,
	OP_CAST,
	OP_FN,
	OP_RET,
	OP_IF,
	OP_JMP,
	OP_GREAT,
	OP_LESS,
	OP_NEQ,
	OP_GEQ,
	OP_LEQ,
	OP_EQEQ,
	OP_LAND,
	OP_LOR,
	OP_CALL,
	OP_INDEX,
	OP_ARRAYLIST,
	OP_MEMSET,
	OP_COUNT,
	OP_ALLOCGLOBAL,
	OP_DEBUGINFO,
	OP_SWITCHINT,
};

struct ir_switchint
{
	slice<u32> Cases;
	slice<u32> OnValues;
	u32 Matcher;
	u32 After;
};

enum ir_debug_type
{
	IR_DBG_VAR,
	IR_DBG_LOCATION,
	IR_DBG_SCOPE,
	IR_DBG_ARG,
};

struct ir_debug_info
{
	ir_debug_type type;
	union {
		struct {
			string Name;
			u32 Register;
			int LineNo;
			u32 TypeID;
		} var;
		struct {
			int LineNo;
		} loc;
		struct {
			string Name;
			u32 Register;
			int LineNo;
			int ArgNo;
			u32 TypeID;
		} arg;
	};
};

enum ir_symbol_flags
{
	IRSymbol_ExternFn = BIT(0),
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

struct call_info
{
	u32 Operand;
	slice<u32> Args;
};

struct array_list_info
{
	u32 Alloc;
	u32 *Registers;
	u32 Count;
};

struct basic_block
{
	dynamic<instruction> Code;
	u32 ID;
	b32 HasTerminator;
};

struct ir_symbol
{
	const string *Name;
	u32 Register;
	u32 Type;
	u32 Flags;
};

struct reg_allocation;

struct function
{
	const string *Name;
	const string *LinkName;
	dynamic<basic_block> Blocks;
	dynamic<ir_symbol> Locals;
	reg_allocation *Allocated;
	slice<ir_symbol> ModuleSymbols;
	string ModuleName;
	u32 LineNo;
	u32 LastRegister;
	u32 Type;
	b32 NoDebugInfo;
};

struct defer_scope
{
	dynamic<node *> Expressions;
};

struct block_builder
{
	basic_block CurrentBlock;
	function *Function; // @NOTE: DO NOT USE AFTER THE FUNCTION IS GENERATED
	slice<import> Imported;
	stack<defer_scope> Defered;
	module *Module;
	u32 BreakBlockID;
	u32 LastRegister;
	u32 LastBlock;
};

struct ir
{
	dynamic<function>Functions;
	slice<ir_symbol> GlobalSymbols;
	u32 MaxRegisters;
};

ir BuildIR(file *File, u32 LastRegister);
string Dissasemble(slice<function> Fn);
string DissasembleFunction(function Fn, int indent);
instruction Instruction(op Op, u64 Val, u32 Type, block_builder *Builder);
instruction Instruction(op Op, u32 Left, u32 Right, u32 Type, block_builder *Builder);
u32 PushInstruction(block_builder *Builder, instruction I);
u32 BuildIRFromExpression(block_builder *Builder, node *Node, b32 IsLHS = false, b32 NeedResult = true);
function BuildFunctionIR(dynamic<node *> &Body, const string *Name, u32 TypeIdx, slice<node *> &Args, node *Node,
		slice<import> Imported, u32 IRStartRegister);
void IRPushDebugLocation(block_builder *Builder, const error_info *Info);
u32 BuildIRStoreVariable(block_builder *Builder, u32 Expression, u32 TypeIdx);
void BuildIRFunctionLevel(block_builder *Builder, node *Node);

