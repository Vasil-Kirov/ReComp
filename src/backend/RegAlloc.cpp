#include "RegAlloc.h"
#include "IR.h"
#include "StackAllocator.h"
#include "Type.h"
#include <unordered_set>

const char *phyregs[] = {
	"rax",
	"rbx",
	"rcx",
	"rdx",
	"rsi",
	"rdi",
	"r8 ",
	"r9 ",
	"r10",
	"r11",
	"r12",
	"rbp",
	"rsp",
};

struct liveness
{
	slice<u32> Gen;
	slice<u32> Kill;
};

struct successor_block
{
	basic_block Block;
	dynamic<u32> Predecessors;
	slice<u32> Successors;
};

void PostorderSort(array<successor_block> Blocks, u32 BlockID, std::unordered_set<u32> &Visited, dynamic<successor_block> &Sort)
{
	if(Visited.count(BlockID))
		return;

	Visited.insert(BlockID);

	For(Blocks[BlockID].Successors)
	{
		PostorderSort(Blocks, *it, Visited, Sort);
	}

	Sort.Push(Blocks[BlockID]);
}

slice<successor_block> SortBasicBlocks(slice<basic_block> Blocks)
{
	array<successor_block> SBlocks(Blocks.Count);

	For(SBlocks)
		*it = {};

	ForArray(BlockIdx, Blocks)
	{
		auto Block = Blocks[BlockIdx];
		slice<u32> Successors = {};
		if(Block.Code.Count > 0)
		{
			auto i = Block.Code[Block.Code.Count-1];
			switch(i.Op)
			{
				case OP_JMP:
				{
					Successors = SliceFromConst({(u32)i.BigRegister});
					SBlocks[i.BigRegister].Predecessors.Push(Block.ID);
				} break;
				case OP_IF:
				{
					Successors = SliceFromConst({i.Left, i.Right});
					SBlocks[i.Left].Predecessors.Push(Block.ID);
					SBlocks[i.Right].Predecessors.Push(Block.ID);
				} break;
				case OP_SWITCHINT:
				{
					ir_switchint *Info = (ir_switchint *)i.Ptr;
					array<u32> SArray(Info->Cases.Count + (Info->Default != -1 ? 1 : 0));
					ForArray(CaseIdx, Info->Cases)
					{
						SArray[CaseIdx] = Info->Cases[CaseIdx];
						SBlocks[Info->Cases[CaseIdx]].Predecessors.Push(Block.ID);
					}
					if(Info->Default != -1)
					{
						SArray[Info->Cases.Count] = Info->Default;
						SBlocks[Info->Default].Predecessors.Push(Block.ID);
					}
					Successors = SliceFromArray(SArray);
				} break;
				default: {}break;
			}
		}
		SBlocks[BlockIdx].Block = Block;
		SBlocks[BlockIdx].Successors = Successors;
	}

	

	std::unordered_set<u32> Visited = {};
	dynamic<successor_block> SortDyn = {};
	PostorderSort(SBlocks, 0, Visited, SortDyn);

	array<successor_block> Sort(SortDyn);
	Sort.Reverse();

	return SliceFromArray(Sort);
}

liveness ComputeLivenessSetForBlock(reg_allocator *r, basic_block Block)
{
	u32 Used[1024];
	dynamic<u32> Gen = {};
	dynamic<u32> Kill = {};
	ForReverse(Block.Code)
	{
		op_reg_usage Usage = r->RegUsage[it->Op];
		if(!HAS_FLAG(Usage.Flags, OpFlag_DstIsUnused))
		{
			Kill.Push(it->Result);
		}
		size_t Count = 0;
		GetUsedRegisters(*it, Used, &Count);
		for(size_t Idx = 0; Idx < Count; ++Idx)
		{
			bool Defined = false;
			For(Kill)
			{
				if(*it == Used[Idx])
				{
					Defined = true;
					break;
				}
			}
			if(!Defined)
				Gen.Push(Used[Idx]);
		}
	}
	return liveness { SliceFromArray(Gen), SliceFromArray(Kill) };
}

array<live_interval> ComputeLiveIntervals(reg_allocator *r, slice<successor_block> Blocks, void *Memory, u32 VirtualRegCount)
{
	uint Lifetime = 0;
	u32 Used[1024];

	auto LifeSpans = array<live_interval>(Memory, VirtualRegCount);
	For(LifeSpans)
		it->Start = UINT_MAX;

	ForArray(bi, Blocks)
	{
		basic_block Block = Blocks[bi].Block;
		ForArray(ii, Block.Code)
		{
			instruction i = Block.Code[ii];

			size_t Count = 0;
			GetUsedRegisters(i, Used, &Count);
			for(size_t Idx = 0; Idx < Count; ++Idx)
			{
				live_interval& Span = LifeSpans[Used[Idx]];
				if(Span.Start != UINT_MAX)
				{
					Span.End = Lifetime;
				}
				else
				{
					Span.Start = Lifetime;
					Span.Virtual = Used[Idx];
				}
			}
			if(!HAS_FLAG(r->RegUsage[i.Op].Flags, OpFlag_DstIsUnused) && !HAS_FLAG(r->RegUsage[i.Op].Flags, OpFlag_DstIsFixed))
			{
				live_interval& Span = LifeSpans[i.Result];
				if(Span.Start != UINT_MAX)
				{
					Span.End = Lifetime;
				}
				else
				{
					Span.Start = Lifetime;
					Span.Virtual = i.Result;
				}
			}
			Lifetime++;
		}
	}

	return LifeSpans;
}

slice<fixed_register_interval> PlaceRegisterConstraints(reg_allocator *r, slice<successor_block> &Blocks)
{
	dynamic<fixed_register_interval> Intervals = {};

	uint Lifetime = 0;
	ForArray(BlockIdx, Blocks)
	{
		dynamic<instruction> FixedInstructions = {};
		auto Block = Blocks[BlockIdx].Block;
		ForArray(InstrIdx, Block.Code)
		{
			auto i = Block.Code.Data[InstrIdx];
			if(i.Op == OP_ARG)
			{
					auto Arg = i.BigRegister;
					if(Arg < r->FnArgsRegisters.Count)
					{
						uint Register = r->FnArgsRegisters[Arg];
						Intervals.Push((fixed_register_interval){Register, 0, Lifetime, i.Result});

						//auto NewInstr = Instruction(OP_COPYPHYSICAL, Register, 0, i.Result, i.Type);
						//FixedInstructions.Push(NewInstr);
					}
					else
					{
						Lifetime++;
						FixedInstructions.Push(i);
					}
			}
			else if(i.Op == OP_CALL)
			{
				uint BeforeCall = Lifetime;
				call_info *Call = (call_info *)i.BigRegister;
				const type *T = GetType(i.Type);
				ForArray(AIdx, Call->Args)
				{
					auto NewInstr = Instruction(OP_COPYTOPHYSICAL, Call->Args[AIdx], 0, r->FnArgsRegisters[AIdx], i.Type);
					FixedInstructions.Push(NewInstr);
					Lifetime++;
				}
				FixedInstructions.Push(i);
				if(T->Kind == TypeKind_Pointer)
					T = GetType(T->Pointer.Pointed);

				// @TODO: Multiple return values? Do they need handling?
				//auto NewInstr = Instruction(OP_COPYPHYSICAL, 0, 0, i.Result, T->Function.Returns[0]);
				//FixedInstructions.Push(NewInstr);
				Intervals.Push((fixed_register_interval){0, Lifetime, Lifetime, i.Result});
				for(int i = 1; i < r->PhysicalRegs; ++i)
				{
					Intervals.Push((fixed_register_interval){(uint)i, BeforeCall, Lifetime, UINT_MAX});
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

					Intervals.Push((fixed_register_interval){Usage.LeftFixed, ++Lifetime, (uint)-1, UINT_MAX});
					i.Left = Usage.LeftFixed;
				}
				if(HAS_FLAG(Usage.Flags, OpFlag_RightIsFixed))
				{
					RightIntervalIdx = Intervals.Count;

					auto NewInstr = Instruction(OP_COPYTOPHYSICAL, i.Right, 0, Usage.RightFixed, i.Type);
					FixedInstructions.Push(NewInstr);

					Intervals.Push((fixed_register_interval){Usage.RightFixed, ++Lifetime, (uint)-1, UINT_MAX});
					i.Right = Usage.RightFixed;
				}
				if(HAS_FLAG(Usage.Flags, OpFlag_DstIsFixed))
				{
					i.Result = -1;
				}
				FixedInstructions.Push(i);
				if(LeftIntervalIdx != -1)
				{
					Intervals.Data[LeftIntervalIdx].End = ++Lifetime;
				}
				if(RightIntervalIdx != -1)
				{
					Intervals.Data[RightIntervalIdx].End = ++Lifetime;
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
							Intervals.Data[LeftIntervalIdx].End = ++Lifetime;
						}
						else if(Fixed == Usage.RightFixed)
						{
							Intervals.Data[RightIntervalIdx].End = ++Lifetime;
						}
						else
						{
							Intervals.Push((fixed_register_interval){Fixed, ResultStart, ++Lifetime, UINT_MAX});
						}
					}
				}
			}
			Lifetime++;
		}
		Blocks.Data[BlockIdx].Block.Code.Free();
		Blocks.Data[BlockIdx].Block.Code = FixedInstructions;
	}
	return SliceFromArray(Intervals);
}

struct RegisterTracker
{
	live_interval VirtualInterval;
	uint FixedUntil;
};

slice<lifespan> AllocateRegistersForBasicBlocks(reg_allocator *r, array<live_interval> Intervals, slice<fixed_register_interval> Fixed)
{
	array<RegisterTracker> Active(r->PhysicalRegs);
	For(Active)
	{
		*it = {};
		it->VirtualInterval.Virtual = UINT_MAX;
		it->FixedUntil = UINT_MAX;
	}

	Intervals.Sort([](void *, const void *ap, const void *bp) -> int {
			live_interval *a = (live_interval *)ap;
			live_interval *b = (live_interval *)bp;
			if(a->Start == UINT_MAX)
				return 1;
			if(b->Start == UINT_MAX)
				return -1;
			return (int)a->Start - (int)b->Start;
			}, nullptr);

	dynamic<lifespan> Lifespans = {};

	auto IsFixedExpired = [&Active](uint RegIdx, uint At) -> bool {
		return Active[RegIdx].FixedUntil == UINT_MAX || Active[RegIdx].FixedUntil < At;
	};

	For(Intervals)
	{
		if(it->Start == UINT_MAX)
			break;

		ForN(Fixed, f)
		{
			if(f->Start <= it->Start && f->End >= it->Start)
			{
				// Fixed register is already in Active
				if(Active[f->Register].FixedUntil == f->End)
					continue;

				// No overlaps should be possible
				Assert(IsFixedExpired(f->Register, f->Start));

				Active[f->Register].FixedUntil = f->End;
				if(Active[f->Register].VirtualInterval.Virtual != UINT_MAX)
				{
					ForN(Lifespans, ls)
					{
						if(ls->Interval.Virtual == Active[f->Register].VirtualInterval.Virtual)
						{
							ls->SpilledAt = it->Start;
							break;
						}
					}
				}
				Active[f->Register].VirtualInterval.Virtual = UINT_MAX;
			}
		}

		// ExpireOldIntervals()
		ForN(Active, Interval)
		{
			if(Interval->VirtualInterval.End < it->Start)
				Interval->VirtualInterval.Virtual = UINT_MAX;
		}

		bool FoundFixed = false;
		ForN(Fixed, f)
		{
			if(f->VirtualReg == it->Virtual && !IsFixedExpired(f->Register, it->Start))
			{
				// @Note: probably don't need to spill? right?
				Active[f->Register].VirtualInterval = *it;
				Lifespans.Push(lifespan {*it, UINT_MAX, (u32)f->Register});
				FoundFixed = true;
				break;
			}
		}
		if(FoundFixed)
			continue;

		int FreeReg = -1;
		ForArray(Idx, Active)
		{
			if(Active[Idx].VirtualInterval.Virtual == UINT_MAX && IsFixedExpired(Idx, it->Start))
			{
				FreeReg = Idx;
				break;
			}
		}
		if(FreeReg != -1)
		{
			Active[FreeReg].VirtualInterval = *it;
			Lifespans.Push(lifespan {*it, UINT_MAX, (u32)FreeReg});
		}
		else
		{
			// SpillAtInterval
			int Longest = -1;
			ForArray(Idx, Active)
			{
				if (IsFixedExpired(Idx, it->Start))
				{
					Longest = Idx;
					break;
				}
			}

			Assert(Longest != -1);
			ForArray(Idx, Active)
			{
				if (Active[Longest].VirtualInterval.End < Active[Idx].VirtualInterval.End && IsFixedExpired(Idx, it->Start))
				{
					Longest = Idx;
				}
			}
			if(Active[Longest].VirtualInterval.End > it->End)
			{
				ForN(Lifespans, ls)
				{
					if(ls->Interval.Virtual == Active[Longest].VirtualInterval.Virtual)
					{
						ls->SpilledAt = it->Start;
						break;
					}
				}
				Active[Longest].VirtualInterval = *it;
				Lifespans.Push(lifespan{*it, UINT_MAX, (u32)Longest});
			}
			else
			{
				Lifespans.Push(lifespan {*it, it->Start, UINT_MAX});
			}
		}
	}

	Active.Free();
	return SliceFromArray(Lifespans);
}

string PrintLifespans(slice<lifespan> Lifespans, slice<fixed_register_interval> Fixed)
{
	string_builder b = MakeBuilder();
	For(Lifespans)
	{
		b.printf("\t%d = [%d:%d] in %d", it->Interval.Virtual, it->Interval.Start, it->Interval.End, it->InRegister);
		if(it->SpilledAt != UINT_MAX)
		{
			b.printf(" spills at %d", it->SpilledAt);
		}
		b += '\n';
	}
	b += '\n';
	For(Fixed)
	{
		b.printf("\tfixed: %s = [%d:%d]\n", phyregs[it->Register], it->Start, it->End);
	}

	return MakeString(b);
}

void AllocateRegisters(reg_allocator *r, ir *IR)
{
	stack_alloc Allocator{};
	Assert(r->PhysicalRegs > 0);

	LINFO("--- Register Allocation ---");
	ForArray(FIdx, IR->Functions)
	{
		function *Fn = &IR->Functions.Data[FIdx];
		slice<basic_block> UnorderedBlocks = SliceFromArray(Fn->Blocks);
		slice<successor_block> Blocks = SortBasicBlocks(UnorderedBlocks);

		auto Fixed = PlaceRegisterConstraints(r, Blocks);

		void *LifespanMemory = Allocator.Push(Fn->LastRegister * sizeof(live_interval));
		auto LiveIntervals = ComputeLiveIntervals(r, Blocks, LifespanMemory, Fn->LastRegister);

		Fn->Lifespans = AllocateRegistersForBasicBlocks(r, LiveIntervals, Fixed);
		string Result = PrintLifespans(Fn->Lifespans, Fixed);

		LINFO("%s IR:\n", Fn->Name->Data, (int)Result.Size, Result.Data);
		auto b = MakeBuilder();
		b += '\n';
		For(Blocks)
		{
			ForArray(Idx, it->Block.Code)
			{
				b.printf("%zu:\t", Idx);
				DissasembleInstruction(&b, it->Block.Code[Idx]);
				b += '\n';
			}
		}
		LINFO("%s", MakeString(b).Data);
		LINFO("%s:\n%.*s", Fn->Name->Data, (int)Result.Size, Result.Data);
		Allocator.Pop();
	}

	Allocator.Free();
}

reg_allocator MakeRegisterAllocator(slice<op_reg_usage> OpUsage, uint PhysicalRegisterCount, slice<uint> FnArgsRegisters)
{
	Assert(PhysicalRegisterCount > 0);
	return reg_allocator{OpUsage, FnArgsRegisters, PhysicalRegisterCount};
}

