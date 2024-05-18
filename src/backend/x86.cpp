#include "x86.h"

code GenerateFunction(function *Fn)
{
	code Code = {};
	Code.Data = (u8 *)AllocatePermanent(KB(128));
	Code.Stack = (stack_memory *)AllocatePermanent(Fn->Allocated->Count * sizeof(stack_memory));
	for(int BlockIndex = 0; BlockIndex < Fn->BlockCount; ++BlockIndex)
	{
		basic_block *Block = Fn->Blocks + BlockIndex;
		for(int InstrIdx = 0; InstrIdx < Block->InstructionCount; ++InstrIdx)
		{
			GenerateInstruction(&Code, Block->Code + InstrIdx, Fn, InstrIdx);
		}
	}
	return Code;
}

void x86Generate(ir *IR)
{

}

void PushU8(code *Code, u8 Data)
{
	Code->Data[Code->Size++] = Data;
}

void PushU16(code *Code, u16 Data)
{
	*(u16 *)(Code->Data + Code->Size) = Data;
	Code->Size += 2;
}

void PushU32(code *Code, u32 Data)
{
	*(u32 *)(Code->Data + Code->Size) = Data;
	Code->Size += 4;
}

void PushU64(code *Code, u64 Data)
{
	*(u64 *)(Code->Data + Code->Size) = Data;
	Code->Size += 8;
}

void WriteSpill(code *Code, out_life_span *ToSpill)
{
	//Code->Stack[Code->StackCount++] = ToSpill->VirtualRegister;
}

void CheckSpills(code *Code, function *Fn, u32 CurrentInstructionIndex)
{
	for(int I = 0; I < Fn->Allocated->Count; ++I)
	{
		if(Fn->Allocated->Spans[I].SpillAt == CurrentInstructionIndex)
		{
			WriteSpill(Code, Fn->Allocated->Spans + I);
		}
	}
}

u32 GetRegister(code *Code, function *Fn, u32 Register, u32 InstructionIndex)
{
	for(int I = 0; I < Fn->Allocated->Count; ++I)
	{
		if(Fn->Allocated->Spans[I].VirtualRegister == Register)
		{
			if(InstructionIndex >= Fn->Allocated->Spans[I].SpillAt)
			{
				// @TODO: Work here
			}
		}
	}
}

stack_memory *GetStackLocation(code *Code, u32 VirtualRegister)
{
	for(int I = 0; I < Code->StackCount; ++I)
	{
		if(Code->Stack[I].VirtualRegister == VirtualRegister)
			return &Code->Stack[I];
	}
	Assert(false);
	return NULL;
}

void GenerateInstruction(code *Code, const instruction *I, function *Fn, u32 InstructionIndex)
{
	CheckSpills(Code, Fn, InstructionIndex);
	const type *Type = GetType(I->Type);
	switch(I->Op)
	{
		case OP_NOP: {}break;
		case OP_ALLOC:
		{
			Code->Stack[Code->StackCount].VirtualRegister = I->Result;
			Code->Stack[Code->StackCount].Location = Code->StackEnd;
			Code->StackCount++;
			Code->StackEnd += GetTypeSize(Type);
		} break;
		case OP_STORE:
		{
			stack_memory *S = GetStackLocation(Code, I->Left);
			GetRegister(Code, Fn, I->Right, InstructionIndex);
		} break;
		case OP_LOAD:
		{
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

