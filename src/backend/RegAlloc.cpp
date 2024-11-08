#include "RegAlloc.h"
#include "IR.h"
#include "Memory.h"

b32 IsOpReserved(reg_allocator *r, op Op, slice<uint> *out_reg)
{
	ForArray(i, r->reserved)
	{
		if(r->reserved[i].Op == Op)
		{
			*out_reg = r->reserved[i].regs;
			return true;
		}
	}
	return false;
}

array<lifespan> CalculateLifeSpans(dynamic<basic_block> Blocks, u32 VirtualRegCount)
{
	scratch_arena s{};
	array LifeSpans = array<lifespan>(s.Allocate(VirtualRegCount * sizeof(lifespan)), VirtualRegCount);
	ForArray(bi, Blocks)
	{
		basic_block Block = Blocks[bi];
		ForArray(ii, Block.Code)
		{
			uint SpanNumber = (Block.ID * 1'000'000) + ii;

			instruction i = Block.Code[ii];
			dynamic<u32> Used = {};
			GetUsedRegisters(i, Used);
			ForArray(i, Used)
			{
				if(Used[i] == -1)
					continue;
				lifespan& Span = LifeSpans[Used[i]];
				if(Span.Started)
				{
					Span.End = SpanNumber;
				}
				else
				{
					Span.Start = SpanNumber;
					Span.Started = true;
				}
			}
		}
	}

	return LifeSpans;
}

void FreeVirtualRegister(array<tracker> &Tracking, u32 Register)
{
	ForArray(i, Tracking)
	{
		if(Tracking[i].VirtualRegister == Register)
		{
			Tracking[i].VirtualRegister = -1;
			return;
		}
	}
}

void SpillRegister(array<tracker> &Tracking, dynamic<instruction> &Code, u32 Register)
{
	instruction SpillI = {};
	SpillI.Op = OP_SPILL;
	SpillI.Right = Register;
	Code.Push(SpillI);
	FreeVirtualRegister(Tracking, Register);
}

void ToPhysicalRegister(array<tracker> &Tracking, dynamic<instruction> &Code, u32 Virtual, uint Physical, array<lifespan> &Lifespans)
{
	instruction ToPhyI = {};
	ToPhyI.Op = OP_TOPHYSICAL;
	ToPhyI.Right = Virtual;
	ToPhyI.Result = Physical;
	Code.Push(ToPhyI);
	Tracking[Physical].VirtualRegister = Virtual;
	Tracking[Physical].Lifespan = Lifespans[Virtual];
}

b32 IsVirtualRegisterAllocated(array<tracker> &Tracking, u32 VirtualReg)
{
	ForArray(i, Tracking)
	{
		if(Tracking[i].VirtualRegister == VirtualReg)
			return true;
	}
	return false;
}

uint GetPhysicalRegister(array<tracker> &Tracking, dynamic<instruction> &Code)
{
	ForArray(i, Tracking)
	{
		if(Tracking[i].VirtualRegister == -1)
			return i;
	}

	uint Latest = Tracking[0].Lifespan.End;
	uint LatestI = 0;
	ForArray(i, Tracking)
	{
		LDEBUG("[%d] %d >? [%d] %d", LatestI, Latest, i, Tracking[i].Lifespan.End);
		if(Tracking[i].Lifespan.End > Latest)
		{
			Latest = Tracking[i].Lifespan.End;
			LatestI = i;
		}
	}
	SpillRegister(Tracking, Code, Tracking[LatestI].VirtualRegister);
	return LatestI;
}

dynamic<instruction> AllocateRegistersForBasicBlock(reg_allocator *r, basic_block Block, array<lifespan> Lifespans)
{
	dynamic<instruction> Code = {};
	Assert(Block.HasTerminator);
	scratch_arena s{};
	array Tracking = array<tracker>(s.Allocate(r->reg_count * sizeof(tracker)), r->reg_count);
	ForArray(i, Tracking)
	{
		Tracking[i].VirtualRegister = -1;
	}

	ForArray(InstrIdx, Block.Code)
	{
		instruction i = Block.Code[InstrIdx];

		slice<uint> Reserved = {};
		if(IsOpReserved(r, i.Op, &Reserved))
		{
			ForArray(ri, Reserved)
			{
				if(Tracking[Reserved[ri]].VirtualRegister != -1)
				{
					SpillRegister(Tracking, Code, Tracking[Reserved[ri]].VirtualRegister);
				}
			}
			dynamic<u32> Used = {};
			GetUsedRegisters(i, Used);
			ForArray(j, Used)
			{
				if(Used[j] != -1)
				{
					if(Reserved[j] != -1)
					{
						u32 VToSpill = Tracking[Reserved[j]].VirtualRegister;
						SpillRegister(Tracking, Code, VToSpill);
						ToPhysicalRegister(Tracking, Code, Used[j], Reserved[j], Lifespans);
					}
					else
					{
						if(!IsVirtualRegisterAllocated(Tracking, Used[j]))
						{
							uint PhyReg = GetPhysicalRegister(Tracking, Code);
							ToPhysicalRegister(Tracking, Code, Used[j], PhyReg, Lifespans);
						}
					}
				}
			}
			Used.Free();
		}
		else
		{
			dynamic<u32> Used = {};

			GetUsedRegisters(i, Used);
			ForArray(j, Used)
			{
				if(Used[j] != -1 && !IsVirtualRegisterAllocated(Tracking, Used[j]))
				{
					uint PhyReg = GetPhysicalRegister(Tracking, Code);
					ToPhysicalRegister(Tracking, Code, Used[j], PhyReg, Lifespans);
				}
			}

			Used.Free();
		}

		Code.Push(i);
	}
	return Code;
}

void AllocateRegisters(reg_allocator *r, ir *IR)
{
	Assert(r->reg_count > 0);
	ForArray(FIdx, IR->Functions)
	{
		function *Fn = &IR->Functions.Data[FIdx];
		array Lifespans = CalculateLifeSpans(Fn->Blocks, Fn->LastRegister);
		ForArray(BIdx, Fn->Blocks)
		{
			dynamic<instruction> NewCode = AllocateRegistersForBasicBlock(r, Fn->Blocks[BIdx], Lifespans);
			Fn->Blocks.Data[BIdx].Code.Free();
			Fn->Blocks.Data[BIdx].Code = NewCode;
		}
	}
}

reg_allocator MakeRegisterAllocator(slice<reg_reserve_instruction> reserved, uint reg_count)
{
	Assert(reg_count > 0);
	return reg_allocator{reserved, reg_count};
}

