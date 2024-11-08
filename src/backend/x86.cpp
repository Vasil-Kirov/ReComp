#include "x86.h"
#include "x64CodeWriter.h"
#include <Memory.h>
#include <Type.h>
#include <IR.h>

#define VerifyLocation(Out, VReg) Assert(Code->Locations[VReg].Spilled == false); uint Out = Code->Locations[VReg].PhyReg

stack_memory *GetStackLocation(code *Code, u32 VirtualRegister)
{
	ForArray(i, Code->Stack)
	{
		if(Code->Stack[i].VirtualRegister == VirtualRegister)
			return &Code->Stack.Data[i];
	}
	unreachable;
}

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
		case OP_NOP: {}break;
		case OP_ALLOC:
		{
			stack_memory Slot = {};
			Slot.VirtualRegister = I->Result;
			Slot.Location = Code->StackEnd;
			Code->Stack.Push(Slot);
			Code->StackEnd += GetTypeSize(Type);
		} break;
		case OP_STORE:
		{
			stack_memory *s = GetStackLocation(Code, I->Left);
			VerifyLocation(Loc, I->Right);
			Code->Asm.Mov64(OffsetOperand(reg_sp, -s->Location), RegisterOperand(ToReg(Loc)));
		} break;
		case OP_LOAD:
		{
			const type *T = GetType(I->Type);
			if(IsLoadableType(T))
			{
				Code->Asm.Mov64(operand Dst, operand Src)
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

