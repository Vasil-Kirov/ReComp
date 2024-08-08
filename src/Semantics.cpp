#include "Semantics.h"
#include "ConstVal.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Lexer.h"
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

void FillUntypedStack(checker *Checker, u32 Type)
{
	const type *TypePtr = GetType(Type);
	if(TypePtr->Kind == TypeKind_Pointer)
		Type = Basic_i64;
	while(!Checker->UntypedStack.IsEmpty())
	{
		u32 *ToGiveType = Checker->UntypedStack.Pop();
		*ToGiveType = Type;
	}
}

u32 FindStructTypeNoModuleRenaming(checker *Checker, const string *Name)
{
	for(int I = 0; I < TypeCount; ++I)
	{
		const type *Type = GetType(I);
		if(Type->Kind == TypeKind_Struct)
		{
			if(*Name == Type->Struct.Name)
			{
				return I;
			}
		}
	}
	return INVALID_TYPE;
}

u32 FindType(checker *Checker, const string *Name, const string *ModuleNameOptional=NULL)
{
	string AsModule = {};
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
			case TypeKind_Struct:
			{
				if(AsModule.Data == NULL)
				{
					string ModuleName;
					string NonPtrName = *Name;
					if(ModuleNameOptional == NULL)
					{
						ModuleName = Checker->Module->Name;
					}
					else
					{
						ModuleName = *ModuleNameOptional;
					}
					AsModule = StructToModuleName(NonPtrName, ModuleName);
				}

				if(AsModule == Type->Struct.Name)
				{
					return I;
				}
			} break;
			default:
			{
			} break;
		}
	}
	return INVALID_TYPE;
}

void FixZeroSizedArrayList(u32 ArrayTypeIdx, const type *ArrayType, node *Expression)
{
	if(ArrayType->Kind == TypeKind_Array)
	{
		if(Expression->Type != AST_ARRAYLIST)
		{
			RaiseError(*Expression->ErrorInfo, "Invalid use of array size inference",
					GetTypeName(ArrayType));
		}
		Assert(Expression->ArrayList.IsEmpty);
		Expression->ArrayList.Type = ArrayTypeIdx;
	}
	else
	{
		RaiseError(*Expression->ErrorInfo, "Cannot assign array initializer to non array type %s",
				GetTypeName(ArrayType));
	}
}

b32 FindImportedModule(checker *Checker, string &ModuleName, import *Out)
{
	auto Imports = *Checker->Imported;
	ForArray(Idx, Imports)
	{
		import Imported = Imports[Idx];
		if(Imported.Name == ModuleName
				|| Imported.As == ModuleName)
		{
			*Out = Imported;
			return true;
		}
	}
	return false;
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
		case AST_PTRTYPE:
		{
			u32 Pointed = GetTypeFromTypeNode(Checker, TypeNode->PointerType.Pointed);
			return GetPointerTo(Pointed);
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
		case AST_FN:
		{
			type *FnType = NewType(type);
			FnType->Kind = TypeKind_Function;
			if(TypeNode->Fn.ReturnType)
				FnType->Function.Return = GetTypeFromTypeNode(Checker, TypeNode->Fn.ReturnType);
			else
				FnType->Function.Return = INVALID_TYPE;
			FnType->Function.Flags = TypeNode->Fn.Flags;
			FnType->Function.ArgCount = TypeNode->Fn.Args.Count;
			FnType->Function.Args = (u32 *)VAlloc(TypeNode->Fn.Args.Count * sizeof(u32));
			ForArray(Idx, TypeNode->Fn.Args)
			{
				FnType->Function.Args[Idx] = GetTypeFromTypeNode(Checker, TypeNode->Fn.Args[Idx]->Decl.Type);
			}
			
			return AddType(FnType);
		} break;
		case AST_SELECTOR:
		{
			node *Operand = TypeNode->Selector.Operand;
			if(Operand->Type != AST_ID)
			{
				RaiseError(*Operand->ErrorInfo, "Expected module name in selector");
			}
			import Module;
			string SearchName = *Operand->ID.Name;
			if(!FindImportedModule(Checker, SearchName, &Module))
			{
				RaiseError(*Operand->ErrorInfo, "Couldn't find module `%s`", Operand->ID.Name->Data);
			}
			u32 Type = FindType(Checker, TypeNode->Selector.Member, &Module.Name);
			if(Type == INVALID_TYPE)
			{
				RaiseError(*TypeNode->ErrorInfo, "Type \"%s\" is not defined in module %s", TypeNode->Selector.Member, TypeNode->Selector.Operand->ID.Name->Data);
			}
			return Type;
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
	for(int I = 0; I < Checker->Symbols.Count; ++I)
	{
		const symbol *Local = &Checker->Symbols.Data[I];
		if(Hash == Local->Hash && *ID == *Local->Name)
			return Local;
	}
	for(int I = 0; I < Checker->Module->Globals.Count; ++I)
	{
		const symbol *Local = Checker->Module->Globals.Data[I];
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
		case AST_UNARY:
		{
			if(LHS->Unary.Op != T_PTR)
				return false;
			return true;
		} break;
		case AST_SELECTOR:
		{
			return IsLHSAssignable(Checker, LHS->Selector.Operand);
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
	}
	
	NewType->Function = Function;
	return AddType(NewType);
}

void PopScope(checker *Checker)
{
	Checker->CurrentDepth--;
	for(int I = 0; I < Checker->Symbols.Count; ++I)
	{
		if(Checker->Symbols[I].Depth > Checker->CurrentDepth)
		{
			Checker->Symbols.Count = I;
			break;
		}
	}
}

void AnalyzeFunctionBody(checker *Checker, dynamic<node *> &Body, node *FnNode, u32 FunctionTypeIdx)
{
	u32 Save = Checker->CurrentFnReturnTypeIdx;
	const type *FunctionType = GetType(FunctionTypeIdx);
	Checker->CurrentFnReturnTypeIdx = FunctionType->Function.Return;

	Checker->CurrentDepth++;
	for(int I = 0; I < FunctionType->Function.ArgCount; ++I)
	{
		node *Arg = FnNode->Fn.Args[I];
		u32 flags = SymbolFlag_Const;
		AddVariable(Checker, Arg->ErrorInfo, FunctionType->Function.Args[I], Arg->Decl.ID, NULL, flags);
	}

	b32 FoundReturn = false;
	ForArray(Idx, Body)
	{
		if(Body[Idx]->Type == AST_RETURN)
			FoundReturn = true;
		AnalyzeNode(Checker, Body[Idx]);
	}

	if(!FoundReturn && Body.Count != 0)
	{
		if(FunctionType->Function.Return != INVALID_TYPE)
		{
			RaiseError(*Body[Body.Count-1]->ErrorInfo, "Missing a return statement in function that returns a type");
		}
		Body.Push(MakeReturn(Body[Body.Count-1]->ErrorInfo, NULL));
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
			if(CallType->Kind == TypeKind_Pointer)
				CallType = GetType(CallType->Pointer.Pointed);
			if(Expr->Call.Args.Count != CallType->Function.ArgCount)
			{
				RaiseError(*Expr->ErrorInfo, "Incorrect number of arguments, needed %d got %d",
						CallType->Function.ArgCount, Expr->Call.Args.Count);
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
				if(IsUntyped(ExprType))
				{
					FillUntypedStack(Checker, CallType->Function.Args[Idx]);
				}
				else if(PromotionType)
				{
					node *Arg = Expr->Call.Args[Idx];
					Expr->Call.Args.Data[Idx] = MakeCast(Arg->ErrorInfo, Arg, NULL,
							ExprTypeIdx, CallType->Function.Args[Idx]);
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
		} break;
		case AST_ID:
		{
			Result = GetVariable(Checker, Expr->ID.Name);
			if(Result == INVALID_TYPE)
			{
				ForArray(Idx, Checker->Module->Globals)
				{
					symbol *Sym = Checker->Module->Globals[Idx];
					if(*Sym->Name == *Expr->ID.Name)
					{
						Result = Sym->Type;
						break;
					}
				}

				if(Result == INVALID_TYPE)
				{
					import m;
					string Name = *Expr->ID.Name;
					if(FindImportedModule(Checker, Name, &m))
					{
						Result = Basic_module;
					}
				}
				if(Result == INVALID_TYPE)
					RaiseError(*Expr->ErrorInfo, "Refrenced variable %s is not declared", Expr->ID.Name->Data);
			}
		} break;
		case AST_ARRAYLIST:
		{
			if(Expr->ArrayList.Expressions.Count == 0)
			{
				type *ArrayType = NewType(type);
				ArrayType->Kind = TypeKind_Array;
				ArrayType->Array.Type = Basic_UntypedInteger;
				ArrayType->Array.MemberCount = 0;

				Result = AddType(ArrayType);
				Expr->ArrayList.Type = Result;
				Expr->ArrayList.IsEmpty = true;
			}
			else
			{
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
			}

		} break;
		case AST_STRUCTLIST:
		{
			u32 TypeIdx = GetTypeFromTypeNode(Checker, Expr->StructList.StructType);
			if(TypeIdx == INVALID_TYPE)
			{
				RaiseError(*Expr->ErrorInfo, "Undefined type for struct delcaration");
			}
			const type *Type = GetType(TypeIdx);
			if(Type->Kind != TypeKind_Struct)
			{
				RaiseError(*Expr->ErrorInfo, "Non struct type %s used for struct delcaration",
						GetTypeName(Type));
			}

			uint *StructureIndexes = (uint *)VAlloc(sizeof(uint) * Expr->StructList.Expressions.Count);
			ForArray(Idx, Expr->StructList.Expressions)
			{
				const string *Name = Expr->StructList.Names[Idx];
				uint ToPut = -1;
				ForArray(j, Type->Struct.Members)
				{
					if(*Name == Type->Struct.Members[j].ID)
					{
						ToPut = j;
						break;
					}
				}
				if(ToPut == -1)
				{
					RaiseError(*Expr->ErrorInfo, "`%s` is not a member of struct %s",
							Name->Data, GetTypeName(Type));
				}
				StructureIndexes[Idx] = ToPut;
			}

			ForArray(Idx, Expr->StructList.Names)
			{
				u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->StructList.Expressions[Idx]);
				u32 MemberTypeIdx = Type->Struct.Members[StructureIndexes[Idx]].Type;

				const type *ExprType = GetType(ExprTypeIdx);
				if(ExprType->Kind == TypeKind_Array && ExprType->Array.MemberCount == 0)
				{
					const type *MemberType = GetType(MemberTypeIdx);
					FixZeroSizedArrayList(MemberTypeIdx, MemberType, Expr->StructList.Expressions[Idx]);
				}
				else
				{
					TypeCheckAndPromote(Checker, Expr->StructList.Expressions[Idx]->ErrorInfo,
							MemberTypeIdx, ExprTypeIdx, NULL, &Expr->StructList.Expressions.Data[Idx]);
				}
			}
			Expr->StructList.Type = TypeIdx;
			Expr->StructList.NameIndexes = {StructureIndexes, Expr->StructList.Expressions.Count};
			Result = TypeIdx;
		} break;
		case AST_SELECTOR:
		{
			u32 TypeIdx = AnalyzeExpression(Checker, Expr->Selector.Operand);
			if(TypeIdx == Basic_module)
			{
				Expr->Selector.Index = -1;
				if(Expr->Selector.Operand->Type != AST_ID)
				{
					RaiseError(*Expr->Selector.Operand->ErrorInfo,
							"Invalid use of module");
				}
				string ModuleName = *Expr->Selector.Operand->ID.Name;
				import m;
				if(!FindImportedModule(Checker, ModuleName, &m))
				{
					unreachable;
				}
				b32 Found = false;
				ForArray(Idx, m.Globals)
				{
					symbol *s = m.Globals[Idx];
					if(*s->Name == *Expr->Selector.Member)
					{
						Result = s->Type;
						Expr->Selector.Type = s->Type;
						Found = true;
						break;
					}
				}
				if(!Found)
				{
					RaiseError(*Expr->ErrorInfo,
							"Cannot find public symbol %s in module %s",
							Expr->Selector.Member->Data, ModuleName.Data);
				}
				Assert(Expr->Selector.Operand->Type == AST_ID);
				Expr->Selector.Operand->ID.Name = DupeType(m.Name, string);
			}
			else
			{
				Expr->Selector.Type = TypeIdx;
				const type *Type = GetType(TypeIdx);
				if(Type->Kind != TypeKind_Struct)
				{
					RaiseError(*Expr->ErrorInfo, "Cannot use `.` selector operator on a non struct type %s",
							GetTypeName(Type));
				}

				Result = INVALID_TYPE;
				ForArray(Idx, Type->Struct.Members)
				{
					if(Type->Struct.Members[Idx].ID == *Expr->Selector.Member)
					{
						Expr->Selector.Index = Idx;
						Result = Type->Struct.Members[Idx].Type;
					}
				}
				if(Result == INVALID_TYPE)
				{
					RaiseError(*Expr->ErrorInfo, "Members %s of type %s is not in the struct, invalid `.` selector",
							Expr->Selector.Member->Data, GetTypeName(Type));
				}
			}
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
			{
				Checker->UntypedStack.Push(&Expr->Constant.Type);
			}
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
		case AST_UNARY:
		{
			switch(Expr->Unary.Op)
			{
				case T_PTR:
				{
					u32 PointerIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *Pointer = GetType(PointerIdx);
					if(Pointer->Kind != TypeKind_Pointer)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot derefrence operand. It's not a pointer");
					}
					Expr->Unary.Type = Pointer->Pointer.Pointed;
					return Pointer->Pointer.Pointed;
				} break;
				case T_ADDROF:
				{
					u32 Pointed = AnalyzeExpression(Checker, Expr->Unary.Operand);
					if(!IsLHSAssignable(Checker, Expr->Unary.Operand))
					{
						RaiseError(*Expr->ErrorInfo, "Cannot take address of operand");
					}
					Expr->Unary.Type = GetPointerTo(Pointed);
					return Expr->Unary.Type;
				} break;
				default: unreachable;
			}
		} break;
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

node *OverwriteOpEqExpression(node *Expr, char Op)
{
	u32 Type = Expr->Binary.ExpressionType;
	node *NewOp = MakeBinary(Expr->ErrorInfo, Expr->Binary.Left, Expr->Binary.Right, (token_type)Op);
	node *NewEq = MakeBinary(Expr->ErrorInfo, Expr->Binary.Left, NewOp, (token_type)'=');
	NewOp->Binary.ExpressionType = Type;
	NewEq->Binary.ExpressionType = Type;
	memcpy(Expr, NewEq, sizeof(node));
	return NewOp;
}

u32 TypeCheckAndPromote(checker *Checker, const error_info *ErrorInfo, u32 Left, u32 Right, node **LeftNode, node **RightNode)
{
	u32 Result = Left;

	b32 IsAssignment = LeftNode == NULL;
	const type *LeftType  = GetType(Left);
	const type *RightType = GetType(Right);
	const type *Promotion = NULL;
	if(!IsTypeCompatible(LeftType, RightType, &Promotion, IsAssignment))
	{
TYPE_ERR:
		RaiseError(*ErrorInfo, "Incompatible types.\nLeft: %s\nRight: %s",
				GetTypeName(LeftType), GetTypeName(RightType));
	}
	if(Promotion)
	{
		promotion_description Promote = PromoteType(Promotion, LeftType, RightType, Left, Right);

		if(!IsUntyped(Promote.FromT))
		{
			if(Promote.FromT == LeftType)
			{
				if(IsAssignment)
					goto TYPE_ERR;
				*LeftNode  = MakeCast(ErrorInfo, *LeftNode, NULL, Promote.From, Promote.To);
			}
			else
			{
				*RightNode = MakeCast(ErrorInfo, *RightNode, NULL, Promote.From, Promote.To);
			}
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

		const type *LeftType = GetType(Left);
		const type *RightType = GetType(Right);

		// @TODO: Check how type checking and casting here works with +=, -=, etc... substitution
		u32 Promoted;
		if(Expr->Binary.Op != '=')
			Promoted = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Left, Right, &Expr->Binary.Left, &Expr->Binary.Right);
		else
			Promoted = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Left, Right, NULL, &Expr->Binary.Right);

		u32 Result = Promoted;
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


		node *BinaryExpression = Expr;
		switch(Expr->Binary.Op)
		{
			case T_PEQ:
			{
				BinaryExpression = OverwriteOpEqExpression(Expr, '+');
			} break;
			case T_MEQ:
			{
				BinaryExpression = OverwriteOpEqExpression(Expr, '-');
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


		if(LeftType->Kind == TypeKind_Pointer && HasBasicFlag(RightType, BasicFlag_Integer))
		{
			Assert(BinaryExpression->Type == AST_BINARY);
			if(BinaryExpression->Binary.Op != '+' && BinaryExpression->Binary.Op != '-')
			{
				RaiseError(*BinaryExpression->ErrorInfo, "Invalid binary op between pointer and integer!\n"
						"Only + and - are allowed, got `%s`", GetTokenName(BinaryExpression->Binary.Op));
			}
			node *OverwriteIndex = MakeIndex(BinaryExpression->ErrorInfo,
					BinaryExpression->Binary.Left, BinaryExpression->Binary.Right);
			OverwriteIndex->Index.OperandType = Left;
			OverwriteIndex->Index.IndexedType = LeftType->Pointer.Pointed;
			OverwriteIndex->Index.ForceNotLoad = true;

			memcpy(BinaryExpression, OverwriteIndex, sizeof(node));
		}

		Expr->Binary.ExpressionType = Promoted;
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
		for(int I = 0; I < Checker->Symbols.Count; ++I)
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

	if((Flags & SymbolFlag_Function) == 0)
	{
		const type *Ptr = GetType(Type);
		if(Ptr->Kind == TypeKind_Function)
		{
			Symbol.Type = GetPointerTo(Type);
		}
	}

	Checker->Symbols.Push(Symbol);
}

const u32 AnalyzeDeclerations(checker *Checker, node *Node)
{
	Assert(Node->Type == AST_DECL);
	const string *ID = Node->Decl.ID;
	u32 Type = GetTypeFromTypeNode(Checker, Node->Decl.Type);
	if(Node->Decl.Expression)
	{
		u32 ExprType = AnalyzeExpression(Checker, Node->Decl.Expression);
		const type *ExprTypePointer = GetType(ExprType);
		if(Type != INVALID_TYPE)
		{
			const type *TypePointer = GetType(Type);
			const type *Promotion = NULL;
			if(ExprTypePointer->Kind == TypeKind_Array && ExprTypePointer->Array.MemberCount == 0)
			{
				FixZeroSizedArrayList(Type, TypePointer, Node->Decl.Expression);
				ExprType = Type;
				ExprTypePointer = TypePointer;
			}
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
			if(ExprTypePointer->Kind == TypeKind_Array && ExprTypePointer->Array.MemberCount == 0)
			{
				RaiseError(*Node->ErrorInfo, "Cannot infer type of an empty initializer");
			}
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
	AddVariable(Checker, Node->ErrorInfo, Type, ID, Node, Flags);
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

u32 FixPotentialFunctionPointer(u32 Type)
{
	const type *Ptr = GetType(Type);
	switch(Ptr->Kind)
	{
		case TypeKind_Function:
		{
			return GetPointerTo(Type);
		} break;
		case TypeKind_Pointer:
		{
			u32 NewPointed = FixPotentialFunctionPointer(Ptr->Pointer.Pointed);
			if(NewPointed != Ptr->Pointer.Pointed)
			{
				type *Pointer = NewType(type);
				Pointer->Kind = TypeKind_Pointer;
				Pointer->Pointer.Pointed = NewPointed;
				return AddType(Pointer);
			}
			else
			{
				return Type;
			}
		} break;
		case TypeKind_Array:
		{
			u32 NewArrayType = FixPotentialFunctionPointer(Ptr->Array.Type);
			if(NewArrayType != Ptr->Array.Type)
			{
				type *Pointer = NewType(type);
				Pointer->Kind = TypeKind_Array;
				Pointer->Array.Type = NewArrayType;
				Pointer->Array.MemberCount = Ptr->Array.MemberCount;
				return AddType(Pointer);
			}
			else
			{
				return Type;
			}
		} break;
		default: return Type;
	}
}

void AnalyzeStructDeclaration(checker *Checker, node *Node)
{
	u32 OpaqueType = FindStructTypeNoModuleRenaming(Checker, Node->StructDecl.Name);
	Assert(OpaqueType != INVALID_TYPE);

	type New = {};
	New.Kind = TypeKind_Struct;
	New.Struct.Name = *Node->StructDecl.Name;

	array<struct_member> Members {Node->StructDecl.Members.Count};
	ForArray(Idx, Node->StructDecl.Members)
	{
		u32 Type = GetTypeFromTypeNode(Checker, Node->StructDecl.Members[Idx]->Decl.Type);
		Type = FixPotentialFunctionPointer(Type);


		Members.Data[Idx].ID = *Node->StructDecl.Members[Idx]->Decl.ID;
		Members.Data[Idx].Type = Type;
	}

	ForArray(Idx, Node->StructDecl.Members)
	{
		for(uint j = Idx + 1; j < Node->StructDecl.Members.Count; ++j)
		{
			if(*Node->StructDecl.Members[Idx]->Decl.ID == *Node->StructDecl.Members[j]->Decl.ID)
			{
				RaiseError(*Node->ErrorInfo, "Invalid struct declaration, members #%d and #%d have the same name `%s`",
						Idx, j, Node->StructDecl.Members[Idx]->Decl.ID->Data);
			}
		}
	}

	New.Struct.Members = SliceFromArray(Members);
	New.Struct.Flags = 0; // not supported rn
	FillOpaqueStruct(OpaqueType, New);
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
			if(Node->Return.Expression)
			{
				if(Checker->CurrentFnReturnTypeIdx == INVALID_TYPE)
				{
					RaiseError(*Node->ErrorInfo, "Trying to return a value in a void function");
				}
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
			}
			else if(Checker->CurrentFnReturnTypeIdx != INVALID_TYPE)
			{
				RaiseError(*Node->ErrorInfo, "Function expects a return value, invalid empty return!");
			}
			Node->Return.TypeIdx = Checker->CurrentFnReturnTypeIdx;
		} break;
		default:
		{
			AnalyzeExpression(Checker, Node);
		} break;
	}
}

void AnalyzeForModuleStructs(slice<node *>Nodes, import &Module)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_STRUCTDECL)
		{
			type *New = NewType(type);
			New->Kind = TypeKind_Struct;
			New->Struct.Name = *Nodes[I]->StructDecl.Name;

			AddType(New);
		}
	}
}

checker AnalyzeFunctionDecls(slice<node *>Nodes, import *ThisModule)
{
	checker Checker = {};
	Checker.Module = ThisModule;
	Checker.CurrentDepth = 0;
	Checker.CurrentFnReturnTypeIdx = INVALID_TYPE;


	dynamic<symbol *> GlobalSymbols = {};
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_FN)
		{
			node *Node = Nodes[I];
			u32 FnType = CreateFunctionType(&Checker, Node);
			Node->Fn.TypeIdx = FnType;
			symbol *Sym = NewType(symbol);
			Sym->Name = Node->Fn.Name;
			Sym->Type = FnType;
			Sym->Hash = murmur3_32(Node->Fn.Name->Data, Node->Fn.Name->Size, HASH_SEED);
			Sym->Depth = 0;
			Sym->Flags = SymbolFlag_Function;
			Sym->Flags |= SymbolFlag_Public;
			if(Node->Fn.Flags & FunctionFlag_foreign)
				Sym->Flags |= SymbolFlag_Foreign;
			GlobalSymbols.Push(Sym);
		}
	}
	Checker.Module->Globals = SliceFromArray(GlobalSymbols);
	return Checker;
}

void AnalyzeDefineStructs(checker *Checker, slice<node *>Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_STRUCTDECL)
		{
			AnalyzeStructDeclaration(Checker, Nodes[I]);
		}
	}
}

void Analyze(checker *Checker, slice<node *>Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_FN)
		{
			node *Node = Nodes[I];
			AnalyzeFunctionBody(Checker, Node->Fn.Body, Node, Node->Fn.TypeIdx);
		}
	}
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

