#include "x86.h"
#include "x64CodeWriter.h"
#include "RegAlloc.h"
#include <Memory.h>
#include <Type.h>
#include <IR.h>
#include <Semantics.h>

#define VerifyLocation(Out, VReg) Assert(Code->Locations[VReg].Spilled == false); uint Out = Code->Locations[VReg].PhyReg

register_ ToReg(uint PhyReg)
{
	// @Note: might have to do mapping at some point
	// for now it's just a cast
	return (register_)PhyReg;
}

void GenerateInstruction(code *Code, const instruction *I, function *Fn, u32 InstructionIndex)
{
	const type *Type = GetType(I->Type);
	switch(I->Op)
	{
		case OP_NOP: break;
		case OP_ALLOC: break;
		case OP_RESULT: break;
		case OP_RUN: break;
		case OP_GLOBAL:
		{
			const symbol *s = (const symbol *)I->Ptr;
			// @TODO: Relocation here
		} break;
		case OP_RDTSC:
		{
			Code->Asm.RDTSC();
		} break;
		case OP_DEBUG_BREAK:
		{
			Code->Asm.DebugTrap();
		} break;
		case OP_ATOMIC_ADD:
		{

		} break;
		case OP_STORE:
		{
			//stack_memory *s = GetStackLocation(Code, I->Left);
			//VerifyLocation(Loc, I->Right);
			//Code->Asm.Mov64(OffsetOperand(reg_sp, -s->Location), RegisterOperand(ToReg(Loc)));
		} break;
		case OP_LOAD:
		{
			const type *T = GetType(I->Type);
			if(IsLoadableType(T))
			{
			}
			else if(T->Kind == TypeKind_Function)
			{

			}
			else
			{

			}
		} break;
		case OP_ADD:
		{
		} break;
		case OP_SUB:
		{
		} break;
		case OP_MUL:
		{
		} break;
		case OP_DIV:
		{
		} break;
		case OP_MOD:
		{
		} break;
		case OP_CONST:
		{
		} break;
	}
}

code GenerateFunction(function *Fn)
{
	scratch_arena Arena = {};
	code Code = {
		.Locations = {Arena.Allocate(sizeof(vreg_location) * Fn->LastRegister), Fn->LastRegister},
	};
	ForN(Fn->Blocks, B)
	{
		For(B->Code)
		{
			switch(it->Op)
			{
				case OP_ALLOC:
				{
					Code.StackEnd += GetTypeSize(it->Type);
					Code.Locations[it->Result].Spilled = true;
					Code.Locations[it->Result].StackOffset = Code.StackEnd;
				} break;
				case OP_LOAD:
				{
					Code.StackEnd += GetTypeSize(it->Type);
					Code.Locations[it->Result].Spilled = true;
					Code.Locations[it->Result].StackOffset = Code.StackEnd;
				} break;
				case OP_CMPXCHG:
				{
					LFATAL("cmpxchg not supported in custom backend!");
				} break;
				default: break;
			}
		}
	}

	ForArray(BlockIndex, Fn->Blocks)
	{
		basic_block *Block = Fn->Blocks.Data + BlockIndex;
		ForArray(InstrIdx, Block->Code)
		{
			GenerateInstruction(&Code, Block->Code.Data + InstrIdx, Fn, InstrIdx);
		}
	}
	return Code;
}

void x86Generate(ir *IR)
{

}

op_reg_usage OpUsagex86[OP_COUNT] = {};

void InitX86OpUsage()
{

	OpUsagex86[OP_NOP] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_INSERT] = { 0, OpFlag_ComplexArgOp };
	OpUsagex86[OP_EXTRACT] = { 0, 0 };
	OpUsagex86[OP_DEBUG_BREAK] = { 0, OpFlag_DstIsUnused | OpFlag_SpillEverything };
	OpUsagex86[OP_CMPXCHG] = { 0, OpFlag_ComplexArgOp | OpFlag_SpillEverything };
	OpUsagex86[OP_FENCE] = { 0, OpFlag_DstIsUnused | OpFlag_SpillEverything };
	OpUsagex86[OP_ATOMIC_LOAD] = { 0, OpFlag_ComplexArgOp | OpFlag_SpillEverything };
	OpUsagex86[OP_ATOMIC_ADD] = { 0, OpFlag_ComplexArgOp | OpFlag_SpillEverything };
	OpUsagex86[OP_GLOBAL] = { 0, 0 };
	OpUsagex86[OP_RESULT] = { 0, 0 };
	OpUsagex86[OP_ENUM_ACCESS] = { 0, 0 };
	OpUsagex86[OP_TYPEINFO] = { 0, 0 };
	OpUsagex86[OP_RDTSC] = { 2, OpFlag_DstIsFixed, 0, 0, {RD, RA} };
	OpUsagex86[OP_MEMCMP] = { 0, OpFlag_ComplexArgOp };
	OpUsagex86[OP_ZEROUT] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_CONSTINT] = { 0, 0 };
	OpUsagex86[OP_NULL] = { 0, 0 };
	OpUsagex86[OP_CONST] = { 0, OpFlag_ComplexArgOp };
	OpUsagex86[OP_FN] = { 0, OpFlag_ComplexArgOp };
	OpUsagex86[OP_ADD] = { 0, 0 };
	OpUsagex86[OP_SUB] = { 0, 0 };
	OpUsagex86[OP_MUL] = { 0, 0 };
	OpUsagex86[OP_DIV] = { 2, OpFlag_DstIsFixed | OpFlag_LeftIsFixed, RA, 0, {RA, RD} };
	OpUsagex86[OP_MOD] = { 0, 0 };
	OpUsagex86[OP_LOAD] = { 0, 0 };
	OpUsagex86[OP_STORE] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_UNREACHABLE] = { 0, OpFlag_DstIsUnused | OpFlag_SpillEverything };
	OpUsagex86[OP_PTRCAST] = { 0, 0 };
	OpUsagex86[OP_MEMCPY] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_BITCAST] = { 0, 0 };
	OpUsagex86[OP_CAST] = { 0, 0 };
	OpUsagex86[OP_ALLOC] = { 0, 0 };
	OpUsagex86[OP_BITNOT] = { 0, 0 };
	OpUsagex86[OP_ALLOCGLOBAL] = { 0, 0 };
	OpUsagex86[OP_RET] = { 0, OpFlag_DstIsUnused | OpFlag_SpillEverything };
	OpUsagex86[OP_CALL] = { 0, OpFlag_ComplexArgOp | OpFlag_SpillEverything };
	OpUsagex86[OP_SWITCHINT] = { 0, OpFlag_ComplexArgOp };
	OpUsagex86[OP_RUN] = { 0, OpFlag_SpillEverything };
	OpUsagex86[OP_IF] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_JMP] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_INDEX] = { 0, 0 };
	OpUsagex86[OP_ARRAYLIST] = { 0, 0 };
	OpUsagex86[OP_MEMSET] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_PTRDIFF] = { 0, 0 };
	OpUsagex86[OP_ARG] = { 0, 0 };
	OpUsagex86[OP_NEQ] = { 0, 0 };
	OpUsagex86[OP_GREAT] = { 0, 0 };
	OpUsagex86[OP_GEQ] = { 0, 0 };
	OpUsagex86[OP_LESS] = { 0, 0 };
	OpUsagex86[OP_LEQ] = { 0, 0 };
	OpUsagex86[OP_SL] = { 0, 0 };
	OpUsagex86[OP_SR] = { 0, 0 };
	OpUsagex86[OP_EQEQ] = { 0, 0 };
	OpUsagex86[OP_AND] = { 0, 0 };
	OpUsagex86[OP_OR] = { 0, 0 };
	OpUsagex86[OP_XOR] = { 0, 0 };
	OpUsagex86[OP_DEBUGINFO] = { 0, OpFlag_DstIsUnused | OpFlag_SpillEverything };
	OpUsagex86[OP_SPILL] = { 0, OpFlag_DstIsUnused };
	OpUsagex86[OP_COPYTOPHYSICAL] = { 0, 0 };
	OpUsagex86[OP_COPYPHYSICAL] = { 0, 0 };
}

