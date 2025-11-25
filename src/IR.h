#pragma once
#include "ConstVal.h"
#include "Module.h"
#include "Parser.h"
#include "Stack.h"

enum op
{
	OP_NOP,
	OP_CONST,
	OP_NULL,
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

	// Right = Expression
	OP_BITNOT,

	// Left = Target Register
	// Right = Source Type
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
	OP_CALL,
	OP_INDEX,
	OP_ARRAYLIST,
	OP_MEMSET,

	// BigRegister = size to allocate
	OP_ALLOCGLOBAL,
	OP_DEBUGINFO,
	OP_SWITCHINT,
	OP_SPILL,
	OP_COPYPHYSICAL,
	OP_COPYTOPHYSICAL,
	OP_PTRDIFF,
	OP_ZEROUT,

	// Result = time
	OP_RDTSC,

	// BigRegister = ir_memcmp *
	// Result = true if success, false otherwise
	OP_MEMCMP,

	// Left = dst
	// Right = src,
	OP_MEMCPY,

	// Left = type_table_reg
	// Right = idx
	// Type = TypeInfo
	OP_TYPEINFO,

	// Right = Member Idx;
	OP_ENUM_ACCESS,

	// Right = Block
	OP_RUN,

	// Signals the result of an expression for the interpreter
	// Right = register
	OP_RESULT,
	
	// Ptr = symbol*
	OP_GLOBAL,

	// Right = Ignore by interpreter bool
	OP_UNREACHABLE,

	OP_DEBUG_BREAK,

	OP_FENCE,

	OP_ATOMIC_ADD,
	OP_ATOMIC_LOAD,

	OP_CMPXCHG,

	// SIMD
	// Ptr = ir_insert
	OP_INSERT,

	// Left = Vec Register
	// Right = Idx (u64)
	OP_EXTRACT,

	// Left = Source Type
	// Right = Target Register
	OP_BITCAST,

	// Right = Target Register
	OP_PTRCAST,


	OP_COUNT,
};

struct ir_insert
{
	u32 Register;
	u32 Idx;
	u32 ValueRegister;
};

struct ir_memcmp
{
	u32 LeftPtr;
	u32 RightPtr;
	u32 Count;
};

struct ir_switchint
{
	slice<u32> Cases;
	slice<u32> OnValues;
	u32 Default;
	u32 Matcher;
	u32 After;
};

enum ir_debug_type
{
	IR_DBG_VAR,
	IR_DBG_SCOPE,
	IR_DBG_ERROR_INFO,
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
			const error_info *ErrorInfo;
		} err_i;
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
		void *Ptr;
	};
	u32 Result;
	u32 Type;
	op Op;
};

struct call_info
{
	u32 Operand;
	slice<u32> Args;
	const error_info *ErrorInfo;
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

struct run_location
{
	uint BlockID;
	uint Index;
};

enum load_as
{
	LoadAs_Normal,
	LoadAs_Int,
	LoadAs_MultiInt,
	LoadAs_Floats,
};

struct arg_location
{
	load_as Load;
	int Start;
	int Count;
};

struct function
{
	const string *Name;
	const string *LinkName;
	ir *IR;
	dynamic<basic_block> Blocks;
	slice<symbol> ModuleSymbols;
	slice<run_location> Runs;
	slice<arg_location> Args;
	string ModuleName;
	u32 LineNo;
	u32 LastRegister;
	u32 Type;
	b32 NoDebugInfo;
	b32 FakeFunction;
	b32 ReturnPassedInPtr;
};

struct defer_scope
{
	dynamic<dynamic<node *>> Expressions;
};

struct profiling
{
	u32 CallbackType;
	u32 StartTime;  // Int Register
	node *Callback;
};

struct yield_info
{
	u32 ToBlockID;
	u32 ValueStore;
};

struct block_builder
{
	basic_block CurrentBlock;
	function *Function; // @NOTE: DO NOT USE AFTER THE FUNCTION IS GENERATED
	slice<import> Imported;
	stack<defer_scope> Defered;
	module *Module;
	profiling *Profile;
	stack<dict<symbol>> Scope;
	stack<yield_info> YieldReturn;
	dynamic<run_location> RunIndexes;
	const error_info *LastErrorInfo;
	u32 BreakBlockID;
	u32 ContinueBlockID;
	u32 LastRegister;
	u32 LastBlock;
	b32 IsGlobal;
};

struct ir_global
{
	const symbol *s;
	function Init;
	const_value Value;
};

struct ir
{
	dynamic<function> Functions;
	dynamic<ir_global> Globals;
	dynamic<function> GlobalRuns;
	u32 MaxRegisters;
};

ir BuildIR(file *File);
void BuildEnumIR();
u32 BuildStringCompare(block_builder *Builder, u32 Left, u32 Right, b32 IsNeq=false);
instruction Instruction(op Op, u64 Val, u32 Type, block_builder *Builder);
instruction Instruction(op Op, u32 Left, u32 Right, u32 Type, block_builder *Builder);
inline instruction Instruction(op Op, u32 Left, u32 Right, u32 ResultRegister, u32 Type);
u32 PushInstruction(block_builder *Builder, instruction I);
u32 BuildIRFromExpression(block_builder *Builder, node *Node, b32 IsLHS = false, b32 NeedResult = true);
function BuildFunctionIR(ir *IR, dynamic<node *> &Body, const string *Name, u32 TypeIdx, slice<node *> &Args, node *Node,
		slice<import> Imported);
b32 CanGetPointerAfterSize(const type *T, int Size);
void PushErrorInfo(block_builder *Builder, node *Node);
u32 BuildIRStoreVariable(block_builder *Builder, u32 Expression, u32 TypeIdx);
void BuildIRFunctionLevel(block_builder *Builder, node *Node);
int GetPointerPassIdx(u32 TypeIdx, uint Size);

void GetUsedRegisters(instruction I, dynamic<u32> &out);
u32 FixFunctionTypeForCallConv(u32 TIdx, dynamic<arg_location> &Loc, b32 *RetInPtr);

string Dissasemble(ir *IR);
string DissasembleFunction(function Fn, int indent);
void DissasembleInstruction(string_builder *Builder, instruction Instr);

