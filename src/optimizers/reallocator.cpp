#include "reallocator.h"

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

void ExpireOldIntervals(life_span *New, register_tracker *Tracker, register_list List)
{
	for(int I = 0; I < List.RegisterCount; ++I)
	{
		if(Tracker->Registers[I]->EndPoint >= New->StartPoint)
			continue;

		Tracker->Registers[I] = NULL;
		Tracker->UsedRegisters--;
	}
}

void AllocateFunctionRegisters(function *Function, register_list List)
{
	register_tracker Tracker = {};
	Tracker.Registers = (life_span **)VAlloc(List.RegisterCount * sizeof(life_span *));

	life_span *Spans = (life_span *)VAlloc(Function->LastRegister * sizeof(life_span));
	memset(Spans, 0xFF, sizeof(life_span) * Function->LastRegister);
	FillLifeSpans(Spans, Function->Blocks, Function->BlockCount);
	qsort(Spans, Function->LastRegister, sizeof(life_span), CompareLifeSpans);

	int SpanCount = 0;
	for(int I = 0; I < Function->LastRegister; ++I)
	{
		life_span *Span = &Spans[I];
		if(Span->StartPoint == -1)
			break;

		SpanCount = I + 1;

		// @Note: release all registers that aren't refrenced again
		ExpireOldIntervals(Span, &Tracker, List);
		// @Note: if we have an available register we just take it, otherwise we spill the
		// register whose endpoint is the highest
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
	Function->Spans = Spans;
}

void AllocateRegisters(ir *IR, register_list List)
{
	auto FnCount = ArrLen(IR->Functions);
	for(int I = 0; I < FnCount; ++I)
	{
		AllocateFunctionRegisters(IR->Functions[I], List);
	}
}


