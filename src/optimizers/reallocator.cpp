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

int CompareSpills(void const *AIn, void const *BIn)
{
	life_span *A = *(life_span **)AIn;
	life_span *B = *(life_span **)BIn;
	return A->SpillAt - B->SpillAt;
}

void CopyInstructionUntil(instruction *NewInstructions, u32 *InstructionCountPtr, basic_block *Old, u32 From, u32 Until)
{
	u32 InstructionCount = *InstructionCountPtr;
	for(int I = From; I < Until; ++I)
	{
		NewInstruction[InstructionCount++] = Old->Code[I];
	}
	*InstructionCountPtr = InstructionCount;
}

instruction InstructionRealloc(function *Function, op Op, u32 Left, u32 Right, u32 Type)
{
	instruction R;
	R.Op = Op;
	R.Left = Left;
	R.Right = Right;
	R.Type = Type;
	R.Result = Function->LastRegister++;
}

u32 RewriteInstructionForSpilling(instruction *NewInstructions, basic_block *Block, life_span *Spans, int SpanCount, function *Function)
{
	int Start = 0;
	u32 NewInstructionCount = 0;
	life_span *Spills[SpanCount] = {};
	u32 SpillCount = 0;
	for(int I = 0; I < SpanCount; ++I)
	{
		if(Spans[I].SpillAt != -1)
		{
			Spills[SpillCount++] = Spans[I];
		}
	}
	if(SpillCount == 0)
	{
		memcpy(NewInstructions, Block->Code, Block->InstructionCount * sizeof(instruction));
		return Block->InstructionCount;
	}

	qsort(Spills, SpillCount, sizeof(life_span *), CompareSpills);

	for(int I = 0; I < SpillCount; ++I)
	{
		life_span *Spill = Spills[I];
		CopyInstructionUntil(NewInstructions, &NewInstructionCount, Block, Start, Spill->SpillAt);
		Start = Spill->SpillAt;
		instruction Alloc = InstructionRealloc(Functoin, OP_ALLOC, -1, -1, Spill->Type);
		instruction Store = InstructionRealloc(Function, OP_STORE, Alloc.Result, Spill->Register, Spill->Type);
		NewInstructions[NewInstructionCount++] = Alloc;
		NewInstructions[NewInstructionCount++] = Store;
	}
	CopyInstructionUntil(NewInstructions, &NewInstructionCount, Block, Start, Block->InstructionCount);
	return NewInstructionCount;
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

	for(int BlockIndex = 0; BlockIndex < Function->BlockCount; ++BlockIndex)
	{
		basic_block *Block = &Function->Blocks[BlockIndex];
		instruction *NewInstructions = (instruction *)VAlloc((Block->InstructionCount + 2) * sizeof(instruction) * 1.5);

		u32 NewInstructionCount = RewriteInstructionForSpilling(NewInstructions, Block, Spans, SpanCount);
		ArrFree(Block->Code);

		basic_block NewBlock;
		NewBlock.Code = NewInstructions;
		NewBlock.InstructionCount = NewInstructionCount;
		Function->Blocks[BlockIndex] = NewBlock;
	}

	VFree(Spans);
}

void AllocateRegisters(ir *IR, register_list List)
{

}


