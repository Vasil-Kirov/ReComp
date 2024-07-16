#include "Semantics.h"
#include "ConstVal.h"
#include "Dynamic.h"
#include "Parser.h"
#include "Type.h"
#include "Memory.h"
extern const type BasicTypes[];
extern const int  BasicTypesCount;

extern const type *BasicBool;
extern const type *UntypedInteger;
extern const type *UntypedFloat;
extern const type *BasicUint;
extern const type *BasicInt;
extern const type *BasicF32;
extern u32 TypeCount;

uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed);
uint32_t murmur3_32(const char* key, size_t len, uint32_t seed)
{
	return murmur3_32((uint8_t *)key, len, seed);
}

const int HASH_SEED = 0;

void RaiseBinaryTypeError(const error_info *ErrorInfo, const type *Left, const type *Right)
{
	RaiseError(*ErrorInfo, "Incompatible types in binary expression: %s and %s",
			GetTypeName(Left), GetTypeName(Right));
}

void AddFunctionToModule(checker *Checker, node *FnNode, u32 Type)
{
	AddVariable(Checker, FnNode->ErrorInfo, Type, FnNode->Fn.Name, FnNode, SymbolFlag_Public | SymbolFlag_Function);
}

void FillUntypedStack(checker *Checker, u32 Type)
{
	while(!Checker->UntypedStack.IsEmpty())
	{
		u32 *ToGiveType = Checker->UntypedStack.Pop();
		*ToGiveType = Type;
	}
}

u32 FindType(checker *Checker, const string *Name)
{
	for(int I = 0; I < TypeCount; ++I)
	{
		const type *Type = GetType(I);
		switch(Type->Kind)
		{
			case TypeKind_Basic:
			{
				if(*Name == Type->Basic.Name)
				{
					return I;
				}
			} break;
			default:
			{
				Assert(false);
			} break;
		}
	}
	return INVALID_TYPE;
}

u32 GetTypeFromTypeNode(checker *Checker, node *TypeNode)
{
	if(TypeNode == NULL)
		return INVALID_TYPE;

	switch(TypeNode->Type)
	{
		case AST_BASICTYPE:
		{
			const string *Name = TypeNode->BasicType.ID->ID.Name;
			u32 Type = FindType(Checker, Name);
			if(Type == INVALID_TYPE)
			{
				RaiseError(*TypeNode->ErrorInfo, "Type \"%s\" is not defined", Name->Data);
			}
			return Type;
		} break;
		case AST_ARRAYTYPE:
		{
			uint Size = 0;
			if(TypeNode->ArrayType.Expression)
			{
				node *Expr = TypeNode->ArrayType.Expression;
				if(Expr->Type != AST_CONSTANT || Expr->Constant.Value.Type != const_type::Integer)
				{
					RaiseError(*Expr->ErrorInfo, "Expected constant integer for array type size");
				}
				if(Expr->Constant.Value.Int.IsSigned && Expr->Constant.Value.Int.Signed <= 0)
				{
					RaiseError(*Expr->ErrorInfo, "Expected positive integer for array type size");
				}
				if(Expr->Constant.Value.Int.IsSigned && Expr->Constant.Value.Int.Unsigned >= MB(1))
				{
					RaiseError(*Expr->ErrorInfo, "Value given for array type size is too big, cannot reliably allocate it on the stack");
				}
				Size = Expr->Constant.Value.Int.Unsigned;
			}

			type *ArrayType = NewType(type);
			ArrayType->Kind = TypeKind_Array;
			ArrayType->Array.Type = GetTypeFromTypeNode(Checker, TypeNode->ArrayType.Type);
			ArrayType->Array.MemberCount = Size;
			return AddType(ArrayType);
		} break;
		default:
		{
			RaiseError(*TypeNode->ErrorInfo, "Expected valid type!");
			return NULL;
		} break;
	}
}

const symbol *FindSymbol(checker *Checker, const string *ID)
{
	u32 Hash = murmur3_32(ID->Data, ID->Size, HASH_SEED);
	for(int I = 0; I < Checker->SymbolCount; ++I)
	{
		const symbol *Local = &Checker->Symbols[I];
		if(Hash == Local->Hash && *ID == *Local->Name)
			return Local;
	}
	return NULL;
}

b32 IsLHSAssignable(checker *Checker, node *LHS)
{
	switch(LHS->Type)
	{
		case AST_ID:
		{
			const symbol *Sym = FindSymbol(Checker, LHS->ID.Name);
			if(Sym == NULL)
			{
				RaiseError(*LHS->ErrorInfo, "Undeclared identifier %s", LHS->ID.Name->Data);
			}
			return (Sym->Flags & SymbolFlag_Const) == 0;
		} break;
		case AST_INDEX:
		{
			return IsLHSAssignable(Checker, LHS->Index.Operand);
		} break;
		default:
		{
			return false;
		};
	}
}

u32 GetVariable(checker *Checker, const string *ID)
{
	// @Note: find the last instance of the variable (for shadowing)
	const symbol *Symbol = FindSymbol(Checker, ID);
	return Symbol ? Symbol->Type : INVALID_TYPE;
}

const int MAX_ARGS = 512;
locals_for_next_scope LocalsNextScope[MAX_ARGS];
int LocalNextCount = 0;

u32 CreateFunctionType(checker *Checker, node *FnNode)
{
	type *NewType = NewType(type);
	NewType->Kind = TypeKind_Function;
	function_type Function;
	Function.Return = GetTypeFromTypeNode(Checker, FnNode->Fn.ReturnType);
	Function.ArgCount = FnNode->Fn.Args.Count;
	Function.Args = NULL;
	Function.Flags = FnNode->Fn.Flags;

	if(Function.ArgCount > 0)
		Function.Args = (u32 *)AllocatePermanent(sizeof(u32) * Function.ArgCount);

	for(int I = 0; I < Function.ArgCount; ++I)
	{
		Function.Args[I] = GetTypeFromTypeNode(Checker, FnNode->Fn.Args[I]->Decl.Type);

		locals_for_next_scope Arg;
		Arg.Type = Function.Args[I];
		Arg.ErrorInfo = FnNode->Fn.Args[I]->ErrorInfo;
		Arg.ID = FnNode->Fn.Args[I]->Decl.ID->ID.Name;
		LocalsNextScope[LocalNextCount++] = Arg;
	}
	
	NewType->Function = Function;
	return AddType(NewType);
}

void PopScope(checker *Checker)
{
	Checker->CurrentDepth--;
	for(int I = 0; I < Checker->SymbolCount; ++I)
	{
		if(Checker->Symbols[I].Depth > Checker->CurrentDepth)
		{
			Checker->SymbolCount = I;
			break;
		}
	}
}

void AnalyzeFunctionBody(checker *Checker, dynamic<node *> &Body, u32 FunctionTypeIdx)
{
	u32 Save = Checker->CurrentFnReturnTypeIdx;
	Checker->CurrentFnReturnTypeIdx = GetType(FunctionTypeIdx)->Function.Return;

	Checker->CurrentDepth++;
	for(int I = 0; I < LocalNextCount; ++I)
	{
		locals_for_next_scope Local = LocalsNextScope[I];
		u32 flags = SymbolFlag_Const;
		AddVariable(Checker, Local.ErrorInfo, Local.Type, Local.ID, NULL, flags);
	}
	LocalNextCount = 0;

	ForArray(Idx, Body)
	{
		AnalyzeNode(Checker, Body[Idx]);
	}

	PopScope(Checker);
	Checker->CurrentFnReturnTypeIdx = Save;
}

u32 AnalyzeAtom(checker *Checker, node *Expr)
{
	u32 Result = INVALID_TYPE;
	switch(Expr->Type)
	{
		case AST_CALL:
		{
			u32 CallTypeIdx = AnalyzeExpression(Checker, Expr->Call.Fn);
			const type *CallType = GetType(CallTypeIdx);
			if(!IsCallable(CallType))
			{
				RaiseError(*Expr->ErrorInfo, "Trying to call a non function type \"%s\"", GetTypeName(CallType));
			}

			ForArray(Idx, Expr->Call.Args)
			{
				u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->Call.Args[Idx]);
				const type *ExpectType = GetType(CallType->Function.Args[Idx]);
				const type *ExprType = GetType(ExprTypeIdx);
				const type *PromotionType = NULL;
				if(!IsTypeCompatible(ExpectType, ExprType, &PromotionType, true))
				{
					RaiseError(*Expr->ErrorInfo, "Argument #%d is of incompatible type %s, tried to pass: %s",
							Idx, GetTypeName(ExpectType), GetTypeName(ExprType));
				}
			}

			Expr->Call.Type = CallTypeIdx;
			Result = GetReturnType(CallType);
		} break;
		case AST_CAST:
		{
			// @TODO: auto cast
			Assert(Expr->Cast.TypeNode);
			u32 To = GetTypeFromTypeNode(Checker, Expr->Cast.TypeNode);
			u32 From = AnalyzeExpression(Checker, Expr->Cast.Expression);
			Assert(To != INVALID_TYPE && From != INVALID_TYPE);
			const type *ToType = GetType(To);
			const type *FromType = GetType(From);
			if(!IsCastValid(FromType, ToType))
			{
				RaiseError(*Expr->ErrorInfo, "Cannot cast %s to %s", GetTypeName(FromType), GetTypeName(ToType));
			}
			if(IsCastRedundant(FromType, ToType))
			{
				RaiseError(*Expr->ErrorInfo, "Redundant cast");
			}
			Expr->Cast.FromType = From;
			Expr->Cast.ToType = To;
			Result = To;

			if(IsUntyped(FromType))
			{
				FillUntypedStack(Checker, To);
				memcpy(Expr, Expr->Cast.Expression, sizeof(node));
			}
			else
			{
				FillUntypedStack(Checker, To);
			}
		} break;
		case AST_ID:
		{
			Result = GetVariable(Checker, Expr->ID.Name);
			if(Result == INVALID_TYPE)
			{
				RaiseError(*Expr->ErrorInfo, "Refrenced variable %s is not declared", Expr->ID.Name->Data);
			}
		} break;
		case AST_ARRAYLIST:
		{
			if(Expr->ArrayList.Expressions.Count == 0)
			{
				RaiseError(*Expr->ErrorInfo, "Empty initializer lists are not supported yet");
			}

			u32 ListType = INVALID_TYPE;
			node **First = NULL;
			b32 ShouldRunTyping = false;
			ForArray(Idx, Expr->ArrayList.Expressions)
			{
				u32 ExprType = AnalyzeExpression(Checker, Expr->ArrayList.Expressions[Idx]);
				const type *Type = GetType(ExprType);
				if(IsUntyped(Type))
				{
					ShouldRunTyping = true;
				}
				if(ListType == INVALID_TYPE)
				{
					// @NOTE: Kinda bad, accessing the .Data
					ListType = ExprType;
					First = &Expr->ArrayList.Expressions.Data[Idx];
				}
				else
				{
					// @NOTE: Kinda bad, accessing the .Data
					node **ListMemberNode = &Expr->ArrayList.Expressions.Data[Idx];
					ListType = TypeCheckAndPromote(Checker, Expr->ErrorInfo, ListType, ExprType, First, ListMemberNode);
				}
			}
			const type *Type = GetType(ListType);
			if(IsUntyped(Type))
			{
				if(Type->Basic.Flags & BasicFlag_Integer)
					ListType = Basic_i64;
				else if(Type->Basic.Flags & BasicFlag_Float)
					ListType = Basic_f64;
				else
					Assert(false);
			}

			if(ShouldRunTyping)
			{
				FillUntypedStack(Checker, ListType);
			}

			type *ArrayType = NewType(type);
			ArrayType->Kind = TypeKind_Array;
			ArrayType->Array.Type = ListType;
			ArrayType->Array.MemberCount = Expr->ArrayList.Expressions.Count;

			Result = AddType(ArrayType);
			Expr->ArrayList.Type = Result;

		} break;
		case AST_INDEX:
		{
			u32 OperandTypeIdx = AnalyzeExpression(Checker, Expr->Index.Operand);
			u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->Index.Expression);
			const type *ExprType = GetType(ExprTypeIdx);
			const type *OperandType = GetType(OperandTypeIdx);
			if(ExprType->Kind != TypeKind_Basic || (ExprType->Basic.Flags & BasicFlag_Integer) == 0)
			{
				RaiseError(*Expr->ErrorInfo, "Indexing expression needs to be of an integer type");
			}

			switch(OperandType->Kind)
			{
				case TypeKind_Pointer:
				{
					Result = OperandType->Pointer.Pointed;
				} break;
				case TypeKind_Array:
				{
					Result = OperandType->Array.Type;
				} break;
				default:
				{
					RaiseError(*Expr->ErrorInfo, "Cannot index type %s", GetTypeName(OperandType));
				} break;
			}

			if(IsUntyped(ExprType))
			{
				FillUntypedStack(Checker, Basic_uint);
			}

			Expr->Index.OperandType = OperandTypeIdx;
			Expr->Index.IndexedType = Result;
		} break;
		case AST_CONSTANT:
		{
			Result = GetConstantType(Expr->Constant.Value);
			Expr->Constant.Type = Result;
			if(Expr->Constant.Value.Type != const_type::String)
				Checker->UntypedStack.Push(&Expr->Constant.Type);
		} break;
		default:
		{
			Assert(false);
		} break;
	}
	return Result;
}

u32 AnalyzeUnary(checker *Checker, node *Expr)
{
	switch(Expr->Type)
	{
		default:
		{
			return AnalyzeAtom(Checker, Expr);
		} break;
	}
}

promotion_description PromoteType(const type *Promotion, const type *Left, const type *Right, u32 LeftIdx, u32 RightIdx)
{
	Assert(Promotion);
	promotion_description Result = {};
	if(Promotion == Left)
	{
		Result.From  = RightIdx;
		Result.FromT = Right;
		Result.To = LeftIdx;
		Result.ToT = Left;

	}
	else if(Promotion == Right)
	{
		Result.From = LeftIdx;
		Result.FromT = Left;
		Result.To = RightIdx;
		Result.ToT = Right;
	}
	else
		Assert(false);
	return Result;
}

void OverwriteOpEqExpression(node *Expr, char Op)
{
	u32 Type = Expr->Binary.ExpressionType;
	node *NewOp = MakeBinary(Expr->ErrorInfo, Expr->Binary.Left, Expr->Binary.Right, (token_type)Op);
	node *NewEq = MakeBinary(Expr->ErrorInfo, Expr->Binary.Left, NewOp, (token_type)'=');
	NewOp->Binary.ExpressionType = Type;
	NewEq->Binary.ExpressionType = Type;
	memcpy(Expr, NewEq, sizeof(node));
}

u32 TypeCheckAndPromote(checker *Checker, const error_info *ErrorInfo, u32 Left, u32 Right, node **LeftNode, node **RightNode)
{
	u32 Result = Left;

	const type *LeftType  = GetType(Left);
	const type *RightType = GetType(Right);
	const type *Promotion = NULL;
	if(!IsTypeCompatible(LeftType, RightType, &Promotion, false))
	{
		RaiseError(*ErrorInfo, "Incompatible types.\nLeft: %s\nRight: %s",
				GetTypeName(LeftType), GetTypeName(RightType));
	}
	if(Promotion)
	{
		promotion_description Promote = PromoteType(Promotion, LeftType, RightType, Left, Right);

		if(!IsUntyped(Promote.FromT))
		{
			if(Promote.FromT == LeftType)
				*LeftNode  = MakeCast(ErrorInfo, *LeftNode, NULL, Promote.From, Promote.To);
			else
				*RightNode = MakeCast(ErrorInfo, *RightNode, NULL, Promote.From, Promote.To);
		}
		else
		{
			FillUntypedStack(Checker, Promote.To);
		}
		Result = Promote.To;
	}

	return Result;
}

u32 AnalyzeExpression(checker *Checker, node *Expr)
{
	if(Expr->Type == AST_BINARY)
	{
		u32 Left  = AnalyzeExpression(Checker, Expr->Binary.Left);
		u32 Right = AnalyzeExpression(Checker, Expr->Binary.Right);
		Expr->Binary.ExpressionType = Left;

		// @TODO: Check how type checking and casting here works with +=, -=, etc... substitution
		u32 Promoted = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Left, Right, &Expr->Binary.Left, &Expr->Binary.Right);

		u32 Result = Left;
		switch(Expr->Binary.Op)
		{
			case T_PEQ:
			case T_MEQ:
			case T_TEQ:
			case T_DEQ:
			case T_MODEQ:
			case T_ANDEQ:
			case T_XOREQ:
			case T_OREQ:
			case T_EQ:
			{
				if(!IsLHSAssignable(Checker, Expr->Binary.Left))
					RaiseError(*Expr->ErrorInfo, "Left-hand side of assignment is not assignable");
				if(Promoted == Right && Promoted != Left)
				{
					RaiseError(*Expr->ErrorInfo, "Incompatible types in assignment expression!\n"
							"Right-hand side doesn't fit in the left-hand side");
				}
			} break;

			case T_LESS:
			case T_GREAT:
			case T_NEQ:
			case T_GEQ:
			case T_LEQ:
			case T_EQEQ:
			case T_LOR:
			case T_LAND:
			{
				Result = (u32)Basic_bool;
			} break;
			default:
			{
			} break;
		}


		switch(Expr->Binary.Op)
		{
			case T_PEQ:
			{
				OverwriteOpEqExpression(Expr, '+');
			} break;
			case T_MEQ:
			{
				OverwriteOpEqExpression(Expr, '-');
			} break;
			case T_TEQ:
			{
				OverwriteOpEqExpression(Expr, '*');
			} break;
			case T_DEQ:
			{
				OverwriteOpEqExpression(Expr, '/');
			} break;
			case T_MODEQ:
			{
				OverwriteOpEqExpression(Expr, '%');
			} break;
			case T_ANDEQ:
			{
				OverwriteOpEqExpression(Expr, '&');
			} break;
			case T_XOREQ:
			{
				OverwriteOpEqExpression(Expr, '^');
			} break;
			case T_OREQ:
			{
				OverwriteOpEqExpression(Expr, '|');
			} break;

			default: {} break;
		}


		if(Result != Basic_bool)
		{
			Expr->Binary.ExpressionType = Promoted;
			// @TODO: Is this a hack? Could checking for bool mess something up?
			Result = Promoted;
		}
		return Result;
	}
	else
	{
		return AnalyzeUnary(Checker, Expr);
	}
}

void AddVariable(checker *Checker, const error_info *ErrorInfo, u32 Type, const string *ID, node *Node, u32 Flags)
{
	u32 Hash = murmur3_32(ID->Data, ID->Size, HASH_SEED);
	if((Flags & SymbolFlag_Shadow) == 0)
	{
		for(int I = 0; I < Checker->SymbolCount; ++I)
		{
			if(Hash == Checker->Symbols[I].Hash && *ID == *Checker->Symbols[I].Name)
			{
				RaiseError(*ErrorInfo,
						"Redeclaration of variable %s.\n"
						"If this is intentional mark it as a shadow like this:\n\t#shadow %s := 0;",
						ID->Data, ID->Data);
			}
		}
	}
	symbol Symbol;
	Symbol.Hash    = Hash;
	Symbol.Depth   = Checker->CurrentDepth;
	Symbol.Name    = ID;
	Symbol.Type    = Type;
	Symbol.Flags   = Flags;

	Checker->Symbols[Checker->SymbolCount++] = Symbol;
}

const u32 AnalyzeDeclerations(checker *Checker, node *Node)
{
	Assert(Node->Type == AST_DECL);
	const node *ID = Node->Decl.ID;
	u32 Type = GetTypeFromTypeNode(Checker, Node->Decl.Type);
	if(Node->Decl.Expression)
	{
		u32 ExprType = AnalyzeExpression(Checker, Node->Decl.Expression);
		if(Type != INVALID_TYPE)
		{
			const type *TypePointer     = GetType(Type);
			const type *ExprTypePointer = GetType(ExprType);
			const type *Promotion = NULL;
			if(!IsTypeCompatible(TypePointer, ExprTypePointer, &Promotion, true))
			{
DECL_TYPE_ERROR:
				RaiseError(*Node->ErrorInfo, "Cannot assign expression of type %s to variable of type %s",
						GetTypeName(ExprTypePointer), GetTypeName(TypePointer));
			}
			if(Promotion)
			{
				if(!IsUntyped(ExprTypePointer))
				{
					if(Promotion != TypePointer && TypePointer->Kind != TypeKind_Array)
						goto DECL_TYPE_ERROR;
					if(TypePointer->Kind == TypeKind_Array)
					{
						Type = PromoteType(Promotion, TypePointer, ExprTypePointer, Type, ExprType).To;
					}
					else
					{
						Node->Decl.Expression = MakeCast(Node->ErrorInfo, Node->Decl.Expression, NULL, ExprType, Type);
					}
				}
				else
				{
					FillUntypedStack(Checker, Type);
				}
			}
		}
		else
		{
			Type = ExprType;
		}
	}
	else
	{
		if(Type == INVALID_TYPE)
		{
			RaiseError(*Node->ErrorInfo, "Expected either type or expression in variable declaration");
		}
	}
	const type *TypePtr = GetType(Type);
	if(IsUntyped(TypePtr))
	{
		// @TODO: This being signed integer could result in some problems
		// like:
		// Foo := 0xFF_FF_FF_FF;
		// Bar := $i32 Foo;
		if(TypePtr->Basic.Flags & BasicFlag_Integer)
		{
			Type = Basic_i64;
		}
		else if(TypePtr->Basic.Flags & BasicFlag_Float)
		{
			Type = Basic_f64;
		}
		else
		{
			Assert(false);
		}

		FillUntypedStack(Checker, Type);
	}
	Node->Decl.TypeIndex = Type;
	u32 Flags = (Node->Decl.IsShadow) ? SymbolFlag_Shadow : 0 | (Node->Decl.IsConst) ? SymbolFlag_Const : 0;
	AddVariable(Checker, Node->ErrorInfo, Type, ID->ID.Name, Node, Flags);
	return Type;
}

void AnalyzeInnerBody(checker *Checker, dynamic<node *> &Body)
{
	Assert(Body.IsValid());
	Checker->CurrentDepth++;
	for(int Idx = 0; Idx < Body.Count; ++Idx)
	{
		AnalyzeNode(Checker, Body[Idx]);
	}
	PopScope(Checker);
}

void AnalyzeIf(checker *Checker, node *Node)
{
	u32 ExprTypeIdx = AnalyzeExpression(Checker, Node->If.Expression);
	const type *ExprType = GetType(ExprTypeIdx);
	if(ExprType->Kind != TypeKind_Basic && ExprType->Kind != TypeKind_Pointer)
	{
		RaiseError(*Node->ErrorInfo, "If statement expression cannot be evaluated to a boolean. It has a type of %s",
				GetTypeName(ExprType));
	}
	if(ExprType->Kind != TypeKind_Basic && ((ExprType->Basic.Flags & BasicFlag_Boolean) == 0))
	{
		Node->If.Expression = MakeCast(Node->ErrorInfo, Node->If.Expression, NULL, ExprTypeIdx, Basic_bool);
	}
	AnalyzeInnerBody(Checker, Node->If.Body);
	if(Node->If.Else.IsValid())
	{
		AnalyzeInnerBody(Checker, Node->If.Else);
	}
}

void AnalyzeFor(checker *Checker, node *Node)
{
	Checker->CurrentDepth++;
	if(Node->For.Init)
		AnalyzeDeclerations(Checker, Node->For.Init);
	if(Node->For.Expr)
		AnalyzeExpression(Checker, Node->For.Expr);
	if(Node->For.Incr)
		AnalyzeExpression(Checker, Node->For.Incr);

	AnalyzeInnerBody(Checker, Node->For.Body);
	PopScope(Checker);
}

u32 AnalyzeGlobal(checker *Checker, node *Node)
{
	u32 Result = INVALID_TYPE;
	switch(Node->Type)
	{
		case AST_FN:
		{
			// @TODO: check fn return
			Result = CreateFunctionType(Checker, Node);

			Node->Fn.TypeIdx = Result;

			AddFunctionToModule(Checker, Node, Result);

			AnalyzeFunctionBody(Checker, Node->Fn.Body, Result);

		} break;
		default:
		{
			Assert(false);
		} break;
	}
	return Result;
}

void AnalyzeNode(checker *Checker, node *Node)
{
	switch(Node->Type)
	{
		case AST_DECL:
		{
			u32 TypeIdx = AnalyzeDeclerations(Checker, Node);
			const type *Type = GetType(TypeIdx);
			if(Type->Kind == TypeKind_Function)
				Node->Fn.TypeIdx = TypeIdx;
		} break;
		case AST_IF:
		{
			AnalyzeIf(Checker, Node);
		} break;
		case AST_FOR:
		{
			AnalyzeFor(Checker, Node);
		} break;
		case AST_RETURN:
		{
			u32 Result = AnalyzeExpression(Checker, Node->Return.Expression);
			const type *Type = GetType(Result);
			const type *Return = GetType(Checker->CurrentFnReturnTypeIdx);
			const type *Promotion = NULL;
			if(!IsTypeCompatible(Return, Type, &Promotion, true))
			{
RetErr:
				RaiseError(*Node->ErrorInfo, "Type of return expression does not match function return type!\n"
						"Expected: %s\n"
						"Got: %s",
						GetTypeName(Return),
						GetTypeName(Type));
			}
			if(Promotion)
			{
				promotion_description Promote = PromoteType(Promotion, Return, Type, Checker->CurrentFnReturnTypeIdx, Result);
				if(Promote.To == Result)
					goto RetErr;
				if(!IsUntyped(Type))
					Node->Return.Expression = MakeCast(Node->ErrorInfo, Node->Return.Expression, NULL, Promote.From, Promote.To);
				else
				{
					FillUntypedStack(Checker, Promote.To);
				}
			}
			Node->Return.TypeIdx = Checker->CurrentFnReturnTypeIdx;
		} break;
		default:
		{
			AnalyzeExpression(Checker, Node);
		} break;
	}
}

void Analyze(node **Nodes)
{
	// @NOTE: Too much?
	checker Checker = {};
	Checker.Symbols = (symbol *)AllocateVirtualMemory(MB(1) * sizeof(symbol));
	Checker.SymbolCount = 0;
	Checker.CurrentDepth = 0;
	Checker.CurrentFnReturnTypeIdx = INVALID_TYPE;

	int NodeCount = ArrLen(Nodes);
	for(int I = 0; I < NodeCount; ++I)
	{
		AnalyzeGlobal(&Checker, Nodes[I]);
	}

	FreeVirtualMemory(Checker.Symbols);
}

// @Note: Stolen from wikipedia implementations of murmur3
static inline uint32_t murmur_32_scramble(uint32_t k) {
	k *= 0xcc9e2d51;
	k = (k << 15) | (k >> 17);
	k *= 0x1b873593;
	return k;
}

uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed)
{
	uint32_t h = seed;
	uint32_t k;
	/* Read in groups of 4. */
	for (size_t i = len >> 2; i; i--) {
		// Here is a source of differing results across endiannesses.
		// A swap here has no effects on hash properties though.
		memcpy(&k, key, sizeof(uint32_t));
		key += sizeof(uint32_t);
		h ^= murmur_32_scramble(k);
		h = (h << 13) | (h >> 19);
		h = h * 5 + 0xe6546b64;
	}
	/* Read the rest. */
	k = 0;
	for (size_t i = len & 3; i; i--) {
		k <<= 8;
		k |= key[i - 1];
	}
	// A swap is *not* necessary here because the preceding loop already
	// places the low bytes in the low places according to whatever endianness
	// we use. Swaps only apply when the memory is copied in a chunk.
	h ^= murmur_32_scramble(k);
	/* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

