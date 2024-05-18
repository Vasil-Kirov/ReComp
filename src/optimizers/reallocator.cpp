#include "reallocator.h"

void FillLifeSpans(life_span *Spans, basic_block *Blocks, u32 BlockCount)
{
	for(int BlockIndex = 0; BlockIndex < BlockCount; ++BlockIndex)
	{
		basic_block *Block = &Blocks[BlockIndex];
		for(int InstructionIndex = 0; InstructionIndex < Block->InstructionCount; ++InstructionIndex)
		{
			instruction *I = &Block->Code[InstructionIndex];
			if(I->Op == OP_ALLOC)
			{
				Assert(Spans[I->Result].StartPoint == -1);
				Spans[I->Result].VirtualRegister = I->Result;
				Spans[I->Result].StartPoint      = InstructionIndex;
				Spans[I->Result].EndPoint        = InstructionIndex;
				Spans[I->Result].Type            = I->Type;
			}
			else if(I->Op == OP_LOAD)
			{
				// @TODO: I DONT WANT TO THINK ABOUT THIS RIGHT NOW
				Spans[I->BigRegister].EndPoint = InstructionIndex;
			}
		}
	}
}

int CompareLifeSpans(void const *AIn, void const *BIn)
{
	life_span *A = (life_span *)AIn;
	life_span *B = (life_span *)BIn;
	return A->StartPoint > B->StartPoint;
}

u32 GetFirstRegister(register_tracker *Tracker, int Index, u32 RegisterCount)
{
	for(int I = 0; I < RegisterCount; ++I)
	{
		if(Tracker->Registers[I] == -1)
		{
			Tracker->Registers[I] = Index;
			Tracker->UsedRegisters++;
			return I;
		}
	}
	return -1;
}

u32 FindSpillingRegister(life_span *Spans, register_tracker *Tracker, u32 RegisterCount)
{
	Assert(Tracker->UsedRegisters >= RegisterCount);
	u32 Latest = 0;
	for(int I = 0; I < RegisterCount; ++I)
	{
		Assert(Tracker->Registers[I]);
		if(Spans[Tracker->Registers[I]].EndPoint > Spans[Tracker->Registers[Latest]].EndPoint)
		{
			Latest = I;
		}
	}
	return Latest;
}

void ExpireOldIntervals(life_span *Spans, int Index, register_tracker *Tracker, register_list List)
{
	for(int I = 0; I < List.RegisterCount; ++I)
	{
		if(Spans[Tracker->Registers[I]].EndPoint >= Spans[Index].StartPoint)
			continue;

		Tracker->Registers[I] = -1;
		Tracker->UsedRegisters--;
	}
}

void AllocateFunctionRegisters(function *Function, register_list List)
{
	register_tracker Tracker = {};
	Tracker.Registers = (u32 *)VAlloc(List.RegisterCount * sizeof(u32));

	out_life_span *Result = (out_life_span *)AllocateMemory(Function->LastRegister * sizeof(out_life_span));

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
		Result[I].Type = Span->Type;

		// @Note: release all registers that aren't refrenced again
		ExpireOldIntervals(Spans, I, &Tracker, List);
		// @Note: if we have an available register we just take it, otherwise we spill the
		// register whose endpoint is the highest
		if(Tracker.UsedRegisters < List.RegisterCount)
		{
			u32 Register = GetFirstRegister(&Tracker, I, List.RegisterCount);
			Assert(Register != -1);
			Result[I].Register = Register;
		}
		else
		{
			u32 ToSpill = FindSpillingRegister(Spans, &Tracker, List.RegisterCount);
			Result[Tracker.Registers[ToSpill]].SpillAt = Span->StartPoint;

			Tracker.Registers[ToSpill] = I;
			Result[I].Register = ToSpill;
		}
	}
	VFree(Tracker.Registers);
	VFree(Spans);

	Function->Allocated = NewType(reg_allocation);
	Function->Allocated->Spans = Result;
	Function->Allocated->Count = SpanCount;
}

void AllocateRegisters(ir *IR, register_list List)
{
	auto FnCount = ArrLen(IR->Functions);
	for(int I = 0; I < FnCount; ++I)
	{
		AllocateFunctionRegisters(&IR->Functions[I], List);
	}
}


