#include "Semantics.h"
#include "ConstVal.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Lexer.h"
#include "Log.h"
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
	string N = *Name;
	for(int I = 0; I < TypeCount; ++I)
	{
		const type *Type = GetTypeRaw(I);
		switch(Type->Kind)
		{
			case TypeKind_Basic:
			{
				if(N == Type->Basic.Name)
				{
					return I;
				}
			} break;
			case TypeKind_Struct:
			{
				if(AsModule.Data == NULL)
				{
					string ModuleName;
					if(ModuleNameOptional == NULL)
					{
						ModuleName = Checker->Module->Name;
					}
					else
					{
						ModuleName = *ModuleNameOptional;
					}
					AsModule = StructToModuleName(N, ModuleName);
				}

				if(AsModule == Type->Struct.Name)
				{
					return I;
				}
			} break;
			case TypeKind_Enum:
			{
				if(AsModule.Data == NULL)
				{
					string ModuleName;
					if(ModuleNameOptional == NULL)
					{
						ModuleName = Checker->Module->Name;
					}
					else
					{
						ModuleName = *ModuleNameOptional;
					}
					AsModule = StructToModuleName(N, ModuleName);
				}

				if(AsModule == Type->Enum.Name)
				{
					return I;
				}
			} break;
			case TypeKind_Generic:
			{
				if(IsScopeInOrEq(Type->Generic.Scope, Checker->CurrentScope))
				{
					if(N == Type->Generic.Name)
					{
						u32 Replacement = GetGenericReplacement();
						if(Replacement != INVALID_TYPE)
							return Replacement;
						return I;
					}
				}
			} break;
			default:
			{
			} break;
		}
	}
	return INVALID_TYPE;
}

symbol *FindSymbolFromNode(checker *Checker, node *Node, module **OutModule = NULL)
{
	switch(Node->Type)
	{
		case AST_ID:
		{
			ForArray(Idx, Checker->Module->Globals)
			{
				symbol *Sym = Checker->Module->Globals[Idx];
				if(*Sym->Name == *Node->ID.Name)
				{
					return Sym;
				}
			}
			return NULL;
		} break;
		case AST_SELECTOR:
		{
			Node->Selector.Index = -1;
			if(Node->Selector.Operand->Type != AST_ID)
			{
				RaiseError(*Node->Selector.Operand->ErrorInfo,
						"Invalid use of module");
			}
			string ModuleName = *Node->Selector.Operand->ID.Name;
			import Import;
			if(!FindImportedModule(Checker->Imported, ModuleName, &Import))
			{
				RaiseError(*Node->ErrorInfo, "Couldn't find module %s\n", ModuleName.Data);
			}
			if(OutModule)
				*OutModule = Import.M;

			module *m = Import.M;
			ForArray(Idx, m->Globals)
			{
				symbol *s = m->Globals[Idx];
				if(*s->Name == *Node->Selector.Member)
				{
					if((s->Flags & SymbolFlag_Public) == 0)
					{
						RaiseError(*Node->ErrorInfo,
								"Cannot access private member %s in module %s",
								Node->Selector.Member->Data, ModuleName.Data);
					}
					Node->Selector.Operand->ID.Name = DupeType(m->Name, string);
					return s;
				}
			}
			Node->Selector.Operand->ID.Name = DupeType(m->Name, string);
			return NULL;
		} break;
		default: unreachable;
	}
}

u32 GetTypeFromTypeNode(checker *Checker, node *TypeNode)
{
	if(TypeNode == NULL)
		return INVALID_TYPE;

	switch(TypeNode->Type)
	{
		case AST_ID:
		{
			const string *Name = TypeNode->ID.Name;
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
			return GetPointerTo(Pointed, TypeNode->PointerType.Flags);
		} break;
		case AST_ARRAYTYPE:
		{
			uint Size = 0;
			u32 MemberType = GetTypeFromTypeNode(Checker, TypeNode->ArrayType.Type);
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

				return GetArrayType(MemberType, Size);
			}
			else
			{
				return GetSliceType(MemberType);
			}
		} break;
		case AST_FN:
		{
			type *FnType = AllocType(TypeKind_Function);
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
			import Import;
			string SearchName = *Operand->ID.Name;
			if(!FindImportedModule(Checker->Imported, SearchName, &Import))
			{
				RaiseError(*Operand->ErrorInfo, "Couldn't find module `%s`", Operand->ID.Name->Data);
			}
			u32 Type = FindType(Checker, TypeNode->Selector.Member, &Import.M->Name);
			if(Type == INVALID_TYPE)
			{
				RaiseError(*TypeNode->ErrorInfo, "Type \"%s\" is not defined in module %s", TypeNode->Selector.Member->Data, TypeNode->Selector.Operand->ID.Name->Data);
			}
			return Type;
		} break;
		default:
		{
			RaiseError(*TypeNode->ErrorInfo, "Expected valid type!");
			return INVALID_TYPE;
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
			return IsLHSAssignable(Checker, LHS->Unary.Operand);
		} break;
		case AST_SELECTOR:
		{
			u32 TypeIdx = AnalyzeExpression(Checker, LHS->Selector.Operand);
			if(TypeIdx == Basic_module)
			{
				if(LHS->Selector.Operand->Type != AST_ID)
				{
					RaiseError(*LHS->Selector.Operand->ErrorInfo,
							"Invalid use of module");
				}
				string ModuleName = *LHS->Selector.Operand->ID.Name;
				import Import;
				if(!FindImportedModule(Checker->Imported, ModuleName, &Import))
				{
					unreachable;
				}
				b32 Found = false;
				ForArray(Idx, Import.M->Globals)
				{
					symbol *s = Import.M->Globals[Idx];
					if(*s->Name == *LHS->Selector.Member)
					{
						return (s->Flags & SymbolFlag_Public) &&
							((s->Flags & SymbolFlag_Const) == 0);
					}
				}
				if(!Found)
				{
					RaiseError(*LHS->ErrorInfo,
							"Cannot find public symbol %s in module %s",
							LHS->Selector.Member->Data, ModuleName.Data);
				}
				return false;
			}
			else
			{
				return IsLHSAssignable(Checker, LHS->Selector.Operand);
			}
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

scope *AllocScope(node *Node, scope *Parent)
{
	scope *S = NewType(scope);
	S->ScopeNode = Node;
	S->Parent = Parent;
	S->LastGeneric = 0;
	return S;
}

b32 IsScopeInOrEq(scope *SearchingFor, scope *S)
{
	if(!SearchingFor || !S)
		return SearchingFor == S;

	while(S)
	{
		if(S->ScopeNode == SearchingFor->ScopeNode)
		{
			return true;
		}
		S = S->Parent;
	}
	return false;
}

b32 ScopesMatch(scope *A, scope *B)
{
	if(!A || !B)
		return A == B;
	return A->ScopeNode == B->ScopeNode;
}

u32 CreateFunctionType(checker *Checker, node *FnNode)
{
	scope *Save = Checker->CurrentScope;
	scope *FnScope = AllocScope(FnNode);
	Checker->CurrentScope = FnScope;

	u32 Flags = FnNode->Fn.Flags;
	ForArray(Idx, FnNode->Fn.Args)
	{
		if(FnNode->Fn.Args[Idx]->Decl.Type == NULL)
		{
			Flags |= SymbolFlag_VarFunc;
			if(Idx + 1 != FnNode->Fn.Args.Count)
			{
				RaiseError(*FnNode->Fn.Args[Idx]->ErrorInfo, "Variadic arguments needs to be last in function type, but it is #%d", Idx);
			}
		}
	}

	type *NewType = NewType(type);
	NewType->Kind = TypeKind_Function;
	function_type Function;
	Function.ArgCount = FnNode->Fn.Args.Count - ((Flags & SymbolFlag_VarFunc) ? 1 : 0);
	Function.Args = NULL;
	Function.Flags = Flags;

	if(Function.ArgCount > 0)
		Function.Args = (u32 *)AllocatePermanent(sizeof(u32) * Function.ArgCount);

	for(int I = 0; I < Function.ArgCount; ++I)
	{
		Function.Args[I] = GetTypeFromTypeNode(Checker, FnNode->Fn.Args[I]->Decl.Type);
		const type *T = GetType(Function.Args[I]);
		if(T->Kind == TypeKind_Function)
			Function.Args[I] = GetPointerTo(Function.Args[I]);
		else if(HasBasicFlag(T, BasicFlag_TypeID))
		{
			FnNode->Fn.Flags |= SymbolFlag_Generic;
			Function.Flags |= SymbolFlag_Generic;
			MakeGeneric(FnScope, *FnNode->Fn.Args[I]->Decl.ID);
		}
	}
	Function.Return = GetTypeFromTypeNode(Checker, FnNode->Fn.ReturnType);
	
	NewType->Function = Function;
	Checker->CurrentScope = Save;
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

void AnalyzeFunctionBody(checker *Checker, dynamic<node *> &Body, node *FnNode, u32 FunctionTypeIdx, node *ScopeNode = NULL)
{
	dynamic<symbol> SaveSymbols = Checker->Symbols;
	Checker->Symbols = {};
	u32 Save = Checker->CurrentFnReturnTypeIdx;
	scope *SaveScope = Checker->CurrentScope;
	if(!ScopeNode)
		Checker->CurrentScope = AllocScope(FnNode);
	else
		Checker->CurrentScope = AllocScope(ScopeNode);

	Checker->CurrentDepth++;
	const type *FunctionType = GetType(FunctionTypeIdx);
	Checker->CurrentFnReturnTypeIdx = FunctionType->Function.Return;

	for(int I = 0; I < FunctionType->Function.ArgCount; ++I)
	{
		node *Arg = FnNode->Fn.Args[I];
		u32 flags = SymbolFlag_Const;
		const type *ArgT = GetType(FunctionType->Function.Args[I]);
		if(IsFnOrPtr(ArgT))
			flags |= SymbolFlag_Function;
		AddVariable(Checker, Arg->ErrorInfo, FunctionType->Function.Args[I], Arg->Decl.ID, Arg, flags);
		Arg->Decl.Flags = flags;
	}
	if(FunctionType->Function.Flags & SymbolFlag_VarFunc)
	{
		int I = FunctionType->Function.ArgCount;
		node *Arg = FnNode->Fn.Args[I];
		u32 flags = SymbolFlag_Const;
		u32 ArgType = FindStruct(STR_LIT("__init!Arg"));
		u32 Type = GetSliceType(ArgType);
		AddVariable(Checker, Arg->ErrorInfo, Type, Arg->Decl.ID, NULL, flags);

	}

	b32 FoundReturn = false;
	ForArray(Idx, Body)
	{
		node *Node = Body[Idx];
		if(Node->Type == AST_RETURN)
			FoundReturn = true;
		AnalyzeNode(Checker, Node);
	}

	slice<node *>BodySlice = SliceFromArray(Body);
	CheckBodyForUnreachableCode(BodySlice);

	if(!FoundReturn && Body.Count != 0)
	{
		if(FunctionType->Function.Return != INVALID_TYPE)
		{
			RaiseError(*Body[Body.Count-1]->ErrorInfo, "Missing a return statement in function that returns a type");
		}
		Body.Push(MakeReturn(Body[Body.Count-1]->ErrorInfo, NULL));
	}

	PopScope(Checker);
	Checker->CurrentScope = SaveScope;
	Checker->CurrentFnReturnTypeIdx = Save;
	Checker->Symbols = SaveSymbols;
}

u32 AnalyzeAtom(checker *Checker, node *Expr)
{
	u32 Result = INVALID_TYPE;
	switch(Expr->Type)
	{
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
					import Import;
					string Name = *Expr->ID.Name;
					if(FindImportedModule(Checker->Imported, Name, &Import))
					{
						Result = Basic_module;
					}
				}
				if(Result == INVALID_TYPE)
				{
					u32 Find = FindType(Checker, Expr->ID.Name);
					if(Find != INVALID_TYPE)
					{
						Result = Basic_type;
						Expr->ID.Type = Find;
					}
				}
				if(Result == INVALID_TYPE)
					RaiseError(*Expr->ErrorInfo, "Refrenced variable %s is not declared", Expr->ID.Name->Data);
			}
			const type *Type = GetType(Result);
			if(Type->Kind == TypeKind_Function)
				Result = GetPointerTo(Result);
		} break;
		case AST_MATCH:
		{
			u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->Match.Expression);
			const type *ExprType = GetType(ExprTypeIdx);
			if(IsUntyped(ExprType))
			{
				ExprTypeIdx = UntypedGetType(ExprType);
				FillUntypedStack(Checker, ExprTypeIdx);
				ExprType = GetType(ExprTypeIdx);
			}

			IsTypeMatchable(ExprType);
			if(Expr->Match.Cases.Count == 0)
			{
				RaiseError(*Expr->ErrorInfo, "`match` expression has no cases");
			}

			ForArray(Idx, Expr->Match.Cases)
			{
				node *Case = Expr->Match.Cases[Idx];

				u32 CaseTypeIdx = AnalyzeExpression(Checker, Case->Case.Value);
				TypeCheckAndPromote(Checker, Case->ErrorInfo, ExprTypeIdx, CaseTypeIdx, NULL, &Case->Case.Value);
			}

			Result = InvalidType;
			b32 HasResult = false;

			ForArray(Idx, Expr->Match.Cases)
			{
				node *Case = Expr->Match.Cases[Idx];
				if(!Case->Case.Body.IsValid())
				{
					RaiseError(*Case->ErrorInfo, "Missing body for case in match statement");
				}

				Checker->CurrentDepth++;
				b32 CaseReturns = false;
				for(int BodyIdx = 0; BodyIdx < Case->Case.Body.Count; ++BodyIdx)
				{
					node *Node = Case->Case.Body[BodyIdx];
					AnalyzeNode(Checker, Node);
					if(Node->Type == AST_RETURN)
					{
						if(!Node->Return.Expression)
						{
							RaiseError(*Case->ErrorInfo, "Empty return is not allowed in a match statement");
						}
						CaseReturns = true;
						if(!HasResult)
						{
							HasResult = true;
							if(Idx != 0)
							{
								RaiseError(*Case->ErrorInfo, "Previous cases do not return a value but this one does");
							}
							Result = Node->Return.TypeIdx;
							const type *T = GetType(Result);
							if(IsUntyped(T))
							{
								Result = UntypedGetType(T);
								FillUntypedStack(Checker, Result);
							}
						}
						else
						{
							TypeCheckAndPromote(Checker, Case->ErrorInfo, Result, Node->Return.TypeIdx, NULL, &Case->Case.Body.Data[BodyIdx]);
						}
					}
				}
				if(!CaseReturns && HasResult)
				{
					RaiseError(*Case->ErrorInfo, "Missing return in a match that returns a value");
				}
				CheckBodyForUnreachableCode(Case->Case.Body);
				PopScope(Checker);
			}
			Expr->Match.MatchType = ExprTypeIdx;
			Expr->Match.ReturnType = Result;
		} break;
		case AST_RESERVED:
		{
			using rs = reserved;
			switch(Expr->Reserved.ID)
			{
				case rs::Null:
				{
					Result = NULLType;
				} break;
				case rs::False:
				case rs::True:
				{
					Result = Basic_bool;
				} break;
				default: unreachable;
			}
			Expr->Reserved.Type = Result;
		} break;
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
				if(CallType->Function.Flags & SymbolFlag_VarFunc && Expr->Call.Args.Count > CallType->Function.ArgCount)
				{}
				else
				{
					RaiseError(*Expr->ErrorInfo, "Incorrect number of arguments, needed %d got %d",
							CallType->Function.ArgCount, Expr->Call.Args.Count);
				}
			}

			dynamic<u32> ArgTypes = {};
			ForArray(Idx, Expr->Call.Args)
			{
				u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->Call.Args[Idx]);
				const type *ExprType = GetType(ExprTypeIdx);
				if(CallType->Function.ArgCount <= Idx)
				{
					if(IsUntyped(ExprType))
					{
						if(ExprType->Basic.Flags & BasicFlag_Float)
						{
							ExprTypeIdx = Basic_f32;
							FillUntypedStack(Checker, Basic_f32);
						}
						else
						{
							ExprTypeIdx = Basic_int;
							FillUntypedStack(Checker, Basic_int);
						}
					}
					ArgTypes.Push(ExprTypeIdx);
					continue;
				}
				const type *ExpectType = GetType(CallType->Function.Args[Idx]);
				const type *PromotionType = NULL;
				if(!IsTypeCompatible(ExpectType, ExprType, &PromotionType, true))
				{
					RaiseError(*Expr->ErrorInfo, "Argument #%d is of incompatible type %s, tried to pass: %s",
							Idx, GetTypeName(ExpectType), GetTypeName(ExprType));
				}
				if(IsUntyped(ExprType))
				{
					if(IsGeneric(ExpectType))
					{
						FillUntypedStack(Checker, UntypedGetType(ExprType));
					}
					else
					{
						FillUntypedStack(Checker, CallType->Function.Args[Idx]);
					}
				}
				else if(PromotionType)
				{
					node *Arg = Expr->Call.Args[Idx];
					Expr->Call.Args.Data[Idx] = MakeCast(Arg->ErrorInfo, Arg, NULL,
							ExprTypeIdx, CallType->Function.Args[Idx]);
				}
				ArgTypes.Push(ExprTypeIdx);
			}
			Expr->Call.ArgTypes = SliceFromArray(ArgTypes);

			Expr->Call.Type = CallTypeIdx;

			if(CallType->Function.Flags & SymbolFlag_Generic)
			{
				Expr = AnalyzeGenericExpression(Checker, Expr);
				//Checker->Nodes->Push(Expr->Call.Fn);
				CallType = GetType(Expr->Call.Type);
			}

			Result = GetReturnType(CallType);
		} break;
		case AST_TYPEOF:
		{
			u32 ExprType = AnalyzeExpression(Checker, Expr->Size.Expression);
			Expr->TypeOf.Type = ExprType;
			Result = Basic_type;
		} break;
		case AST_SIZE:
		{
			u32 ExprType = AnalyzeExpression(Checker, Expr->Size.Expression);
			Expr->Size.Type = ExprType;
			Result = Basic_int;
		} break;
		case AST_CAST:
		{
			// @TODO: auto cast
			Assert(Expr->Cast.TypeNode);
			u32 To = GetTypeFromTypeNode(Checker, Expr->Cast.TypeNode);
			FillUntypedStack(Checker, To);

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
		case AST_TYPELIST:
		{
			u32 TypeIdx = GetTypeFromTypeNode(Checker, Expr->TypeList.TypeNode);
			Assert(TypeIdx != INVALID_TYPE);
			const type *Type = GetType(TypeIdx);
			switch(Type->Kind)
			{
				case TypeKind_Array: 
				case TypeKind_Slice:
				case TypeKind_Struct:
				break;

				default:
				{
					RaiseError(*Expr->ErrorInfo, "Cannot create a list of type %s, not a struct, array or slice", GetTypeName(Type));
				} break;
			}

			enum {
				NS_UNKNOWN,
				NS_NAMED,
				NS_NOT_NAMED,
			} NamedStatus = NS_UNKNOWN;
			ForArray(Idx, Expr->TypeList.Items)
			{
				node *Item = Expr->TypeList.Items[Idx];
				const string *Name = Item->Item.Name;
				if(Name)
				{
					if(NamedStatus == NS_NOT_NAMED)
					{
						RaiseError(*Item->ErrorInfo, "Name parameter in a list with an unnamed parameter, mixing is not allowed");
					}
					NamedStatus = NS_NAMED;
				}
				else
				{
					if(NamedStatus == NS_NAMED)
					{
						RaiseError(*Item->ErrorInfo, "Unnamed parameter in a list with a named parameter, mixing is not allowed");
					}
					NamedStatus = NS_NOT_NAMED;

				}
			}

			if(NamedStatus == NS_NAMED && (Type->Kind == TypeKind_Array || Type->Kind == TypeKind_Slice))
			{
				RaiseError(*Expr->ErrorInfo, "Still haven't implemented named array lists");
			}

			ForArray(Idx, Expr->TypeList.Items)
			{
				node *Item = Expr->TypeList.Items[Idx];
				const string *NamePtr = Item->Item.Name;
				Assert(Item->Type == AST_LISTITEM);
				u32 ItemType = AnalyzeExpression(Checker, Item->Item.Expression);
				u32 PromotedUntyped = INVALID_TYPE;
				switch(Type->Kind)
				{
					case TypeKind_Array: 
					{
						PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Type->Array.Type, ItemType, NULL, &Item->Item.Expression);
					} break;
					case TypeKind_Slice:
					{
						PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Type->Slice.Type, ItemType, NULL, &Item->Item.Expression);
					} break;
					case TypeKind_Struct:
					{
						int MemberIdx = Idx;
						if(NamePtr)
						{
							string Name = *NamePtr;
							int Found = -1;
							ForArray(MIdx, Type->Struct.Members)
							{
								struct_member Mem = Type->Struct.Members[MIdx];
								if(Mem.ID == Name)
								{
									Found = MIdx;
									break;
								}
							}
							if(Found == -1)
							{
								RaiseError(*Item->ErrorInfo, "No member named %s in struct %s",
										Name.Data, GetTypeName(Type));
							}
							MemberIdx = Found;
						}
						struct_member Mem = Type->Struct.Members[MemberIdx];
						PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Mem.Type, ItemType, NULL, &Item->Item.Expression);
					} break;
					default: unreachable;
				}

				FillUntypedStack(Checker, PromotedUntyped);
			}
			Expr->TypeList.Type = TypeIdx;
			Result = TypeIdx;
		} break;
		case AST_SELECTOR:
		{
			u32 TypeIdx = AnalyzeExpression(Checker, Expr->Selector.Operand);
			if(TypeIdx == Basic_module)
			{
				Assert(Expr->Selector.Operand->Type == AST_ID);
				symbol *s = FindSymbolFromNode(Checker, Expr);
				if(!s)
				{
					RaiseError(*Expr->ErrorInfo,
							"Cannot find public symbol %s in module %s",
							Expr->Selector.Member->Data, Expr->Selector.Member->Data);
				}
				Result = s->Type;
				Expr->Selector.Type = s->Type;
			}
			else
			{
				Expr->Selector.Type = TypeIdx;
				const type *Type = GetType(TypeIdx);
				switch(Type->Kind)
				{
					case TypeKind_Basic:
					if(HasBasicFlag(Type, BasicFlag_TypeID))
					{
						if(Expr->Selector.Operand->Type != AST_ID)
						{
							RaiseError(*Expr->ErrorInfo, "Invalid `.`! Cannot use selector on a typeid");
						}
						u32 TIdx = FindType(Checker, Expr->Selector.Operand->ID.Name);
						if(TIdx == INVALID_TYPE)
						{
							RaiseError(*Expr->ErrorInfo, "Invalid `.`! Cannot use selector on a typeid");
						}

						const type *T = GetType(TIdx);
						if(T->Kind != TypeKind_Enum)
						{
							RaiseError(*Expr->ErrorInfo, "Invalid `.`! Cannot use selector on a direct type %s", GetTypeName(T));
						}

						Result = INVALID_TYPE;
						ForArray(Idx, T->Enum.Members)
						{
							if(T->Enum.Members[Idx].Name == *Expr->Selector.Member)
							{
								Expr->Selector.Operand = NULL;
								Expr->Selector.Index = Idx;
								Expr->Selector.Type = TIdx;
								Result = TIdx;
								break;
							}
						}
						if(Result == INVALID_TYPE)
						{
							RaiseError(*Expr->ErrorInfo, "Members %s is not in enum %s, invalid `.` selector",
									Expr->Selector.Member->Data, GetTypeName(T));
						}
						break;
					}
					if(IsString(Type))
					{
						RaiseError(*Expr->ErrorInfo, "Cannot use `.` selector operator on %s", GetTypeName(Type));
					}
					// fallthrough
					case TypeKind_Slice:
					{
						if(*Expr->Selector.Member == STR_LIT("count"))
						{
						}
						else
						{
							RaiseError(*Expr->ErrorInfo, "Only .count can be accessed on this type");
						}
						Result = Basic_int;
					} break;
					case TypeKind_Enum:
					{
						Result = INVALID_TYPE;
						ForArray(Idx, Type->Enum.Members)
						{
							if(Type->Enum.Members[Idx].Name == *Expr->Selector.Member)
							{
								Expr->Selector.Index = Idx;
								Result = TypeIdx;
								break;
							}
						}
						if(Result == INVALID_TYPE)
						{
							RaiseError(*Expr->ErrorInfo, "Members %s is not enum %s, invalid `.` selector",
									Expr->Selector.Member->Data, GetTypeName(Type));
						}
					} break;
					case TypeKind_Pointer:
					{

						const type *Pointed = NULL;
						if(Type->Pointer.Pointed != INVALID_TYPE)
							Pointed = GetType(Type->Pointer.Pointed);
						if(!Pointed || Pointed->Kind != TypeKind_Struct)
						{
							RaiseError(*Expr->ErrorInfo, "Cannot use `.` selector operator on a pointer that doesn't directly point to a struct. %s",
									GetTypeName(Type));
						}
						if(Type->Pointer.Flags & PointerFlag_Optional)
						{
							RaiseError(*Expr->ErrorInfo, "Cannot derefrence optional pointer, check for null and then mark it non optional with the ? operator");
						}
						Type = Pointed;
					} // fallthrough
					case TypeKind_Struct:
					{
						Result = INVALID_TYPE;
						ForArray(Idx, Type->Struct.Members)
						{
							if(Type->Struct.Members[Idx].ID == *Expr->Selector.Member)
							{
								Expr->Selector.Index = Idx;
								Result = Type->Struct.Members[Idx].Type;
								break;
							}
						}
						if(Result == INVALID_TYPE)
						{
							RaiseError(*Expr->ErrorInfo, "Members %s of type %s is not in the struct, invalid `.` selector",
									Expr->Selector.Member->Data, GetTypeName(Type));
						}
					} break;
					default:
					{
						RaiseError(*Expr->ErrorInfo, "Cannot use `.` selector operator on %s",
								GetTypeName(Type));
					} break;
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
					if(OperandType->Pointer.Flags & PointerFlag_Optional)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot index optional pointer. Check if it's null and then use the ? operator");
					}
					if(OperandType->Pointer.Pointed == INVALID_TYPE)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot index opaque pointer");
					}
					const type *Pointed = GetType(OperandType->Pointer.Pointed);
					if(Pointed->Kind == TypeKind_Function)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot index function pointer");
					}
					Result = OperandType->Pointer.Pointed;
				} break;
				case TypeKind_Array:
				{
					Result = OperandType->Array.Type;
				} break;
				case TypeKind_Slice:
				{
					Result = OperandType->Slice.Type;
				} break;
				case TypeKind_Basic:
				{
					if(!HasBasicFlag(OperandType, BasicFlag_CString))
						RaiseError(*Expr->ErrorInfo, "Cannot index type %s", GetTypeName(OperandType));
					Result = Basic_u8;
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
		case AST_CHARLIT:
		{
			Result = Basic_u8;
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
		case AST_FN:
		{
			symbol *Sym = AnalyzeFunctionDecl(Checker, Expr);
			AnalyzeFunctionBody(Checker, Expr->Fn.Body, Expr, Sym->Type);
			return GetPointerTo(Sym->Type);
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
				case T_BANG:
				{
					u32 TypeIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *Type = GetType(TypeIdx);
					if(HasBasicFlag(Type, BasicFlag_Boolean))
					{
						RaiseError(*Expr->ErrorInfo, "Expected boolean, found %s", GetTypeName(Type));
					}
					return Basic_bool;
				} break;
				case T_QMARK:
				{
					u32 PointerIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *Pointer = GetType(PointerIdx);
					if(Pointer->Kind != TypeKind_Pointer)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot use ? operator on non pointer type %s", GetTypeName(Pointer));
					}
					if((Pointer->Pointer.Flags & PointerFlag_Optional) == 0)
					{
						RaiseError(*Expr->ErrorInfo, "Pointer is not optional, remove the ? operator");
					}
					return GetNonOptional(Pointer);
				} break;
				case T_PTR:
				{
					u32 PointerIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *Pointer = GetType(PointerIdx);
					if(Pointer->Kind != TypeKind_Pointer)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot derefrence operand. It's not a pointer");
					}
					if(Pointer->Pointer.Flags & PointerFlag_Optional)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot derefrence optional pointer, check for null and then mark it non optional with the ? operator");
					}
					if(Pointer->Pointer.Pointed == INVALID_TYPE)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot derefrence opaque pointer");

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
		const type *X = OneIsXAndTheOtherY(LeftType, RightType, TypeKind_Pointer, TypeKind_Basic);
		if(X)
		{
			token_type T = Expr->Binary.Op;
			if(T != '+' && T != '-')
			{
				RaiseError(*Expr->ErrorInfo, "Invalid operator between pointer and basic type");
			}
			if(X->Pointer.Pointed == INVALID_TYPE)
			{
				RaiseError(*Expr->ErrorInfo, "Cannot perform pointer arithmetic on an opaque pointer");
			}
		}

		if(Expr->Binary.Op != '=')
			Promoted = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Left, Right, &Expr->Binary.Left, &Expr->Binary.Right);
		else
			Promoted = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Left, Right, NULL, &Expr->Binary.Right);

		u32 Result = Promoted;
		switch(Expr->Binary.Op)
		{
			case T_SLEQ:
			case T_SREQ:
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
			case T_SLEQ:
			{
				BinaryExpression = OverwriteOpEqExpression(Expr, T_SLEFT);
			} break;
			case T_SREQ:
			{
				BinaryExpression = OverwriteOpEqExpression(Expr, T_SRIGHT);
			} break;
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
	Symbol.Node    = Node;
	Symbol.Depth   = Checker->CurrentDepth;
	Symbol.Name    = ID;
	Symbol.Type    = Type;
	Symbol.Flags   = Flags;
	Symbol.Checker = Checker;

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
		if(ExprTypePointer->Kind == TypeKind_Invalid)
		{
			RaiseError(*Node->ErrorInfo, "Expression does not give a value for the assignment");
		}

		if(Type != INVALID_TYPE)
		{
			const type *TypePointer = GetType(Type);
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
					goto DECL_TYPE_ERROR;
					//Node->Decl.Expression = MakeCast(Node->ErrorInfo, Node->Decl.Expression, NULL, ExprType, Type);
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
		// This also happens in the AST_MATCH type checking
		Type = UntypedGetType(TypePtr);
		FillUntypedStack(Checker, Type);
	}
	Node->Decl.TypeIndex = Type;
	if(IsFnOrPtr(TypePtr))
		Node->Decl.Flags |= SymbolFlag_Function;

	AddVariable(Checker, Node->ErrorInfo, Type, ID, Node, Node->Decl.Flags);
	return Type;
}

void AnalyzeInnerBody(checker *Checker, slice<node *> Body)
{
	Assert(Body.IsValid());
	for(int Idx = 0; Idx < Body.Count; ++Idx)
	{
		AnalyzeNode(Checker, Body[Idx]);
	}
	CheckBodyForUnreachableCode(Body);
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
	if(ExprType->Kind == TypeKind_Basic && ((ExprType->Basic.Flags & BasicFlag_Boolean) == 0))
	{
		Node->If.Expression = MakeCast(Node->ErrorInfo, Node->If.Expression, NULL, ExprTypeIdx, Basic_bool);
	}
	else if(ExprType->Kind == TypeKind_Pointer)
	{
		node *Null = MakeReserve(Node->ErrorInfo, reserved::Null);
		Null->Reserved.Type = NULLType;
		Node->If.Expression = MakeBinary(Node->ErrorInfo, Node->If.Expression, Null, T_NEQ);
	}
	slice<node *> IfBody = SliceFromArray(Node->If.Body); 
	AnalyzeInnerBody(Checker, IfBody);
	if(Node->If.Else.IsValid())
	{
		slice<node *> IfElse = SliceFromArray(Node->If.Else); 
		AnalyzeInnerBody(Checker, IfElse);
	}
}

void AnalyzeFor(checker *Checker, node *Node)
{
	Checker->CurrentDepth++;
	using ft = for_type;
	switch(Node->For.Kind)
	{
		case ft::C:
		{
			if(Node->For.Expr1)
				AnalyzeDeclerations(Checker, Node->For.Expr1);
			if(Node->For.Expr2)
			{
				u32 ConditionIdx = AnalyzeExpression(Checker, Node->For.Expr2);
				const type *Condition = GetType(ConditionIdx);
				if(!HasBasicFlag(Condition, BasicFlag_Boolean))
				{
					RaiseError(*Node->ErrorInfo,
							"Expected boolean type for condition expression, got %s.", GetTypeName(Condition));
				}
			}
			if(Node->For.Expr3)
				AnalyzeExpression(Checker, Node->For.Expr3);
		} break;
		case ft::It:
		{
			u32 TypeIdx = AnalyzeExpression(Checker, Node->For.Expr2);
			const type *T = GetType(TypeIdx);
			if(T->Kind != TypeKind_Array && !HasBasicFlag(T, BasicFlag_Integer))
			{
				RaiseError(*Node->For.Expr2->ErrorInfo,
						"Expression is of non iteratable type %s", GetTypeName(T));
			}
			if(IsUntyped(T))
			{
				TypeIdx = Basic_int;
				T = GetType(TypeIdx);
			}
			Assert(Node->For.Expr1->Type == AST_ID);
			u32 ItType = INVALID_TYPE;
			if(T->Kind == TypeKind_Array)
				ItType = T->Array.Type;
			else
				ItType = TypeIdx;
			AddVariable(Checker, Node->For.Expr1->ErrorInfo, ItType,
					Node->For.Expr1->ID.Name, Node->For.Expr1, 0);
			Node->For.ItType = ItType;
			Node->For.ArrayType = TypeIdx;
		} break;
		case ft::While:
		{
			AnalyzeExpression(Checker, Node->For.Expr1);
		} break;
		case ft::Infinite:
		{
		} break;
	}

	if(Node->For.Body.IsValid())
	{
		slice<node *> ForBody = SliceFromArray(Node->For.Body);
		AnalyzeInnerBody(Checker, ForBody);
	}
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
			if(Ptr->Pointer.Pointed == INVALID_TYPE)
				return Type;
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

void AnalyzeEnum(checker *Checker, node *Node)
{
	if(Node->Enum.Items.Count == 0)
		RaiseError(*Node->ErrorInfo, "Empty enums are not allowed");

	u32 AlreadyDefined = FindType(Checker, Node->Enum.Name);
	if(AlreadyDefined != INVALID_TYPE)
	{
		RaiseError(*Node->ErrorInfo, "Enum %s is a redefinition, original type is %s",
				Node->Enum.Name->Data, GetTypeName(AlreadyDefined));
	}


	u32 Type = INVALID_TYPE;
	if(Node->Enum.Type)
	{
		Type = GetTypeFromTypeNode(Checker, Node->Enum.Type);
		Assert(Type != INVALID_TYPE);
		const type *T = GetType(Type);
		if(!HasBasicFlag(T, BasicFlag_Integer))
		{
			RaiseError(*Node->ErrorInfo, "Enum type must be integral, cannot use %s",
					GetTypeName(T));
		}
		
	}
	else
	{
		Type = Basic_int;
	}
	

	dynamic<enum_member> Members = {};
	enum {
		WITH_EXPR,
		NO_EXPR,
	} EnumType = Node->Enum.Items[0]->Item.Expression ? WITH_EXPR : NO_EXPR;
	ForArray(Idx, Node->Enum.Items)
	{
		auto Item = Node->Enum.Items[Idx];
		if(EnumType == WITH_EXPR)
		{
			if(!Item->Item.Expression)
			{
				RaiseError(*Item->ErrorInfo, "Missing value. Other members in the enum use values and mixing is not allowed");
			}
		}
		else
		{
			if(Item->Item.Expression)
			{
				RaiseError(*Item->ErrorInfo, "Using expression in an enum in which other members don't use expressions is not allowed");

			}
		}
	}

	ForArray(Idx, Node->Enum.Items)
	{
		enum_member Member = {};
		auto Item = Node->Enum.Items[Idx]->Item;
		Member.Name = *Item.Name;
		if(Item.Expression)
		{
			switch(Item.Expression->Type)
			{
				case AST_CONSTANT:
				{
					Member.Value = Item.Expression->Constant.Value;
					if(Member.Value.Type != const_type::Integer)
					{
						RaiseError(*Item.Expression->ErrorInfo, "Enum member value must be an integer");
					}
				} break;
				case AST_CHARLIT:
				{
					const_value Value = {};
					Value.Type = const_type::Integer;
					Value.Int.IsSigned = false;
					Value.Int.Unsigned = Node->CharLiteral.C;
					Member.Value = Value;
				} break;
				default:
				{
					RaiseError(*Item.Expression->ErrorInfo, "Enum member value must be a constant integer");
				} break;
			}
		}
		else
		{
			const_value Value = {};
			Value.Type = const_type::Integer;
			Value.Int.IsSigned = false;
			Value.Int.Unsigned = Idx;
			Member.Value = Value;
		}
		Members.Push(Member);
	}

	MakeEnumType(*Node->Enum.Name, SliceFromArray(Members), Type);
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

b32 IsNodeEndScope(node *Node)
{
	return Node->Type == AST_SCOPE && Node->ScopeDelimiter.IsUp == false;
}

void CheckBodyForUnreachableCode(slice<node *> Body)
{
	ForArray(Idx, Body)
	{
		node *Node = Body[Idx];
		if(Node->Type == AST_RETURN)
		{
			b32 DeadCode = false;
			if(Idx + 1 != Body.Count)
			{
				if(!IsNodeEndScope(Body[Idx+1]) || Idx + 2 != Body.Count)
					DeadCode = true;
			}
			if(DeadCode)
				RaiseError(*Body[Idx + 1]->ErrorInfo, "Unreachable code after return statement");
		}
		if(Node->Type == AST_BREAK && Idx + 1 != Body.Count)
		{
			b32 DeadCode = false;
			if(Idx + 1 != Body.Count)
			{
				if(!IsNodeEndScope(Body[Idx+1]) || Idx + 2 != Body.Count)
					DeadCode = true;
			}
			if(DeadCode)
				RaiseError(*Body[Idx + 1]->ErrorInfo, "Unreachable code after break statement");
		}
	}
}

void AnalyzeNode(checker *Checker, node *Node)
{
	switch(Node->Type)
	{
		case AST_NOP: {} break;
		case AST_DECL:
		{
			AnalyzeDeclerations(Checker, Node);
		} break;
		case AST_DEFER:
		{
			AnalyzeInnerBody(Checker, Node->Defer.Body);
		} break;
		case AST_IF:
		{
			Checker->CurrentScope = AllocScope(Node, Checker->CurrentScope);
			AnalyzeIf(Checker, Node);
			Checker->CurrentScope = Checker->CurrentScope->Parent;
		} break;
		case AST_BREAK:
		{
			b32 FoundBreakableScope = false;
			scope *Current = Checker->CurrentScope;

			while(Current)
			{
				if(Current->ScopeNode->Type == AST_FOR)
				{
					FoundBreakableScope = true;
					break;
				}
				Current = Current->Parent;
			}
			if(!FoundBreakableScope)
			{
				RaiseError(*Node->ErrorInfo, "Invalid context for break, not a loop or a match statement");
			}
		} break;
		case AST_FOR:
		{
			Checker->CurrentScope = AllocScope(Node, Checker->CurrentScope);
			AnalyzeFor(Checker, Node);
			Checker->CurrentScope = Checker->CurrentScope->Parent;
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
		case AST_SCOPE:
		{
			if(Node->ScopeDelimiter.IsUp)
				Checker->CurrentDepth++;
			else
				PopScope(Checker);
		} break;
		default:
		{
			AnalyzeExpression(Checker, Node);
		} break;
	}
}

void AnalyzeForModuleStructs(slice<node *>Nodes, module *Module)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_STRUCTDECL)
		{
			string Name = *Nodes[I]->StructDecl.Name;
			type *New = NewType(type);
			New->Kind = TypeKind_Struct;
			New->Struct.Name = Name;

			uint Count = GetTypeCount();
			string SymbolName = StructToModuleName(Name, Module->Name);
			for(int i = 0; i < Count; ++i)
			{
				const type *T = GetType(i);
				if(T->Kind == TypeKind_Struct)
				{
					if(T->Struct.Name == SymbolName)
					{
						RaiseError(*Nodes[I]->ErrorInfo, "Redifinition of struct %s", Name.Data);
					}
				}
				else if(T->Kind == TypeKind_Enum)
				{
					if(T->Enum.Name == SymbolName)
					{
						RaiseError(*Nodes[I]->ErrorInfo, "Redifinition of enum %s as struct", Name.Data);
					}
				}
			}

			AddType(New);
		}
	}
}

symbol *AnalyzeFunctionDecl(checker *Checker, node *Node)
{
	u32 FnType = CreateFunctionType(Checker, Node);
	Node->Fn.TypeIdx = FnType;
	symbol *Sym = NewType(symbol);
	Sym->Checker = Checker;
	Sym->Name = Node->Fn.Name;
	Sym->Type = FnType;
	Sym->Hash = murmur3_32(Node->Fn.Name->Data, Node->Fn.Name->Size, HASH_SEED);
	Sym->Depth = 0;
	Sym->Flags = SymbolFlag_Function | SymbolFlag_Const | Node->Fn.Flags;
	Sym->Node = Node;
	return Sym;
}

void AnalyzeFunctionDecls(checker *Checker, dynamic<node *> *NodesPtr, module *ThisModule)
{
	Checker->Nodes = NodesPtr;
	Checker->Module = ThisModule;
	Checker->CurrentDepth = 0;
	Checker->CurrentFnReturnTypeIdx = INVALID_TYPE;

	slice<node *> Nodes = SliceFromArray(*NodesPtr);

	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_FN)
		{
			node *Node = Nodes[I];
			symbol *Sym = AnalyzeFunctionDecl(Checker, Node);
			Checker->Module->Globals.Push(Sym);
		}
	}

	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_DECL)
		{
			node *Node = Nodes[I];
			u32 Type = AnalyzeDeclerations(Checker, Node);
			symbol *Sym = NewType(symbol);
			Sym->Checker = Checker;
			Sym->Name = Node->Decl.ID;
			Sym->Type = Type;
			Sym->Hash = murmur3_32(Node->Decl.ID->Data, Node->Decl.ID->Size, 
					HASH_SEED);
			Sym->Depth = 0;
			Sym->Flags = Node->Decl.Flags;
			Checker->Module->Globals.Push(Sym);
		}
	}
}

void AnalyzeDefineStructs(checker *Checker, slice<node *>Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_ENUM)
		{
			AnalyzeEnum(Checker, Nodes[I]);
		}
	}
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_STRUCTDECL)
		{
			AnalyzeStructDeclaration(Checker, Nodes[I]);
		}
	}
}

string MakeNonGenericName(string GenericName)
{
	b32 IsNameGeneric = false;
	for(int i = 0; i < GenericName.Size; ++i)
	{
		if(GenericName.Data[i] == '@')
		{
			IsNameGeneric = true;
			break;
		}
	}
	if(!IsNameGeneric)
		return GenericName;

	string_builder Builder = MakeBuilder();
	for(int i = 0; i < GenericName.Size; ++i)
	{
		if(GenericName.Data[i] == '@')
			break;

		Builder += GenericName.Data[i];
	}

	return MakeString(Builder);
}

string *MakeGenericName(string BaseName, u32 FnTypeNonGeneric)
{
	const type *T = GetType(FnTypeNonGeneric);
	string_builder Builder = MakeBuilder();
	Builder += BaseName;
	Builder += '@';
	for(int i = 0; i < T->Function.ArgCount; ++i)
	{
		Builder += GetTypeNameAsString(T->Function.Args[i]);
		Builder += '_';
	}
	Builder += '@';
	if(T->Function.Return == INVALID_TYPE)
		Builder += "void";
	else
		Builder += GetTypeNameAsString(T->Function.Return);
	string Result = MakeString(Builder);
	return DupeType(Result, string);
}

node *AnalyzeGenericFunction(checker *Checker, node *FnNode, u32 ResolvedType, node *OriginalNode)
{
	node *Result = FnNode;
	Result->Fn.Flags = Result->Fn.Flags & ~SymbolFlag_Generic;

	u32 NewFnType = ToNonGeneric(FnNode->Fn.TypeIdx, ResolvedType);
	Result->Fn.Name = MakeGenericName(Result->Fn.Name ? *Result->Fn.Name : STR_LIT(""), NewFnType);
	Result->Fn.TypeIdx = NewFnType;

	u32 Save = GetGenericReplacement();

	SetGenericReplacement(ResolvedType);
	AnalyzeFunctionBody(Checker, Result->Fn.Body, Result, FnNode->Fn.TypeIdx, OriginalNode);
	SetGenericReplacement(Save);

	return Result;
}

node *AnalyzeGenericExpression(checker *Checker, node *Generic)
{
	node *Expr = Generic;
	switch(Expr->Type)
	{
		case AST_CALL:
		{
			module *MaybeMod = NULL;
			symbol *FnSym = FindSymbolFromNode(Checker, Expr->Call.Fn, &MaybeMod);
			if(!FnSym)
			{
				RaiseError(*Expr->ErrorInfo, "Couldn't resolve generic function");
			}

			const type *T = GetType(FnSym->Type);
			u32 ResolvedType = INVALID_TYPE;
			dynamic<u32> GenericTypes = {};
			for(int i = 0; i < T->Function.ArgCount; ++i)
			{
				const type *ArgT = GetType(T->Function.Args[i]);
				if(HasBasicFlag(ArgT, BasicFlag_TypeID))
				{
					if(ResolvedType != INVALID_TYPE)
					{
						RaiseError(*Expr->ErrorInfo, "Cannot pass more than 1 generic argument");
#if 0
						const type *L = GetType(ResolvedType);
						const type *R = GetType(Expr->Call.ArgTypes[i]);
						if(!TypesMustMatch(L, R))
						{
							RaiseError(*Expr->ErrorInfo,
									"Passing parameters of different types for generic expression %s and %s", GetTypeName(L), GetTypeName(R));
						}
						GenericID = T->Function.Args[i];
						ResolvedType = Expr->Call.ArgTypes[i];
#endif
					}
					else
					{
						node *Arg = Expr->Call.Args[i];
						ResolvedType = GetTypeFromTypeNode(Checker, Arg);
						GenericTypes.Push(ResolvedType);
						if(ResolvedType == INVALID_TYPE)
						{
							RaiseError(*Arg->ErrorInfo,
									"Couldn't resolve generic call with type from argument #%d", i);
						}
					}
				}
			}
			if(ResolvedType == INVALID_TYPE)
			{
				RaiseError(*Expr->ErrorInfo,
						"Couldn't resolve generic function call");
			}

			{
				const type *RT = GetType(ResolvedType);
				if(IsUntyped(RT))
				{
					ResolvedType = UntypedGetType(RT);
				}
			}

			// @THREADING: NOT THREAD SAFE
			node *FnNode = CopyASTNode(FnSym->Node);
			node *NewFn = AnalyzeGenericFunction(FnSym->Checker, FnNode, ResolvedType, FnSym->Node);
			Expr->Call.Fn = NewFn;
			Expr->Call.Type = NewFn->Fn.TypeIdx;
			Expr->Call.GenericTypes = SliceFromArray(GenericTypes);

			return Expr;
		} break;
		default: unreachable;
	}
}

void Analyze(checker *Checker, dynamic<node *> &Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_FN)
		{
			node *Node = Nodes[I];
			if((Node->Fn.Flags & SymbolFlag_Intrinsic) == 0)
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

