#include "FlowTyping.h"
#include "Semantics.h"
#include "Type.h"


bool IsOptionalPointer(const type *T)
{
	if(T->Kind != TypeKind_Pointer)
		return false;
	return (T->Pointer.Flags & PointerFlag_Optional) != 0;
}

enum NullComparisonType
{
	NullCmp_EqEq,
	NullCmp_Neq,
};

struct NullComparison
{
	u32 ResultRegister;
	u32 Allocation;
	NullComparisonType Cmp;
};

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

NullComparison FlowTypeEvaluatePossibleNullComparison(FlowState *flow,
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

void CopyNullableTrackedPointersToBlock(FlowState *flow, int FromBlockID, int ToBlockID)
{
	For(flow->NullLocations)
	{
		(*it)[ToBlockID] = (*it)[FromBlockID];
	}
}

bool CompareFlowBranches(flow_branch *A, flow_branch *B)
{
	while(A->To == B->To && A->From == B->From)
	{
		if(A->Prev == NULL || B->Prev == NULL)
		{
			return A->Prev == B->Prev;
		}
		A = A->Prev;
		B = B->Prev;
	}
	return false;
}

bool IsFlowRepeat(FlowState *flow, flow_branch *Current)
{
	For(flow->AlreadyEvaluated)
	{
		if(CompareFlowBranches(*it, Current))
		{
			return true;
		}
	}
	return false;
}

bool IsFlowLooping(flow_branch *Branch)
{
	flow_branch *At = Branch->Prev;
	while(At)
	{
		if(Branch->To == At->To || Branch->To == At->From)
		{
			return true;
		}
		At = At->Prev;
	}
	return false;
}

void FlowTypeEvaluateBlock(FlowState *flow, slice<basic_block> Blocks, flow_branch *Branch)
{
	u32 BlockID = Branch->To;
	if(IsFlowRepeat(flow, Branch) || IsFlowLooping(Branch))
	{
		return;
	}
	flow->AlreadyEvaluated.Push(Branch);
	flow->CurrentFlow = Branch;

	basic_block *Block = NULL;
	For(Blocks)
	{
		if(it->ID == BlockID)
		{
			Block = it;
			break;
		}
	}
	Assert(Block);

	dynamic<loaded_nullable> LoadedNullables = {};
	dynamic<NullComparison> Cmps = {};
	For(Block->Code)
	{
		switch(it->Op)
		{
			case OP_ALLOC:
			{
				const type *T = GetType(it->Type);
				if(IsOptionalPointer(T))
				{
					flow->NullLocations[it->Result][BlockID] = true;
				}
			} break;
			case OP_LOAD:
			{
				Assert(flow->LastErrorInfo);

				if(IsRegisterLoadedNullable(LoadedNullables, it->Right))
				{
					RaiseError(false, *flow->LastErrorInfo,
							"Trying to dereference a nullable pointer is not allowed. "
							"You can turn it to non nullable by doing an if comparison.");
				}
				else if(flow->NullLocations[it->Right][BlockID])
				{
					LoadedNullables.Push({it->Result, it->Right});
				}
			} break;
			case OP_INDEX:
			{
				const type *T = GetType(it->Type);
				if(T->Kind == TypeKind_Struct)
				{
					const type *MemT = GetType(T->Struct.Members[it->Right].Type);
					if(IsOptionalPointer(MemT))
					{
						flow->NullLocations[it->Result][BlockID] = true;
					}
				}
			} break;
			case OP_STORE:
			{
				Assert(flow->LastErrorInfo);
				if(IsRegisterLoadedNullable(LoadedNullables, it->Right))
				{
					if(!flow->NullLocations[it->Result][BlockID])
					{
						RaiseError(false, *flow->LastErrorInfo,
								"Trying to store a nullable pointer in non nullable allocation is not allowed. "
								"You can turn it to non nullable by doing an if comparison.");
					}
				}
			} break;
			case OP_CALL:
			{
				call_info *CallInfo = (call_info *)it->BigRegister;
				const type *T = GetType(it->Type);
				if(IsRegisterLoadedNullable(LoadedNullables, CallInfo->Operand))
				{
					RaiseError(false, *CallInfo->ErrorInfo, "Trying to call a nullable function pointer is not allowed. You can turn it to non nullable by doing an if comparison.");
				}
				if(T->Kind == TypeKind_Pointer)
					T = GetType(T->Pointer.Pointed);
				Assert(T->Kind == TypeKind_Function);
				for(int Idx = 0; Idx < T->Function.ArgCount; ++Idx)
				{
					if(IsRegisterLoadedNullable(LoadedNullables, CallInfo->Args[Idx]))
					{
						const type *ArgT = GetType(T->Function.Args[Idx]);
						if(ArgT == NULL)
							continue;

						if(ArgT->Kind == TypeKind_Pointer && !IsOptionalPointer(ArgT))
						{
							RaiseError(false, *CallInfo->ErrorInfo, "Trying to pass an optional pointer as an argument for non optional paramter #%d", Idx+1);
						}
					}
				}

				if(T->Function.Flags & SymbolFlag_NoReturn)
				{
					goto StopChecking;
				}
			} break;
			case OP_DEBUGINFO:
			{
				ir_debug_info *Info = (ir_debug_info *)it->Ptr;
				if(Info->type == IR_DBG_ERROR_INFO)
					flow->LastErrorInfo = Info->err_i.ErrorInfo;
			} break;
			case OP_CONSTINT:
			{
				if(it->Type == Basic_bool)
				{
					if(it->BigRegister)
					{
						flow->Trues.Push(it->Result);
					}
					else
					{
						flow->Falses.Push(it->Result);
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
						flow->Trues.Push(it->Result);
					}
					else
					{
						flow->Falses.Push(it->Result);
					}
				}
			} break;
			case OP_NULL:
			{
				flow->Nulls.Push(it->Result);
				LoadedNullables.Push({it->Result, UINT32_MAX});
			} break;
			case OP_EQEQ:
			{
				bool Success = false;
				NullComparison Cmp = FlowTypeEvaluatePossibleNullComparison(flow, Cmps, it, NullCmp_EqEq, LoadedNullables, &Success);
				if(Success)
				{
					Cmps.Push(Cmp);
				}
			} break;
			case OP_NEQ:
			{
				bool Success = false;
				NullComparison Cmp = FlowTypeEvaluatePossibleNullComparison(flow, Cmps, it, NullCmp_Neq, LoadedNullables, &Success);
				if(Success)
				{
					Cmps.Push(Cmp);
				}
			} break;
			case OP_IF:
			{
				//
				// if it->Result goto blocks[it->Left] else goto blocks[it->Right]
				//

				CopyNullableTrackedPointersToBlock(flow, BlockID, it->Left);
				CopyNullableTrackedPointersToBlock(flow, BlockID, it->Right);

				flow_branch *Left  = (flow_branch *)flow->Arena->Allocate(sizeof(flow_branch));
				flow_branch *Right = (flow_branch *)flow->Arena->Allocate(sizeof(flow_branch));
				Left->From = BlockID;
				Left->To = it->Left;
				Left->Prev = flow->CurrentFlow;

				Right->From = BlockID;
				Right->To = it->Right;
				Right->Prev = flow->CurrentFlow;

				bool AlreadyEvaled = false;

				ForArray(Idx, Cmps)
				{
					auto cmp = Cmps[Idx];
					if(it->Result == Cmps[Idx].ResultRegister)
					{
						AlreadyEvaled = true;
						switch(cmp.Cmp)
						{
							case NullCmp_Neq:
							{
								flow->NullLocations[cmp.Allocation][it->Left] = false;
								flow->NullLocations[cmp.Allocation][it->Right] = true;
								FlowTypeEvaluateBlock(flow, Blocks, Left);

								CopyNullableTrackedPointersToBlock(flow, BlockID, it->Right);
								flow->NullLocations[cmp.Allocation][it->Right] = true;
								FlowTypeEvaluateBlock(flow, Blocks, Right);

							} break;
							case NullCmp_EqEq:
							{
								flow->NullLocations[cmp.Allocation][it->Right] = false;
								flow->NullLocations[cmp.Allocation][it->Left] = true;
								FlowTypeEvaluateBlock(flow, Blocks, Right);

								CopyNullableTrackedPointersToBlock(flow, BlockID, it->Left);
								flow->NullLocations[cmp.Allocation][it->Left] = true;
								FlowTypeEvaluateBlock(flow, Blocks, Left);
							} break;
						}
						break;
					}
				}


				if(!AlreadyEvaled)
				{
					FlowTypeEvaluateBlock(flow, Blocks, Left);
					FlowTypeEvaluateBlock(flow, Blocks, Right);
				}
			} break;
			case OP_JMP:
			{
				flow_branch *Jmp  = (flow_branch *)flow->Arena->Allocate(sizeof(flow_branch));
				Jmp->From = BlockID;
				Jmp->To = (u32)it->BigRegister;
				Jmp->Prev = flow->CurrentFlow;

				CopyNullableTrackedPointersToBlock(flow, BlockID, it->BigRegister);
				FlowTypeEvaluateBlock(flow, Blocks, Jmp);
			} break;

			default: {} break;
		}
	}

StopChecking:
	LoadedNullables.Free();
	Cmps.Free();
}

void FlowTypeFunction(function *Fn)
{
	if(Fn->Blocks.Count == 0)
		return;

	if(Fn->FakeFunction)
		return;

	scratch_arena Arena = {};
	FlowState *flow = (FlowState *)Arena.Allocate(sizeof(FlowState));
	memset(flow, 0, sizeof(FlowState));

	flow->Arena = &Arena;

	flow->NullLocations = array<array<bool>>(Fn->LastRegister);
	For(flow->NullLocations)
	{
		*it = array<bool>(Fn->Blocks.Count);
	}

	flow_branch *Jmp  = (flow_branch *)flow->Arena->Allocate(sizeof(flow_branch));
	Jmp->From = UINT32_MAX;
	Jmp->To = 0;
	Jmp->Prev = NULL;
	FlowTypeEvaluateBlock(flow, SliceFromArray(Fn->Blocks), Jmp);

	flow->AlreadyEvaluated.Free();
	flow->Nulls.Free();

	For(flow->NullLocations)
	{
		it->Free();
	}
	flow->NullLocations.Free();

}

