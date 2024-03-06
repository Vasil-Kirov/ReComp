#include "reallocator.h"

void RewriteBasicBlockRegisters(basic_block *Block, register_list List, life_span *LifeSpans, register_tracker *Tracker)
{
	
}

void FillLifeSpans(life_span *Spans, basic_block *Blocks, u32 BlockCount)
{
	for(int BlockIndex = 0; BlockIndex < BlockCount; ++BlockIndex)
	{
		basic_block *Block = &Blocks[I];
		for(int InstructionIndex = 0; InstructionIndex < Block->InstructionCount; ++InstructionIndex)
		{
			instruction *I = &Block->Code[InstructionIndex];
			if(I->Op == OP_ALLOC)
			{
				Assert(Spans[I->Result].StartPoint == -1);
				Spans[I->Result].VirtualRegister = I->Result;
				Spans[I->Result].StartPoint = InstructionIndex;
				Spans[I->Result].EndPoint = InstructionIndex;
			}
			else if(I->Op == OP_LOAD)
			{
				Assert(Spans[I->Result].EndPoint == -1);
				Spans[I->Result].EndPoint = InstructionIndex;
			}
			else if(I->Op == OP_STORE)
			{
				Assert(Spans[I->Result].EndPoint == -1);
				Spans[I->Result].EndPoint = InstructionIndex;
			}
		}
	}
}

int CompareLifeSpans(void const *AIn, void const *BIn)
{
	life_span *A = (life_span *)AIn;
	life_span *B = (life_span *)BIn;
	return A->StartPoint - B->StartPoint;
}

inline int GetActiveLength(life_span *Span)
{
	return Span->EndPoint - Span->StartPoint;
}

u32 GetFirstRegister(register_tracker *Tracker, life_span *Span, u32 RegisterCount)
{
	for(int I = 0; I < RegisterCount; ++I)
	{
		if(Tracker->Registers[I] == NULL)
		{
			Tracker->Registers[I] = Span;
			Tracker->UsedRegisters++;
			return I;
		}
	}
	return -1;
}

u32 FindSpillingRegister(register_tracker *Tracker, u32 RegisterCount)
{
	Assert(Tracker->UsedRegisters >= RegisterCount);
	u32 Latest = 0;
	for(int I = 0; I < RegisterCount; ++I)
	{
		Assert(Tracker->Registers[I]);
		if(Tracker->Registers[I]->EndPoint > Tracker->Register[Latest]->EndPoint)
		{
			Latest = I;
		}
	}
	return I;
}

void AllocateFunctionRegisters(function *Function, register_list List)
{
	register_tracker Tracker = {};
	life_span *Spans = (life_span *)VAlloc(Function->LastRegister * sizeof(life_span));
	memset(Spans, 0xFF, sizeof(life_span) * Function->LastRegister);
	FillLifeSpans(Spans, Function->Blocks, Function->BlockCount);
	qsort(Spans, Function->LastRegister, sizeof(life_span), CompareLifeSpans);
	for(int I = 0; I < Function->LastRegister; ++I)
	{
		life_span *Span = &Spans[I];
		if(Span->StartPoint == -1)
			break;

		if(Tracker.UsedRegisters < List.RegisterCount)
		{
			u32 Register = GetFirstRegister(&Tracker, Span, List.RegisterCount);
			Assert(Register != -1);
			Span->Register = Register;
		}
		else
		{
			u32 ToSpill = FindSpillingRegister(Tracker, List.RegisterCount);
			Tracker.Registers[ToSpill]->SpillAt = Span->StartPoint;

			Tracker->Registers[ToSpill] = Span;
			Span->Register = ToSpill;
		}
	}
	for(int BlockIndex = 0; BlockIndex < BlockCount; ++BlockIndex)
	{
		AllocateBasicBlockRegisters(&Function->Blocks[BlockIndex], List, Spans);
	}


	VFree(Spans);
}

void AllocateRegisters(ir *IR, register_list List)
{

}


