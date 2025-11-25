#include "RegAlloc.h"
#include "IR.h"
#include "Memory.h"
#include "StackAllocator.h"

array<lifespan> ComputeLifeSpans(reg_allocator *r, dynamic<basic_block> Blocks, void *Memory, u32 VirtualRegCount)
{
	uint Lifetime = 0;

	auto LifeSpans = array<lifespan>(Memory, VirtualRegCount);
	ForArray(bi, Blocks)
	{
		basic_block Block = Blocks[bi];
		ForArray(ii, Block.Code)
		{
			instruction i = Block.Code[ii];
			op_reg_usage Usage = r->RegUsage[i.Op];
			auto lambda = [Usage, &LifeSpans](u32 vreg, op_flags Unused, uint Lifetime) {
				if(!HAS_FLAG(Usage.Flags, Unused))
				{
					lifespan& Span = LifeSpans[vreg];
					if(Span.Start != UINT_MAX)
					{
						Span.End = Lifetime;
					}
					else
					{
						Span.Start = Lifetime;
					}
				}
			};

			lambda(i.Left, OpFlag_LeftIsUnused, Lifetime);
			lambda(i.Right, OpFlag_RightIsUnused, Lifetime);
			lambda(i.Result, OpFlag_DstIsUnused, Lifetime);
			Lifetime++;
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
	ToPhyI.Op = OP_COPYPHYSICAL;
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
	auto Tracking = array<tracker>(s.Allocate(r->reg_count * sizeof(tracker)), r->reg_count);
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

slice<fixed_register_internval> PlaceRegisterConstraints(reg_allocator *r, function *Fn)
{
	dynamic<fixed_register_internval> Intervals = {};

	ForArray(BlockIdx, Fn->Blocks)
	{
		dynamic<instruction> FixedInstructions = {};
		auto Block = Fn->Blocks[BlockIdx];
		ForArray(InstrIdx, Block.Code)
		{
			uint Lifetime = Intervals.Count;

			auto i = Block.Code.Data[InstrIdx];
			if(i.Op == OP_ARG)
			{
					auto Arg = i.BigRegister;
					if(Arg < r->FnArgsRegisters.Count)
					{
						uint Register = r->FnArgsRegisters[Arg];
						Intervals.Push((fixed_register_internval){Register, 0, Lifetime});

						auto NewInstr = Instruction(OP_COPYPHYSICAL, Register, 0, i.Result, i.Type);
						FixedInstructions.Push(NewInstr);
					}
					else
					{
						FixedInstructions.Push(i);
					}
			}
			else
			{
				auto Usage = r->RegUsage[i.Op];
				u32 ResultVReg = i.Result;
				uint LeftIntervalIdx = -1;
				uint RightIntervalIdx = -1;
				if(HAS_FLAG(Usage.Flags, OpFlag_LeftIsFixed))
				{
					LeftIntervalIdx = Intervals.Count;

					auto NewInstr = Instruction(OP_COPYTOPHYSICAL, i.Left, 0, Usage.LeftFixed, i.Type);
					FixedInstructions.Push(NewInstr);

					Intervals.Push((fixed_register_internval){Usage.LeftFixed, Lifetime, (uint)-1});
					Lifetime++;
					i.Left = Usage.LeftFixed;
				}
				if(HAS_FLAG(Usage.Flags, OpFlag_RightIsFixed))
				{
					RightIntervalIdx = Intervals.Count;

					auto NewInstr = Instruction(OP_COPYTOPHYSICAL, i.Right, 0, Usage.RightFixed, i.Type);
					FixedInstructions.Push(NewInstr);

					Intervals.Push((fixed_register_internval){Usage.RightFixed, Lifetime, (uint)-1});
					Lifetime++;
					i.Right = Usage.RightFixed;
				}
				if(HAS_FLAG(Usage.Flags, OpFlag_DstIsFixed))
				{
					i.Result = -1;
				}
				FixedInstructions.Push(i);
				if(LeftIntervalIdx != -1)
				{
					Intervals.Data[LeftIntervalIdx].End = Lifetime;
				}
				if(RightIntervalIdx != -1)
				{
					Intervals.Data[RightIntervalIdx].End = Lifetime;
				}

				if(HAS_FLAG(Usage.Flags, OpFlag_DstIsFixed))
				{
					uint ResultStart = Lifetime++;
					for(int Iterator = 0; Iterator < Usage.ResultsFixedCount; ++Iterator)
					{
						uint Fixed = Usage.ResultsFixed[Iterator];
						auto NewInstr = Instruction(OP_COPYPHYSICAL, Fixed, 0, ResultVReg, i.Type);
						FixedInstructions.Push(NewInstr);

						if(Fixed == Usage.LeftFixed)
						{
							Intervals.Data[LeftIntervalIdx].End = Lifetime;
						}
						else if(Fixed == Usage.RightFixed)
						{
							Intervals.Data[RightIntervalIdx].End = Lifetime;
						}
						else
						{
							Intervals.Push((fixed_register_internval){Fixed, ResultStart, Lifetime});
						}

						Lifetime++;
					}
				}
			}
		}
		Block.Code.Free();
		Block.Code = FixedInstructions;
	}
	return SliceFromArray(Intervals);
}

void AllocateRegisters(reg_allocator *r, ir *IR)
{
	stack_alloc Allocator{};
	Assert(r->PhysicalRegs > 0);
	ForArray(FIdx, IR->Functions)
	{
		function *Fn = &IR->Functions.Data[FIdx];

		auto Fixed = PlaceRegisterConstraints(r, Fn);

		void *LifespanMemory = Allocator.Push(Fn->LastRegister * sizeof(lifespan));
		auto Lifespans = ComputeLifeSpans(r, Fn->Blocks, LifespanMemory, Fn->LastRegister);

		ForArray(BIdx, Fn->Blocks)
		{
			dynamic<instruction> NewCode = AllocateRegistersForBasicBlock(r, Fn->Blocks[BIdx], Lifespans);
			Fn->Blocks.Data[BIdx].Code.Free();
			Fn->Blocks.Data[BIdx].Code = NewCode;
		}

		Allocator.Pop();
	}

	Allocator.Free();
}

reg_allocator MakeRegisterAllocator(slice<op_reg_usage> OpUsage, uint PhysicalRegisterCount, slice<uint> FnArgsRegisters)
{
	Assert(PhysicalRegisterCount > 0);
	return reg_allocator{OpUsage, FnArgsRegisters, PhysicalRegisterCount};
}

