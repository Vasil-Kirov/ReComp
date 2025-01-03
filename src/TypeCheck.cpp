#include "TypeCheck.h"
#include "Errors.h"
#include "Parser.h"
#include "Type.h"

void TypeCheck::FillType(int Idx, u32 Type, u32 BonusFlags)
{
	Types[Idx].Type = Type;
	Types[Idx].Flags = Check_Done | BonusFlags;
}

int TypeCheck::AddUnificationType(u32 T, int ExprIdx)
{
	unification_entry Entry = {};
	Entry.Binding = -1;
	Entry.ExprIndex = ExprIdx;
	Entry.UntypedType = T;
	int Result = UnificationTable.Count;
	UnificationTable.Push(Entry);

	return Result;
}

void TypeCheck::ResolveBinding(u32 T, int Binding)
{
	if(Binding == -1)
		return;

	Types[UnificationTable[Binding].ExprIndex].Flags &= ~Check_NeedsUnification;
	Types[UnificationTable[Binding].ExprIndex].Type = T;
	ResolveBinding(T, UnificationTable[Binding].Binding);
}

void TypeCheck::ResolveUnification(u32 T, int ExprIdx)
{
	if((Types[ExprIdx].Flags & Check_NeedsUnification) == 0)
		return;

	Types[ExprIdx].Flags &= ~Check_NeedsUnification;
	Types[ExprIdx].Type  = T;

	For(UnificationTable)
	{
		if(it->ExprIndex == ExprIdx)
		{
			ResolveBinding(T, it->Binding);
		}
	}
}

void TypeCheck::BindUnificationType(int UniIdx, int BindIdx)
{
	ForArray(Idx, UnificationTable)
	{
		if(Idx == UniIdx)
			continue;
		auto it = &UnificationTable.Data[Idx];
		if(it->ExprIndex == BindIdx)
		{
			Assert(it->Binding == -1);
			it->Binding = UniIdx;
		}
	}
}

b32 TypeCheck::IsDone(int Idx)
{
	if(Idx == -1)
		return false;
	return (Types[Idx].Flags & Check_Done) != 0;
}

u32 TypeCheck::FindType(int Idx, u32 *Flags)
{
	if(Idx == -1)
		return INVALID_TYPE;

	checked C =  Types[Idx];
	if(Flags)
		*Flags = C.Flags;
	if(C.Flags & Check_NeedsUnification)
	{
		return UnificationTable[C.Type].UntypedType;
	}
	return C.Type;
} 

void TypeCheck::CheckNode(int Idx)
{
	LNode Node = Nodes[Idx];
	if(Node.Node == NULL)
		Assert(false);
	
	node *Expr = Node.Node;
	switch(Expr->Type)
	{
		case AST_DECL:
		{
			CheckDecl(Idx);
		} break;
		case AST_BINARY:
		{
			CheckBinary(Idx);
		} break;
		case AST_UNARY:
		{
			u32 Flags = 0;
			u32 T = FindType(Node.Left, &Flags);
			CheckUnary(Idx, T, Flags);
		} break;
		case AST_CHARLIT:
		{
			FillType(Idx, Basic_u32, Check_NotAssignable);
		} break;
		case AST_CONSTANT:
		{
			u32 T = GetConstantType(Expr->Constant.Value);
			int UnificationIdx = AddUnificationType(T, Idx);
			FillType(Idx, UnificationIdx, Check_NeedsUnification | Check_NotAssignable);
		} break;
		case AST_NOP:
		case AST_INVALID:
		{} break;
	}
}

void TypeCheck::CheckDecl(int Idx)
{
	LNode Node = Nodes[Idx];
	if(!IsDone(Node.Left) || !IsDone(Node.Idx3))
		return;

	u32 LHSFlags, TypeFlags, ExprFlags = 0;

	u32 LHS  = FindType(Node.Left,  &LHSFlags);
	u32 DeclExpr = FindType(Node.Idx3,  &ExprFlags);
	u32 Type = INVALID_TYPE;

	if(IsDone(Node.Right))
	{
		Type = FindType(Node.Right, &TypeFlags);
		if((TypeFlags & Check_TypeLiteral) == 0)
		{
			RaiseError(false, *Expr->ErrorInfo, "Type annotation of declaration is not a valid type");
			Type = INVALID_TYPE;
		}
		else if(Type == INVALID_TYPE)
		{
			RaiseError(false, *Expr->ErrorInfo, "Type annotation cannot be void");
		}
	}
}

void TypeCheck::CheckBinary(int Idx)
{
	LNode Node = Nodes[Idx];
	if(!IsDone(Node.Left) || !IsDone(Node.Right))
		return;

	u32 LeftFlags = 0;
	u32 RightFlags = 0;
	u32 LT  = FindType(Node.Left, &LeftFlags);
	u32 RT  = FindType(Node.Right, &RightFlags);
	if(LeftFlags & Check_TypeLiteral)
		LT = Basic_type;
	if(RightFlags & Check_TypeLiteral)
		RT = Basic_type;

	node *Expr = Nodes[Idx].Node;
	bool Assignment = IsOpAssignment(Expr->Binary.Op);
	u32 Result = INVALID_TYPE;
	if(Assignment)
	{
		Result = CheckTypes(Idx, LT, RT, true, "Cannot assign %s to %s", true);
		// @TODO: Check assignable
	}
	else
	{
		Result = CheckTypes(Idx, LT, RT, false, "Cannot perform binary op between %s and %s");
	}

	// @TODO: Check pointer indexing
	if(IsUntyped(Result))
	{
		int UniIdx = AddUnificationType(Result, Idx);
		BindUnificationType(UniIdx, Node.Left);
		BindUnificationType(UniIdx, Node.Right);
		FillType(Idx, UniIdx, Check_NeedsUnification);
	}
	else
	{
		ResolveUnification(Result, LT);
		ResolveUnification(Result, RT);
	}
}

void TypeCheck::CheckUnary(int Idx, u32 TIdx, u32 OperandFlags)
{
	LNode Node = Nodes[Idx];
	if(!IsDone(Node.Left))
		return;

	node *Expr = Node.Node;
	switch(Expr->Unary.Op)
	{
		case T_MINUS:
		{
			const type *T = GetType(TIdx);
			if(!HasBasicFlag(T, BasicFlag_Integer) && !HasBasicFlag(T, BasicFlag_Float))
			{
				RaiseError(false, *Expr->ErrorInfo, "Unary `-` can only be used on integers and floats, but here it is used on %s",
						GetTypeName(T));
			}
			if(HasBasicFlag(T, BasicFlag_Unsigned))
			{
				RaiseError(false, *Expr->ErrorInfo, "Cannot use a unary `-` on an unsigned type %s", GetTypeName(T));
			}

			FillType(Idx, TIdx);
		} break;
		case T_BANG:
		{
			// @TODO
			Assert(false);
			FillType(Idx, Basic_bool);
		} break;
		case T_QMARK:
		{
			const type *Pointer = GetType(TIdx);
			if(OperandFlags & Check_TypeLiteral)
			{
				if(Pointer->Kind != TypeKind_Pointer)
				{
					RaiseError(false, *Expr->ErrorInfo, "Cannot declare optional non pointer type: %s", GetTypeName(Pointer));
					return;
				}
				Assert(Expr->Unary.Operand->Type == AST_PTRTYPE);

				u32 Optional = GetOptional(GetType(TIdx));
				FillType(Idx, Optional, Check_TypeLiteral);
				return;
			}
			if(Pointer->Kind != TypeKind_Pointer)
			{
				RaiseError(false, *Expr->ErrorInfo, "Cannot use ? operator on non pointer type %s", GetTypeName(Pointer));
				return;
			}
			if((Pointer->Pointer.Flags & PointerFlag_Optional) == 0)
			{
				// Seems safe to fill Idx and continue
				RaiseError(false, *Expr->ErrorInfo, "Pointer is not optional, remove the ? operator");
			}
			u32 NonOptional = GetNonOptional(Pointer);
			FillType(Idx, NonOptional);
		} break;
		case T_PTR:
		{
			const type *Pointer = GetType(TIdx);
			if(OperandFlags & Check_TypeLiteral)
			{
				u32 NewPointer = GetPointerTo(TIdx);
				FillType(Idx, NewPointer, Check_TypeLiteral);
				return;
			}

			if(Pointer->Kind != TypeKind_Pointer)
			{
				RaiseError(false, *Expr->ErrorInfo, "Cannot derefrence operand. It's not a pointer");
				return;
			}
			if(Pointer->Pointer.Flags & PointerFlag_Optional)
			{
				// Seems safe to continue
				RaiseError(false, *Expr->ErrorInfo, "Cannot derefrence optional pointer, check for null and then mark it non optional with the ? operator");
			}
			if(Pointer->Pointer.Pointed == INVALID_TYPE)
			{
				RaiseError(true, *Expr->ErrorInfo, "Cannot derefrence opaque pointer");
				return;
			}
			FillType(Idx, Pointer->Pointer.Pointed);
			return;
		} break;
		case T_ADDROF:
		{
			if(OperandFlags & Check_NotAssignable)
			{
				RaiseError(false, *Expr->ErrorInfo, "Cannot take address of operand");
				return;
			}
			FillType(Idx,  GetPointerTo(TIdx));
		} break;
		default: unreachable;
	}
}

void TypeCheck::DoTypeChecking()
{

}

u32 TypeCheck::CheckTypes(int Idx, u32 LT, u32 RT, b32 IsAssignment, const char *ErrorFmt, b32 RightFirst)
{
	node *Expr = Nodes[Idx].Node;
	Assert(Expr);

	const type *Left  = GetType(LT);
	const type *Right = GetType(RT);
	const type *Promotion = NULL;
	if(!IsTypeCompatible(Left, Right, &Promotion, IsAssignment) || (IsAssignment && Promotion == Right))
	{
		if(RightFirst)
		{
			RaiseError(false, *Expr->ErrorInfo, ErrorFmt,
					GetTypeName(Right), GetTypeName(Left));
		}
		else
		{
			RaiseError(false, *Expr->ErrorInfo, ErrorFmt,
					GetTypeName(Left), GetTypeName(Right));
		}
		return LT;
	}

	u32 Result = LT;
	if(Promotion)
	{
		Result = Promotion == Left ? LT : RT;
		cast_inserts Insert = {};
		Insert.To = Result;
		Insert.At = Idx;
		Casts.Push(Insert);
	}

	return Result;
}

