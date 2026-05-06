#include "FlowTyping.h"
#include "Semantics.h"
#include "Type.h"
#include "backend/RegAlloc.h"


bool IsOptionalPointer(const type *T)
{
	if(T->Kind != TypeKind_Pointer)
		return false;
	return (T->Pointer.Flags & PointerFlag_Optional) != 0;
}

u32 FindNullableAllocation(dynamic<loaded_nullable> &LoadedNullables, u32 Loaded)
{
	For(LoadedNullables)
	{
		if(it->Loaded == Loaded)
		{
			return it->Location;
		}
	}
	return UINT32_MAX;
}

loaded_nullable *FindNullabe(dynamic<loaded_nullable> &LoadedNullables, u32 Register)
{
	For(LoadedNullables)
	{
		if(it->Loaded == Register)
		{
			return it;
		}
	}
	return NULL;
}

bool IsRegisterLoadedNullable(dynamic<loaded_nullable> &LoadedNullables, u32 Register)
{
	For(LoadedNullables)
	{
		if(it->Loaded == Register)
		{
			return true;
		}
	}
	return false;
}

NullComparison CmpFlipType(NullComparison Cmp)
{
	NullComparison r = Cmp;
	switch(Cmp.Cmp)
	{
		case NullCmp_EqEq:
		{
			r.Cmp = NullCmp_Neq;
		} break;
		case NullCmp_Neq:
		{
			r.Cmp = NullCmp_EqEq;
		} break;
	}

	return r;
}

NullComparison FlowTypeEvaluatePossibleNullComparison(flow_state *flow,
		dynamic<NullComparison> &NullCmps, instruction *i, NullComparisonType type,
		dynamic<loaded_nullable> &LoadedNullables, bool *IsNullCmp)
{
	*IsNullCmp = false;

	// Comparison of nullables
	if(IsRegisterLoadedNullable(LoadedNullables, i->Left) &&
			IsRegisterLoadedNullable(LoadedNullables, i->Right))
	{
		bool LeftIsNull = false;
		bool RightIsNull = false;
		ForArray(Idx, flow->Nulls)
		{
			if(flow->Nulls[Idx] == i->Left)
			{
				LeftIsNull = true;
			}
			if(flow->Nulls[Idx] == i->Right)
			{
				RightIsNull = true;
			}
		}

		// if it's not nullable (==/!=) null we don't care
		if((!LeftIsNull && !RightIsNull) || (LeftIsNull && RightIsNull))
			return {};

		*IsNullCmp = true;
		if(LeftIsNull)
		{
			u32 Allocation = FindNullableAllocation(LoadedNullables, i->Right);
			Assert(Allocation != UINT32_MAX);
			return {i->Result, Allocation, type};
		}
		else
		{
			u32 Allocation = FindNullableAllocation(LoadedNullables, i->Left);
			Assert(Allocation != UINT32_MAX);
			return {i->Result, Allocation, type};
		}
	}
	else
	{
		enum CmpState {
			None,
			IsCmp,
			IsTrue,
			IsFalse,
		};

		NullComparison FoundCmp = {};
		CmpState Left = None;
		CmpState Right = None;
		For(NullCmps)
		{
			if(it->ResultRegister == i->Left)
			{
				FoundCmp = *it;
				Left = IsCmp;
			}
			if(it->ResultRegister == i->Right)
			{
				FoundCmp = *it;
				Right = IsCmp;
			}
		}
		For(flow->Trues)
		{
			if(*it == i->Left)
				Left = IsTrue;
			if(*it == i->Right)
				Right = IsTrue;
		}
		For(flow->Falses)
		{
			if(*it == i->Left)
				Left = IsFalse;
			if(*it == i->Right)
				Right = IsFalse;
		}
		if(Left == None || Right == None)
			return {};
		if((Left != IsCmp && Right != IsCmp) || (Left == IsCmp && Right == IsCmp))
			return {};


		CmpState Other = None;
		if(Left == IsCmp)
			Other = Right;
		else
			Other = Left;

		NullComparison Result = {};
		*IsNullCmp = true;
		switch(Other)
		{
			case IsTrue:
			{
				if(type == NullCmp_EqEq)
					Result = FoundCmp;
				else
					Result = CmpFlipType(FoundCmp);
			} break;
			case IsFalse:
			{
				if(type == NullCmp_EqEq)
					Result = CmpFlipType(FoundCmp);
				else
					Result = FoundCmp;
			} break;
			default: unreachable;
		}
		Result.ResultRegister = i->Result;
		return Result;
	}

	return {};
}

void FlowTypeFindNullLocations(flow_state *Flow, successor_block *Block)
{
	For(Block->Block.Code)
	{
		if(it->Op == OP_ALLOC)
		{
			const type *T = GetType(it->Type);
			if(IsOptionalPointer(T))
			{
				Flow->NullLocations[it->Result] = true;
			}
		}
		else if(it->Op == OP_INDEX)
		{
				const type *T = GetType(it->Type);
				if(T->Kind == TypeKind_Struct)
				{
					const type *MemT = GetType(T->Struct.Members[it->Right].Type);
					if(IsOptionalPointer(MemT))
					{
						Flow->NullLocations[it->Result] = true;
					}
				}
				else if(T->Kind == TypeKind_Pointer)
				{
					const type *ElemType = GetType(T->Pointer.Pointed);
					if(IsOptionalPointer(ElemType))
					{
						Flow->NullLocations[it->Result] = true;
					}
				}
		}
	}
}

void FlowTypeFindConstants(flow_state *Flow, basic_block *Block)
{
	For(Block->Code)
	{
		switch(it->Op)
		{
			case OP_CONSTINT:
			{
				if(it->Type == Basic_bool)
				{
					if(it->BigRegister)
					{
						Flow->Trues.Push(it->Result);
					}
					else
					{
						Flow->Falses.Push(it->Result);
					}
				}
			} break;
			case OP_CONST:
			{
				if(it->Type == Basic_bool)
				{
					const_value *Val = (const_value *)it->BigRegister;
					if(Val->Int.Unsigned)
					{
						Flow->Trues.Push(it->Result);
					}
					else
					{
						Flow->Falses.Push(it->Result);
					}
				}
			} break;
			case OP_NULL:
			{
				Flow->Nulls.Push(it->Result);
			} break;
			default: break;
		}
	}
}

void FlowTypeMarkBlock(flow_state *Flow, successor_block *Block)
{
	flow_block_info *Info = &Flow->Blocks[Block->Block.ID];
	dynamic<loaded_nullable> LoadedNullables = {};
	const error_info *LastErrorInfo = NULL;
	dynamic<NullComparison> Cmps = {};
	For(Block->Block.Code)
	{
		switch(it->Op)
		{
			case OP_LOAD:
			{
				loaded_nullable *Nullable = FindNullabe(LoadedNullables, it->Right);
				if(Nullable)
				{
					Info->CannotBeNull.Add(Nullable->Location, {LastErrorInfo, true});
				}
				else if(Flow->NullLocations[it->Right])
				{
					LoadedNullables.Push({it->Result, it->Right});
				}
			} break;
			case OP_STORE:
			{
				loaded_nullable *Nullable = FindNullabe(LoadedNullables, it->Right);
				if(Nullable)
				{
					Info->MustBeNullable.Add(it->Result, {LastErrorInfo, true});
				}
			} break;
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)it->BigRegister;
				const type *T = GetType(it->Type);
				loaded_nullable *OperandN = FindNullabe(LoadedNullables, CallInfo->Operand);
				if(OperandN)
				{
					Info->CannotBeNull.Add(OperandN->Location, {CallInfo->ErrorInfo, true});
				}
				if(T->Kind == TypeKind_Pointer)
					T = GetType(T->Pointer.Pointed);
				Assert(T->Kind == TypeKind_Function);
				for(int Idx = 0; Idx < T->Function.ArgCount; ++Idx)
				{
					loaded_nullable *ArgN = FindNullabe(LoadedNullables, CallInfo->Args[Idx]);
					if(ArgN)
					{
						const type *ArgT = GetType(T->Function.Args[Idx]);
						if(ArgT == NULL)
							continue;

						if(ArgT->Kind == TypeKind_Pointer && !IsOptionalPointer(ArgT))
						{
							Info->CannotBeNull.Add(ArgN->Location, {CallInfo->ErrorInfo, true});
						}
					}
				}
			} break;
			case OP_NULL:
			{
				LoadedNullables.Push({it->Result, UINT32_MAX});
			} break;
			case OP_DEBUGINFO:
			{
				ir_debug_info *Info = (ir_debug_info *)it->Ptr;
				if(Info->type == IR_DBG_ERROR_INFO)
					LastErrorInfo = Info->err_i.ErrorInfo;
			} break;
			case OP_EQEQ:
			{
				bool Success = false;
				NullComparison Cmp = FlowTypeEvaluatePossibleNullComparison(Flow, Cmps, it, NullCmp_EqEq, LoadedNullables, &Success);
				if(Success)
				{
					Cmps.Push(Cmp);
				}
			} break;
			case OP_NEQ:
			{
				bool Success = false;
				NullComparison Cmp = FlowTypeEvaluatePossibleNullComparison(Flow, Cmps, it, NullCmp_Neq, LoadedNullables, &Success);
				if(Success)
				{
					Cmps.Push(Cmp);
				}
			} break;
			default:
			break;
		}
	}
	Info->Cmps = SliceFromArray(Cmps);
}

void FlowTypeSetBranchRegister(int FromID, flow_block_info *To, int Reg, bool SetTo, bool *Changed)
{
	if(!To->State.Contains(Reg))
	{
		To->State.Add(Reg, {});
	}
	if(!To->State[Reg].Contains(FromID))
	{
		To->State[Reg].Add(FromID, SetTo);
		*Changed = true;
	}
	else
	{
		if(To->State[Reg][FromID] != SetTo)
			*Changed = true;
		To->State[Reg][FromID] = SetTo;
	}
}


bool FlowTypeEvaluateBlock(flow_state *Flow, successor_block *Block)
{
	bool Changed = false;
	flow_block_info *Info = &Flow->Blocks[Block->Block.ID];
	ForArray(Reg, Flow->NullLocations)
	{
		if(!Flow->NullLocations[Reg])
			continue;

		enum NullState
		{
			NS_Unknown,
			NS_Null,
			NS_NotNull,
		};

		NullState NotNull = NS_Unknown;
		ForN(Block->Predecessors, Pred)
		{
			int ID = Flow->Blocks[*Pred].Block->ID;
			if(!Info->State.Contains(Reg) || !Info->State[Reg].Contains(ID))
				continue;

			if(Info->State[Reg][ID])
			{
				if(NotNull == NS_Unknown)
					NotNull = NS_NotNull;
			}
			else
			{
				NotNull = NS_Null;
			}
		}

		if(Block->Predecessors.Count == 0)
			NotNull = NS_Null;
		Assert(NotNull != NS_Unknown);

		if(NotNull == NS_Unknown)
			continue;

		if(!Info->IsNotNull.Contains(Reg))
		{
			Info->IsNotNull.Add(Reg, NotNull == NS_NotNull);
			Changed = true;
		}
		if(Info->IsNotNull[Reg] != (NotNull == NS_NotNull))
		{
			Info->IsNotNull[Reg] = NotNull == NS_NotNull;
			Changed = true;
		}
	}

	For(Block->Block.Code)
	{
		switch(it->Op)
		{
			case OP_JMP:
			{
				int TargetID = it->BigRegister;
				flow_block_info *Target = &Flow->Blocks[TargetID];
				ForN(Info->IsNotNull.Keys, rit)
				{
					int Reg = *rit;
					FlowTypeSetBranchRegister(Block->Block.ID, Target, Reg, Info->IsNotNull[Reg], &Changed);
				}
			} break;
			case OP_IF:
			{
				bool SetFromCmp = false;
				ForArray(Idx, Info->Cmps)
				{
					auto cmp = Info->Cmps[Idx];
					if(it->Result == cmp.ResultRegister)
					{
						SetFromCmp = true;
						switch(cmp.Cmp)
						{
							case NullCmp_Neq:
							{
								flow_block_info *Left = &Flow->Blocks[it->Left];
								flow_block_info *Right = &Flow->Blocks[it->Right];
								ForN(Info->IsNotNull.Keys, Reg)
								{
									bool SetLeft = Info->IsNotNull[*Reg];
									bool SetRight = SetLeft;
									if(*Reg == cmp.Allocation)
									{
										SetLeft = true;
										SetRight = false;
									}
									FlowTypeSetBranchRegister(Block->Block.ID, Left, *Reg, SetLeft, &Changed);;
									FlowTypeSetBranchRegister(Block->Block.ID, Right, *Reg, SetRight, &Changed);;
								}
							} break;
							case NullCmp_EqEq:
							{
								flow_block_info *Left = &Flow->Blocks[it->Left];
								flow_block_info *Right = &Flow->Blocks[it->Right];
								ForN(Info->IsNotNull.Keys, Reg)
								{
									bool SetLeft = Info->IsNotNull[*Reg];
									bool SetRight = SetLeft;
									if(*Reg == cmp.Allocation)
									{
										SetLeft = false;
										SetRight = true;
									}
									FlowTypeSetBranchRegister(Block->Block.ID, Left, *Reg, SetLeft, &Changed);;
									FlowTypeSetBranchRegister(Block->Block.ID, Right, *Reg, SetRight, &Changed);;
								}
							} break;
							default: unreachable;
						}
						break;
					}
				}
				if(!SetFromCmp)
				{
					flow_block_info *Left = &Flow->Blocks[it->Left];
					flow_block_info *Right = &Flow->Blocks[it->Right];
					ForN(Info->IsNotNull.Keys, rit)
					{
						int Reg = *rit;
						FlowTypeSetBranchRegister(Block->Block.ID, Left, Reg, Info->IsNotNull[Reg], &Changed);
						FlowTypeSetBranchRegister(Block->Block.ID, Right, Reg, Info->IsNotNull[Reg], &Changed);
					}
				}
			} break;

			default: break;
		}
	}

	return Changed;
}

void FlowTypeRaiseErrors(flow_state *Flow, basic_block *Block)
{
	flow_block_info *Info = &Flow->Blocks[Block->ID];
	ForN(Info->CannotBeNull.Keys, Reg)
	{
		Assert(Info->CannotBeNull[*Reg].Valid);
		if(!Info->IsNotNull.Contains(*Reg) || !Info->IsNotNull[*Reg])
		{
			RaiseError(false, *Info->CannotBeNull[*Reg].ErrorInfo,
					"Trying to dereference a nullable pointer is not allowed. "
					"You can turn it to non nullable by doing an if comparison.");
		}
	}
	ForN(Info->MustBeNullable.Keys, Reg)
	{
		Assert(Info->MustBeNullable[*Reg].Valid);
		if((Info->IsNotNull.Contains(*Reg) && Info->IsNotNull[*Reg]) || (Info->CannotBeNull.Contains(*Reg)))
		{
			RaiseError(false, *Info->MustBeNullable[*Reg].ErrorInfo,
					"Trying to store a nullable pointer into a non nullable one is not allowed.");
		}
	}
}

void FlowTypeDebugFunctionPrint(flow_state *Flow)
{
	auto b = MakeBuilder();
	b += '\n';
	For(Flow->Blocks)
	{
		b.printf("block[%d] | ", it->Block->ID);
		ForN(it->IsNotNull.Keys, Key)
		{
			b.printf("(%d=%s) ", *Key, it->IsNotNull[*Key] ? "true" : "false");
		}
		b += '\n';
	}
	LINFO("%s", MakeString(b).Data);
}

void FlowTypeFunction(function *Fn)
{
	if(Fn->Blocks.Count == 0)
		return;

	if(Fn->FakeFunction)
		return;

	scratch_arena Arena = {};
	flow_state *flow = (flow_state *)Arena.Allocate(sizeof(flow_state));
	memset(flow, 0, sizeof(flow_state));

	flow->Arena = &Arena;
	flow->Blocks = array<flow_block_info>(Fn->Blocks.Count);
	flow->NullLocations = array<bool>(Fn->LastRegister);

	slice<successor_block> Blocks = SortBasicBlocks(SliceFromArray(Fn->Blocks));
	For(Blocks)
	{
		flow->Blocks[it->Block.ID].Block = &it->Block;
	}
	For(Blocks)
		FlowTypeFindConstants(flow, &it->Block);
	For(Blocks)
		FlowTypeFindNullLocations(flow, it);
	For(Blocks)
		FlowTypeMarkBlock(flow, it);

	bool NeedsToRunAgain;
	do {
		NeedsToRunAgain = false;
		For(Blocks)
		{
			bool Changed = FlowTypeEvaluateBlock(flow, it);
			NeedsToRunAgain = NeedsToRunAgain || Changed;
		}
	} while(NeedsToRunAgain);

	if(Fn->LinkName && *Fn->LinkName == "build.test")
	{
		FlowTypeDebugFunctionPrint(flow);
	}

	For(Blocks)
		FlowTypeRaiseErrors(flow, &it->Block);

	For(flow->Blocks)
	{
		it->IsNotNull.Free();
		it->CannotBeNull.Free();
		it->MustBeNullable.Free();
		it->State.Free();
	}

	flow->Trues.Free();
	flow->Falses.Free();
	flow->Nulls.Free();
	flow->NullLocations.Free();
	flow->Blocks.Free();

}

