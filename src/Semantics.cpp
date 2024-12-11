#include "Semantics.h"
#include "ConstVal.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Lexer.h"
#include "Log.h"
#include "Module.h"
#include "Parser.h"
#include "Platform.h"
#include "Type.h"
#include "Memory.h"
#include "VString.h"
extern const type BasicTypes[];
extern const int  BasicTypesCount;

extern const type *BasicBool;
extern const type *UntypedInteger;
extern const type *UntypedFloat;
extern const type *BasicUint;
extern const type *BasicInt;
extern const type *BasicF32;
extern u32 TypeCount;

void RaiseBinaryTypeError(const error_info *ErrorInfo, const type *Left, const type *Right)
{
	RaiseError(*ErrorInfo, "Incompatible types in binary expression: %s and %s",
			GetTypeName(Left), GetTypeName(Right));
}

void FillUntypedStack(checker *Checker, u32 Type)
{
	const type *TypePtr = GetType(Type);
	if(IsGeneric(TypePtr))
		return;

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
				if(IsScopeInOrEq(Type->Generic.Scope, Checker->Scope.TryPeek()))
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
			symbol *s = Checker->Module->Globals[*Node->ID.Name];
			if(s)
				return s;
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
			symbol *s = Import.M->Globals[*Node->Selector.Member];
			if(s)
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
			FindType(Checker, Node->Selector.Member, &m->Name);
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
				u32 Type = GetTypeFromTypeNode(Checker, Operand);
				if(Type != INVALID_TYPE)
				{
					if(GetType(Type)->Kind != TypeKind_Enum)
						RaiseError(*TypeNode->ErrorInfo, "Cannot use `.` selector on type %s", GetTypeName(Type));

					return Type;
				}
				RaiseError(*Operand->ErrorInfo, "Expected module name in selector");
			}
			import Import;
			string SearchName = *Operand->ID.Name;
			if(!FindImportedModule(Checker->Imported, SearchName, &Import))
			{
				u32 Type = FindType(Checker, &SearchName, &Checker->Module->Name);
				if(Type != INVALID_TYPE)
				{
					if(GetType(Type)->Kind != TypeKind_Enum)
						RaiseError(*TypeNode->ErrorInfo, "Cannot use `.` selector on type %s", GetTypeName(Type));

					return Type;
				}
				RaiseError(*Operand->ErrorInfo, "Couldn't find module `%s`", Operand->ID.Name->Data);
			}
			u32 Type = FindType(Checker, TypeNode->Selector.Member, &Import.M->Name);
			if(Type == INVALID_TYPE)
			{
				RaiseError(*TypeNode->ErrorInfo, "Type \"%s\" is not defined in module %s", TypeNode->Selector.Member->Data, TypeNode->Selector.Operand->ID.Name->Data);
			}
			return Type;
		} break;
		case AST_GENERIC:
		{
			if(!Checker->Scope.TryPeek() || (Checker->Scope.Peek()->ScopeNode->Type != AST_FN))
			{
				RaiseError(*TypeNode->ErrorInfo, "Declaring generic type outside of function arguments is not allowed");
			}
			return MakeGeneric(Checker->Scope.Peek(), *TypeNode->Generic.Name);
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
	for(int i = Checker->Scope.Data.Count-1; i >= 0; i--)
	{
		symbol *s = Checker->Scope.Data[i]->Symbols.GetUnstablePtr(*ID);
		if(s)
			return s;
	}
	symbol *s = Checker->Module->Globals[*ID];
	if(s)
		return s;
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
				symbol *s = Import.M->Globals[*LHS->Selector.Member];
				if(s)
				{
					return (s->Flags & SymbolFlag_Public) &&
						((s->Flags & SymbolFlag_Const) == 0);
				}
				else
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
	scope *FnScope = AllocScope(FnNode, Checker->Scope.TryPeek());
	Checker->Scope.Push(FnScope);

	FnNode->Fn.FnModule = Checker->Module;

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
			MakeGeneric(FnScope, *FnNode->Fn.Args[I]->Decl.ID);
		}
		else if(IsGeneric(T))
		{
			FnNode->Fn.Flags |= SymbolFlag_Generic;
			Function.Flags |= SymbolFlag_Generic;
		}
	}
	Function.Return = GetTypeFromTypeNode(Checker, FnNode->Fn.ReturnType);
	if(Function.Return != INVALID_TYPE)
	{
		if(IsGeneric(Function.Return))
		{
			FnNode->Fn.Flags |= SymbolFlag_Generic;
			Function.Flags |= SymbolFlag_Generic;
		}
	}
	
	NewType->Function = Function;
	Checker->Scope.Pop();
	FnNode->Fn.Flags |= Function.Flags;
	return AddType(NewType);
}

void AnalyzeFunctionBody(checker *Checker, dynamic<node *> &Body, node *FnNode, u32 FunctionTypeIdx, node *ScopeNode = NULL)
{
	if(FnNode->Fn.AlreadyAnalyzed)
		return;

	u32 Save = Checker->CurrentFnReturnTypeIdx;
	if(!ScopeNode)
		Checker->Scope.Push(AllocScope(FnNode, Checker->Scope.TryPeek()));
	else
		Checker->Scope.Push(AllocScope(ScopeNode, Checker->Scope.TryPeek()));

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
	if(FunctionType->Function.Flags & SymbolFlag_VarFunc && !IsForeign(FunctionType))
	{
		int I = FunctionType->Function.ArgCount;
		node *Arg = FnNode->Fn.Args[I];
		u32 flags = SymbolFlag_Const;
		u32 ArgType = FindStruct(STR_LIT("__init_Arg"));
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

	Checker->Scope.Pop();
	Checker->CurrentFnReturnTypeIdx = Save;
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
				symbol *Sym = Checker->Module->Globals[*Expr->ID.Name];
				if(Sym)
				{
					Result = Sym->Type;
					break;
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
		case AST_TYPEINFO:
		{
			u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->TypeInfoLookup.Expression);
			const type *ExprType = GetType(ExprTypeIdx);
			if(!HasBasicFlag(ExprType, BasicFlag_TypeID))
			{
				RaiseError(*Expr->ErrorInfo, "#info expected an expression with a type of `type`, got: %s",
						GetTypeName(ExprType));
			}
			Expr->TypeInfoLookup.Type = ExprTypeIdx;
			Result = FindStruct(STR_LIT("__init_TypeInfo"));
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

			Result = INVALID_TYPE;
			b32 HasResult = false;

			ForArray(Idx, Expr->Match.Cases)
			{
				node *Case = Expr->Match.Cases[Idx];
				if(!Case->Case.Body.IsValid())
				{
					RaiseError(*Case->ErrorInfo, "Missing body for case in match statement");
				}

				Checker->Scope.Push(AllocScope(Case, Checker->Scope.TryPeek()));
				b32 CaseReturns = false;
				for(int BodyIdx = 0; BodyIdx < Case->Case.Body.Count; ++BodyIdx)
				{
					node *Node = Case->Case.Body[BodyIdx];
					if(Node->Type == AST_RETURN)
					{
						if(Node->Return.Expression)
						{
							u32 TypeIdx = AnalyzeExpression(Checker, Node->Return.Expression);
							CaseReturns = true;
							if(!HasResult)
							{
								HasResult = true;
								if(Idx != 0)
								{
									RaiseError(*Case->ErrorInfo, "Previous cases do not return a value but this one does");
								}
								Result = TypeIdx;
								const type *T = GetType(Result);
								if(IsUntyped(T))
								{
									Result = UntypedGetType(T);
									FillUntypedStack(Checker, Result);
								}
							}
							else
							{
								TypeCheckAndPromote(Checker, Case->ErrorInfo, Result, TypeIdx, NULL, &Case->Case.Body.Data[BodyIdx]);
							}
						}
					}
					else
					{
						AnalyzeNode(Checker, Node);
					}
				}
				if(!CaseReturns && HasResult)
				{
					RaiseError(*Case->ErrorInfo, "Missing return in a match that returns a value");
				}
				CheckBodyForUnreachableCode(Case->Case.Body);
				Checker->Scope.Pop();
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

			{
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
					const type *ArgTMaybeGeneric = GetTypeRaw(CallType->Function.Args[Idx]);
					if(IsGeneric(ArgTMaybeGeneric))
					{
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
							//FillUntypedStack(Checker, UntypedGetType(ExprType));
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
						ExprTypeIdx = CallType->Function.Args[Idx];
					}
					ArgTypes.Push(ExprTypeIdx);
				}
				Expr->Call.ArgTypes = SliceFromArray(ArgTypes);
			}

			Expr->Call.Type = CallTypeIdx;

			if(CallType->Function.Flags & SymbolFlag_Generic)
			{
				string IDOut = {};
				node *Node = AnalyzeGenericExpression(Checker, Expr, &IDOut);

				Expr->Call.Fn = MakeID(Expr->ErrorInfo, DupeType(IDOut, string));
				Expr->Call.Type = Node->Fn.TypeIdx;
				CallType = GetType(Expr->Call.Type);
			}

			Result = GetReturnType(CallType);
		} break;
		case AST_EMBED:
		{
			string File = ReadEntireFile(*Expr->Embed.FileName);
			if(File.Data == NULL)
				RaiseError(*Expr->ErrorInfo, "Couldn't open #embed_%s file %s", Expr->Embed.IsString ? "str" : "bin", Expr->Embed.FileName->Data);

			Expr->Embed.Content = File;
			Result = Expr->Embed.IsString ? Basic_string : GetPointerTo(Basic_u8);
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
			if(ExprType == Basic_type)
			{
				Expr->Size.Type = GetTypeFromTypeNode(Checker, Expr->Size.Expression);
			}
			else
			{
				Expr->Size.Type = ExprType;
			}
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
				*Expr = *Expr->Cast.Expression;
				Result = From;
			}
			else
			{
				Expr->Cast.FromType = From;
				Expr->Cast.ToType = To;
				Result = To;

				if(IsUntyped(FromType))
				{
					FillUntypedStack(Checker, To);
					memcpy(Expr, Expr->Cast.Expression, sizeof(node));
				}
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
					if(!IsString(Type))
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

			if(Expr->TypeList.Items.Count == 0 && Type->Kind == TypeKind_Struct &&
					(Type->Struct.Flags & StructFlag_Generic))
			{
				RaiseError(*Expr->ErrorInfo, "Cannot 0 initialize generic struct %s", GetTypeName(Type));
			}

			node *Filled[4096] = {};
			u32 ExprTypes[4096] = {};
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
						Filled[Idx] = Item;
						PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Type->Array.Type, ItemType, NULL, &Item->Item.Expression);
					} break;
					case TypeKind_Slice:
					{
						Filled[Idx] = Item;
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
						Filled[MemberIdx] = Item;
						ExprTypes[MemberIdx] = ItemType;
						struct_member Mem = Type->Struct.Members[MemberIdx];
						PromotedUntyped = INVALID_TYPE;
						if(!IsGeneric(Mem.Type))
							PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Mem.Type, ItemType, NULL, &Item->Item.Expression);
					} break;
					case TypeKind_Basic:
					{
						Assert(HasBasicFlag(Type, BasicFlag_String));

						int MemberIdx = Idx;
						if(NamePtr)
						{
							string Name = *NamePtr;
							if(Name == STR_LIT("data"))
							{
								MemberIdx = 0;
							}
							else if(Name == STR_LIT("count"))
							{
								MemberIdx = 1;
							}
							else
							{
								RaiseError(*Item->ErrorInfo, "No member named %s in string",
										Name.Data);
							}
						}
						u32 Type = INVALID_TYPE;
						if(MemberIdx == 0)
							Type = GetPointerTo(Basic_u8);
						else
							Type = Basic_int;

						Filled[MemberIdx] = Item;
						ExprTypes[MemberIdx] = ItemType;
						PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Type, ItemType, NULL, &Item->Item.Expression);
					} break;
					default: unreachable;
				}

				if(PromotedUntyped != INVALID_TYPE)
					FillUntypedStack(Checker, PromotedUntyped);
			}
			if(Type->Kind == TypeKind_Struct && Type->Struct.Flags & StructFlag_Generic)
			{
				dynamic<struct_member> Members = {};
				u32 GenericResolved = INVALID_TYPE;
				ForArray(Idx, Type->Struct.Members)
				{
					struct_member Member = Type->Struct.Members[Idx];
					struct_member NewMember = {};
					NewMember.ID = Member.ID;
					NewMember.Type = Member.Type;
					const type *MT = GetType(Member.Type);
					if(HasBasicFlag(MT, BasicFlag_TypeID))
					{
						if(Filled[Idx] == NULL) {
							RaiseError(*Expr->ErrorInfo,
									"Type field needs to be specified in initialization of struct %s",
									GetTypeName(Type));
						}
						node *Expr = Filled[Idx]->Item.Expression;
						GenericResolved = GetTypeFromTypeNode(Checker, Expr);
						FillUntypedStack(Checker, GenericResolved);
					}
					Members.Push(NewMember);
				}
				ForArray(Idx, Type->Struct.Members)
				{
					struct_member Member = Type->Struct.Members[Idx];
					const type *MT = GetType(Member.Type);
					if(IsGeneric(MT))
					{
						// @NOTE: This shouldn't be able to happen because it's checked previously
						Assert(GenericResolved != INVALID_TYPE);
						u32 NonGeneric = ToNonGeneric(Member.Type, GenericResolved, Member.Type);
						TypeCheckAndPromote(Checker, Expr->ErrorInfo, NonGeneric, ExprTypes[Idx], NULL,
								&Filled[Idx]->Item.Expression);
						Members.Data[Idx].Type = NonGeneric;
					}
				}

				TypeIdx = MakeStruct(SliceFromArray(Members), Type->Struct.Name, Type->Struct.Flags & (~StructFlag_Generic));
			}
			Expr->TypeList.Type = TypeIdx;
			Result = TypeIdx;
		} break;
		case AST_SELECTOR:
		{
			u32 TypeIdx = AnalyzeExpression(Checker, Expr->Selector.Operand);
			const type *Type = NULL;
			if(TypeIdx == Basic_module)
			{
				Assert(Expr->Selector.Operand->Type == AST_ID);
				symbol *s = FindSymbolFromNode(Checker, Expr);
				if(!s)
				{
					const string *ModuleName = Expr->Selector.Operand->ID.Name;
					u32 t = FindType(Checker, Expr->Selector.Member, ModuleName);
					if(t == INVALID_TYPE)
					{
						RaiseError(*Expr->ErrorInfo,
								"Cannot find public symbol %s in module %s",
								Expr->Selector.Member->Data, Expr->Selector.Operand->ID.Name->Data);
					}
					else
					{
						Result = Basic_type;
						Expr->Selector.Type = Basic_type;
					}
				}
				else
				{
					Result = s->Type;
					Expr->Selector.Type = s->Type;
				}
			}
			else
			{
				Expr->Selector.Type = TypeIdx;
				Type = GetType(TypeIdx);
				switch(Type->Kind)
				{
					case TypeKind_Basic:
					{
						if(HasBasicFlag(Type, BasicFlag_TypeID))
						{
#if 0
							if(Expr->Selector.Operand->Type != AST_ID)
							{
								RaiseError(*Expr->ErrorInfo, "Invalid `.`! Cannot use selector on a typeid");
							}
							u32 TIdx = FindType(Checker, Expr->Selector.Operand->ID.Name);
#endif
							u32 TIdx = GetTypeFromTypeNode(Checker, Expr);
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
								RaiseError(*Expr->ErrorInfo, "No member named %s in enum %s; Invalid `.` selector",
										Expr->Selector.Member->Data, GetTypeName(T));
							}
						}
						else if(IsString(Type))
						{
							if(*Expr->Selector.Member == STR_LIT("count"))
							{
								Expr->Selector.Index = 1;
								Result = Basic_int;
							}
							else if(*Expr->Selector.Member == STR_LIT("data"))
							{
								Expr->Selector.Index = 0;
								Result = GetPointerTo(Basic_u8);
							}
							else
							{
								RaiseError(*Expr->ErrorInfo, "Only .data and .count can be accessed on a string");
							}
						}
						else
						{
							RaiseError(*Expr->ErrorInfo, "Cannot use `.` selector operator on %s", GetTypeName(Type));
						}
					} break;
					case TypeKind_Slice:
					{
ANALYZE_SLICE_SELECTOR:
						if(*Expr->Selector.Member == STR_LIT("count"))
						{
							Expr->Selector.Index = 0;
							Result = Basic_int;
						}
						else if(*Expr->Selector.Member == STR_LIT("data"))
						{
							Expr->Selector.Index = 1;
							Result = GetPointerTo(Type->Slice.Type);
						}
						else
						{
							RaiseError(*Expr->ErrorInfo, "Only .count and .data can be accessed on this type");
						}
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
							RaiseError(*Expr->ErrorInfo, "No member named %s in enum %s; Invalid `.` selector",
									Expr->Selector.Member->Data, GetTypeName(Type));
						}
					} break;
					case TypeKind_Pointer:
					{

						const type *Pointed = NULL;
						if(Type->Pointer.Pointed != INVALID_TYPE)
							Pointed = GetType(Type->Pointer.Pointed);
						if(!Pointed || (Pointed->Kind != TypeKind_Struct && Pointed->Kind != TypeKind_Slice))
						{
							RaiseError(*Expr->ErrorInfo, "Cannot use `.` selector operator on a pointer that doesn't directly point to a struct. %s",
									GetTypeName(Type));
						}
						if(Type->Pointer.Flags & PointerFlag_Optional)
						{
							RaiseError(*Expr->ErrorInfo, "Cannot derefrence optional pointer, check for null and then mark it non optional with the ? operator");
						}
						Type = Pointed;
						if(Pointed->Kind == TypeKind_Slice)
							goto ANALYZE_SLICE_SELECTOR;
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
							RaiseError(*Expr->ErrorInfo, "No member named %s in struct %s; Invalid `.` selector",
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
			const type *OperandType = GetType(OperandTypeIdx);

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
					RaiseError(*Expr->ErrorInfo, "Cannot index type %s", GetTypeName(OperandType));
				} break;
				default:
				{
					RaiseError(*Expr->ErrorInfo, "Cannot index type %s", GetTypeName(OperandType));
				} break;
			}
			FillUntypedStack(Checker, Result);

			u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->Index.Expression);
			const type *ExprType = GetType(ExprTypeIdx);
			if(ExprType->Kind != TypeKind_Basic || (ExprType->Basic.Flags & BasicFlag_Integer) == 0)
			{
				RaiseError(*Expr->ErrorInfo, "Indexing expression needs to be of an integer type");
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
			Result = Basic_u32;
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
		case AST_PTRTYPE:
		{
			Expr->PointerType.Analyzed = GetTypeFromTypeNode(Checker, Expr);
			Result = Basic_type;
		} break;
		case AST_ARRAYTYPE:
		{
			Expr->ArrayType.Analyzed = GetTypeFromTypeNode(Checker, Expr);
			Result = Basic_type;
		} break;
		default:
		{
			LDEBUG("TYPE: %d", Expr->Type);
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
				case T_MINUS:
				{
					u32 TypeIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *T = GetType(TypeIdx);
					if(!HasBasicFlag(T, BasicFlag_Integer) && !HasBasicFlag(T, BasicFlag_Float))
					{
						RaiseError(*Expr->ErrorInfo, "Unary `-` can only be used on integers and floats, but here it is used on %s",
								GetTypeName(T));
					}
					if(HasBasicFlag(T, BasicFlag_Unsigned))
					{
						RaiseError(*Expr->ErrorInfo, "Cannot use a unary `-` on an unsigned type %s", GetTypeName(T));
					}
					Expr->Unary.Type = TypeIdx;
					if(IsUntyped(T))
						Checker->UntypedStack.Push(&Expr->Unary.Type);

					return TypeIdx;
				} break;
				case T_BANG:
				{
					AnalyzeBooleanExpression(Checker, &Expr->Unary.Operand);
					return Basic_bool;
				} break;
				case T_QMARK:
				{
					u32 PointerIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *Pointer = GetType(PointerIdx);
					if(PointerIdx == Basic_type)
					{
						u32 OptionalType = GetTypeFromTypeNode(Checker, Expr->Unary.Operand);
						const type *Opt = GetType(OptionalType);
						if(Opt->Kind != TypeKind_Pointer)
						{
							RaiseError(*Expr->ErrorInfo, "Cannot declare optional non pointer type: %s", GetTypeName(Opt));
						}
						Assert(Expr->Unary.Operand->Type == AST_PTRTYPE);
						Expr->Unary.Operand->PointerType.Analyzed = GetOptional(GetType(Expr->Unary.Operand->PointerType.Analyzed));
						Expr->Unary.Operand->PointerType.Flags |= PointerFlag_Optional;
						*Expr = *Expr->Unary.Operand;
						return Basic_type;
					}
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
					if(PointerIdx == Basic_type)
					{
						*Expr = *MakePointerType(Expr->ErrorInfo, Expr->Unary.Operand);
						Expr->PointerType.Analyzed = GetTypeFromTypeNode(Checker, Expr);
						return Basic_type;
					}

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

			if(BinaryExpression->Binary.Right->Type == AST_CAST && GetType(BinaryExpression->Binary.Right->Cast.ToType)->Kind == TypeKind_Pointer)
			{
				BinaryExpression->Binary.Right = BinaryExpression->Binary.Right->Cast.Expression;
			}

			if(BinaryExpression->Binary.Op == '-')
			{
				BinaryExpression->Binary.Right = MakeUnary(BinaryExpression->ErrorInfo, BinaryExpression->Binary.Right, T_MINUS);
				BinaryExpression->Binary.Right->Unary.Type = Right;
				if(IsUntyped(RightType))
					BinaryExpression->Binary.Right->Unary.Type = Basic_int;
			}
			node *OverwriteIndex = MakeIndex(BinaryExpression->ErrorInfo,
					BinaryExpression->Binary.Left, BinaryExpression->Binary.Right);
			OverwriteIndex->Index.OperandType = Left;
			OverwriteIndex->Index.IndexedType = LeftType->Pointer.Pointed;
			OverwriteIndex->Index.ForceNotLoad = true;

			memcpy(BinaryExpression, OverwriteIndex, sizeof(node));
		}
		else if(LeftType->Kind == TypeKind_Pointer && RightType->Kind == TypeKind_Pointer && BinaryExpression->Binary.Op == '-')
		{
			if(LeftType->Pointer.Pointed != RightType->Pointer.Pointed)
			{
				const char *LeftName = LeftType->Pointer.Pointed == INVALID_TYPE ? "void" : GetTypeName(LeftType->Pointer.Pointed);
				const char *RightName = RightType->Pointer.Pointed == INVALID_TYPE ? "void" : GetTypeName(RightType->Pointer.Pointed);
				RaiseError(*Expr->ErrorInfo, "Cannot do a pointer diff between 2 pointers of different types %s and %s",
						LeftName, RightName);
			}
			node *OverwritePtrDiff = MakePointerDiff(BinaryExpression->ErrorInfo,
					BinaryExpression->Binary.Left, BinaryExpression->Binary.Right, Left);

			memcpy(BinaryExpression, OverwritePtrDiff, sizeof(node));
			Result = Basic_int;
		}
		else
		{
			Expr->Binary.ExpressionType = Promoted;
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
	if((Flags & SymbolFlag_Shadow) == 0)
	{
		scope *scope = Checker->Scope.TryPeek();
		if(scope)
		{
			const symbol *s = scope->Symbols.GetUnstablePtr(*ID);
			if(s)
			{
				RaiseError(*ErrorInfo,
						"Redeclaration of variable %s.\n"
						"If this is intentional mark it as a shadow like this:\n\t#shadow %s := 0;",
						ID->Data, ID->Data);
			}
		}
	}
	symbol Symbol;
	Symbol.Node    = Node;
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

	Assert(Checker->Scope.TryPeek());
	bool Success = Checker->Scope.Peek()->Symbols.Add(*ID, Symbol);
	Assert(Success);
}

const u32 AnalyzeDeclerations(checker *Checker, node *Node, b32 NoAdd = false)
{
	Assert(Node->Type == AST_DECL);
	const string *ID = Node->Decl.ID;
	u32 Type = GetTypeFromTypeNode(Checker, Node->Decl.Type);
	if(Node->Decl.Expression)
	{
		u32 ExprType = AnalyzeExpression(Checker, Node->Decl.Expression);
		if(ExprType == INVALID_TYPE)
		{
			RaiseError(*Node->ErrorInfo, "Expression does not give a value for the assignment");
		}
		const type *ExprTypePointer = GetType(ExprType);

		if(Type != INVALID_TYPE)
		{
			const type *TypePointer = GetType(Type);
			const type *Promotion = NULL;
			if(!IsTypeCompatible(TypePointer, ExprTypePointer, &Promotion, true))
			{
				RaiseError(*Node->ErrorInfo, "Cannot assign expression of type %s to variable of type %s",
						GetTypeName(ExprTypePointer), GetTypeName(TypePointer));
			}
			if(Promotion)
			{
				if(!IsUntyped(ExprTypePointer))
				{
					Node->Decl.Expression = MakeCast(Node->ErrorInfo, Node->Decl.Expression, NULL, ExprType, Type);
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

	if(NoAdd)
	{
	}
	else
	{
		AddVariable(Checker, Node->ErrorInfo, Type, ID, Node, Node->Decl.Flags);
	}
	return Type;
}

void AnalyzeInnerBody(checker *Checker, slice<node *> Body)
{
	for(int Idx = 0; Idx < Body.Count; ++Idx)
	{
		AnalyzeNode(Checker, Body[Idx]);
	}
	CheckBodyForUnreachableCode(Body);
}

u32 AnalyzeBooleanExpression(checker *Checker, node **NodePtr)
{
	node *Node = *NodePtr;
	u32 ExprTypeIdx = AnalyzeExpression(Checker, Node);
	const type *ExprType = GetType(ExprTypeIdx);
	if(ExprType->Kind != TypeKind_Basic && ExprType->Kind != TypeKind_Pointer)
	{
		RaiseError(*Node->ErrorInfo, "Expected boolean type for condition expression, got %s.",
				GetTypeName(ExprType));
	}
	if(ExprType->Kind == TypeKind_Basic && ((ExprType->Basic.Flags & BasicFlag_Boolean) == 0))
	{
		const_value ZeroValue = {};
		ZeroValue.Type = const_type::Integer;
		node *Zero = MakeConstant(Node->ErrorInfo, ZeroValue);
		Zero->Constant.Type = ExprTypeIdx;
		*NodePtr = MakeBinary(Node->ErrorInfo, Node, Zero, T_NEQ);
	}
	else if(ExprType->Kind == TypeKind_Pointer)
	{
		node *Null = MakeReserve(Node->ErrorInfo, reserved::Null);
		Null->Reserved.Type = NULLType;
		*NodePtr = MakeBinary(Node->ErrorInfo, Node, Null, T_NEQ);
	}
	return ExprTypeIdx;
}

void AnalyzeIf(checker *Checker, node *Node)
{
	Checker->Scope.Push(AllocScope(Node, Checker->Scope.TryPeek()));
	AnalyzeBooleanExpression(Checker, &Node->If.Expression);
	slice<node *> IfBody = SliceFromArray(Node->If.Body); 
	AnalyzeInnerBody(Checker, IfBody);
	if(Node->If.Else.IsValid())
	{
		slice<node *> IfElse = SliceFromArray(Node->If.Else); 
		AnalyzeInnerBody(Checker, IfElse);
	}
	Checker->Scope.Pop();
}

void AnalyzeFor(checker *Checker, node *Node)
{
	Checker->Scope.Push(AllocScope(Node, Checker->Scope.TryPeek()));
	using ft = for_type;
	switch(Node->For.Kind)
	{
		case ft::C:
		{
			if(Node->For.Expr1)
				AnalyzeDeclerations(Checker, Node->For.Expr1);
			if(Node->For.Expr2)
			{
				AnalyzeBooleanExpression(Checker, &Node->For.Expr2);
			}
			if(Node->For.Expr3)
				AnalyzeExpression(Checker, Node->For.Expr3);
		} break;
		case ft::It:
		{
			u32 TypeIdx = AnalyzeExpression(Checker, Node->For.Expr2);
			const type *T = GetType(TypeIdx);
			if(!IsTypeIterable(T))
			{
				RaiseError(*Node->For.Expr2->ErrorInfo,
						"Expression is of non iteratable type %s", GetTypeName(T));
			}
			if(IsUntyped(T))
			{
				TypeIdx = Basic_int;
				T = GetType(TypeIdx);
				FillUntypedStack(Checker, TypeIdx);
			}
			Assert(Node->For.Expr1->Type == AST_ID);
			u32 ItType = INVALID_TYPE;
			if(T->Kind == TypeKind_Array)
				ItType = T->Array.Type;
			else if(T->Kind == TypeKind_Slice)
				ItType = T->Slice.Type;
			else if(HasBasicFlag(T, BasicFlag_String))
				ItType = Basic_u32;
			else if(HasBasicFlag(T, BasicFlag_Integer))
				ItType = TypeIdx;
			else
				Assert(false);

			AddVariable(Checker, Node->For.Expr1->ErrorInfo, ItType,
					Node->For.Expr1->ID.Name, Node->For.Expr1, 0);
			if(T->Kind == TypeKind_Array || T->Kind == TypeKind_Slice || HasBasicFlag(T, BasicFlag_String))
			{
				string *n = NewType(string);
				*n = STR_LIT("i");

				AddVariable(Checker, Node->For.Expr1->ErrorInfo, Basic_int,
						n, Node->For.Expr1, 0);
			}
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
	Checker->Scope.Pop();
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
				return GetPointerTo(NewPointed);
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
				return GetArrayType(NewArrayType, Ptr->Array.MemberCount);
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
			b32 Parsing = true;
			node *Expr = Item.Expression;
			while(Parsing)
			{
				b32 IsNeg = false;
				switch(Expr->Type)
				{
					case AST_CONSTANT:
					{
						Member.Value = Expr->Constant.Value;
						if(Member.Value.Type != const_type::Integer)
						{
							RaiseError(*Expr->ErrorInfo, "Enum member value must be an integer");
						}
						if(IsNeg)
						{
							using ct = const_type;
							switch(Member.Value.Type)
							{
								case ct::Integer:
								{
									Member.Value.Int.IsSigned = true;
									Member.Value.Int.Signed = -Member.Value.Int.Signed;
								} break;
								case ct::Float:
								{
									Member.Value.Float = -Member.Value.Float;
								} break;
								case ct::String:
								{
									RaiseError(*Expr->ErrorInfo, "Cannot use - operator on a string");
								} break;
							}
						}
						Parsing = false;
					} break;
					case AST_CHARLIT:
					{
						const_value Value = {};
						Value.Type = const_type::Integer;
						Value.Int.IsSigned = false;
						Value.Int.Unsigned = Node->CharLiteral.C;
						Member.Value = Value;
						if(IsNeg)
						{
							RaiseError(*Expr->ErrorInfo, "Cannot use - operator on a char literal");
						}
						Parsing = false;
					} break;
					case AST_UNARY:
					{
						if(Expr->Unary.Op != T_MINUS)
						{
							RaiseError(*Expr->ErrorInfo, "Cannot use this unary operator on an enum literal value");
						}
						IsNeg = !IsNeg;
						Expr = Expr->Unary.Operand;
					} break;
					default:
					{
						RaiseError(*Expr->ErrorInfo, "Enum member value must be a constant integer");
					} break;
				}
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
	scope *StructScope = AllocScope(Node, Checker->Scope.TryPeek());
	Checker->Scope.Push(StructScope);

	type New = {};
	New.Kind = TypeKind_Struct;
	New.Struct.Name = *Node->StructDecl.Name;

	array<struct_member> Members {Node->StructDecl.Members.Count};
	ForArray(Idx, Node->StructDecl.Members)
	{
		u32 Type = GetTypeFromTypeNode(Checker, Node->StructDecl.Members[Idx]->Decl.Type);
		Type = FixPotentialFunctionPointer(Type);
		const type *T = GetType(Type);
		if(HasBasicFlag(T, BasicFlag_TypeID))
		{
			MakeGeneric(StructScope, *Node->StructDecl.Members[Idx]->Decl.ID);
		}
		else if(IsGeneric(T))
		{
			if(New.Struct.Flags & StructFlag_Generic)
			{
				RaiseError(*Node->ErrorInfo, "Structs cannot have more than 1 generic type");
			}
		}

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
	if(Node->StructDecl.IsUnion)
	{
		if(Node->StructDecl.Members.Count == 0)
			RaiseError(*Node->ErrorInfo, "Empty unions are not allowed");
		New.Struct.Flags |= StructFlag_Union;
	}

	FillOpaqueStruct(OpaqueType, New);
	Checker->Scope.Pop();
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
			if(!IsNodeEndScope(Body[Idx+1]) || Idx + 2 != Body.Count)
				RaiseError(*Body[Idx + 1]->ErrorInfo, "Unreachable code after break statement");
		}
		if(Node->Type == AST_CONTINUE && Idx + 1 != Body.Count)
		{
			if(!IsNodeEndScope(Body[Idx+1]) || Idx + 2 != Body.Count)
				RaiseError(*Body[Idx + 1]->ErrorInfo, "Unreachable code after continue statement");
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
			AnalyzeIf(Checker, Node);
		} break;
		case AST_CONTINUE:
		{
			b32 FoundContinueScope = false;
			scope *Current = Checker->Scope.TryPeek();

			while(Current)
			{
				if(Current->ScopeNode->Type == AST_FOR)
				{
					FoundContinueScope = true;
					break;
				}
				Current = Current->Parent;
			}
			if(!FoundContinueScope)
			{
				RaiseError(*Node->ErrorInfo, "Invalid context for continue, not a for loop");
			}

		} break;
		case AST_BREAK:
		{
			b32 FoundBreakableScope = false;
			scope *Current = Checker->Scope.TryPeek();
			b32 MatchError = false;

			while(Current)
			{
				if(Current->ScopeNode->Type == AST_MATCH)
					MatchError = true;
				else if(Current->ScopeNode->Type == AST_FOR)
				{
					FoundBreakableScope = true;
					break;
				}
				Current = Current->Parent;
			}
			if(!FoundBreakableScope)
			{
				if(MatchError)
				{
					RaiseError(*Node->ErrorInfo, "Invalid context for break, not a for loop to break out of a match statement use return instead");
				}
				else
				{
					RaiseError(*Node->ErrorInfo, "Invalid context for break, not a for loop");
				}
			}
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
		case AST_SCOPE:
		{
			if(Node->ScopeDelimiter.IsUp)
			{
				Checker->Scope.Push(AllocScope(Node, Checker->Scope.TryPeek()));
			}
			else
			{
				if(!Checker->Scope.TryPeek() || !Checker->Scope.Peek()->Parent)
				{
					RaiseError(*Node->ErrorInfo, "Unexpected scope closing }");
				}
				Checker->Scope.Pop();
			}
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

symbol *CreateFunctionSymbol(checker *Checker, node *Node)
{
	string Name = *Node->Fn.Name;

	symbol *Sym = NewType(symbol);
	Sym->Checker = Checker;
	Sym->Name = Node->Fn.Name;
	Sym->Type = Node->Fn.TypeIdx;
	Sym->Flags = SymbolFlag_Function | SymbolFlag_Const | Node->Fn.Flags;
	Sym->Node = Node;
	if(!Node->Fn.Body.IsValid())
		Sym->Flags |= SymbolFlag_Extern;
	if(Node->Fn.LinkName)
		Sym->LinkName = Node->Fn.LinkName;
	else if(Node->Fn.Flags & SymbolFlag_Foreign)
		Sym->LinkName = Node->Fn.Name;
	else
		Sym->LinkName = StructToModuleNamePtr(Name, Checker->Module->Name);
	return Sym;
}

symbol *AnalyzeFunctionDecl(checker *Checker, node *Node)
{
	u32 FnType = CreateFunctionType(Checker, Node);
	Node->Fn.TypeIdx = FnType;
	return CreateFunctionSymbol(Checker, Node);
}

void AnalyzeFunctionDecls(checker *Checker, dynamic<node *> *NodesPtr, module *ThisModule)
{
	Checker->Nodes = NodesPtr;
	Checker->Module = ThisModule;
	Checker->Scope = {};
	Checker->CurrentFnReturnTypeIdx = INVALID_TYPE;

	slice<node *> Nodes = SliceFromArray(*NodesPtr);

	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_FN)
		{
			node *Node = Nodes[I];
			symbol *Sym = AnalyzeFunctionDecl(Checker, Node);
			bool Success = Checker->Module->Globals.Add(*Node->Fn.Name, Sym);
			if(!Success)
			{
				symbol *Redifined = Checker->Module->Globals[*Node->Fn.Name];
				Assert(Redifined);
				RaiseError(*Nodes[I]->ErrorInfo, "Function %s redifines other symbol in file %s at (%d:%d)",
						Node->Fn.Name->Data,
						Redifined->Node->ErrorInfo->FileName, Redifined->Node->ErrorInfo->Line, Redifined->Node->ErrorInfo->Character);
			}
		}
	}

	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_DECL)
		{
			node *Node = Nodes[I];
			string Name = *Node->Decl.ID;
			u32 Type = AnalyzeDeclerations(Checker, Node, true);
			symbol *Sym = NewType(symbol);
			Sym->Checker = Checker;
			Sym->Name = Node->Decl.ID;
			Sym->LinkName = StructToModuleNamePtr(Name, ThisModule->Name);
			Sym->Type = Type;
			Sym->Flags = Node->Decl.Flags;
			Sym->Node = Node;
			bool Success = Checker->Module->Globals.Add(*Node->Decl.ID, Sym);
			if(!Success)
			{
				symbol *Redifined = Checker->Module->Globals[*Node->Fn.Name];
				Assert(Redifined);
				RaiseError(*Nodes[I]->ErrorInfo, "Variable %s redifines other symbol in file %s at (%d:%d)",
						Node->Fn.Name->Data,
						Redifined->Node->ErrorInfo->FileName, Redifined->Node->ErrorInfo->Line, Redifined->Node->ErrorInfo->Character);
			}
		}
	}
}

void AnalyzeEnums(checker *Checker, slice<node *>Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_ENUM)
		{
			AnalyzeEnum(Checker, Nodes[I]);
		}
	}
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

string MakeNonGenericName(string GenericName)
{
	b32 IsNameGeneric = false;
	for(int i = 0; i < GenericName.Size; ++i)
	{
		if(GenericName.Data[i] == ':')
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
		if(GenericName.Data[i] == ':')
			break;

		Builder += GenericName.Data[i];
	}

	return MakeString(Builder);
}

string *MakeGenericName(string BaseName, u32 FnTypeNonGeneric, u32 FnTypeGeneric, node *ErrorNode)
{
	const type *FG = GetType(FnTypeGeneric);
	const type *T = GetType(FnTypeNonGeneric);
	string_builder Builder = MakeBuilder();
	Builder += BaseName;
	Builder += ':';
	for(int i = 0; i < T->Function.ArgCount; ++i)
	{
		if(IsGeneric(FG->Function.Args[i]))
		{
			u32 ResolvedGenericID = GetGenericPart(T->Function.Args[i], FG->Function.Args[i]);
			if(ResolvedGenericID == INVALID_TYPE)
			{
				// @NOTE: I think this is checked earilier but just to be sure
				RaiseError(*ErrorNode->ErrorInfo, "Invalid type for generic declaration");
			}
			const type *RG = GetType(ResolvedGenericID);
			if(RG->Kind == TypeKind_Struct)
			{
				Builder += "::";
				ForArray(Idx, RG->Struct.Members)
				{
					Builder += GetTypeNameAsString(RG->Struct.Members[Idx].Type);
					Builder += '_';
				}
				Builder += "::";
			}
		}
		Builder += GetTypeNameAsString(T->Function.Args[i]);
		Builder += '_';
	}
	Builder += ':';
	if(T->Function.Return == INVALID_TYPE)
		Builder += "void";
	else
		Builder += GetTypeNameAsString(T->Function.Return);
	string Result = MakeString(Builder);
	return DupeType(Result, string);
}

u32 FunctionTypeGetNonGeneric(const type *Old, u32 ResolvedType, node *Call, node *FnError)
{
	type *NewFT = AllocType(TypeKind_Function);
	NewFT->Function.Return = Old->Function.Return;
	NewFT->Function.Flags = Old->Function.Flags & ~SymbolFlag_Generic;
	NewFT->Function.ArgCount = Old->Function.ArgCount;
	NewFT->Function.Args = (u32 *)AllocatePermanent(sizeof(u32) * Old->Function.ArgCount);
	for(int i = 0; i < Old->Function.ArgCount; ++i)
	{
		u32 TypeIdx = Old->Function.Args[i];
		const type *T = GetType(TypeIdx);
		if(IsGeneric(T))
		{
			TypeIdx = ToNonGeneric(TypeIdx, ResolvedType, Call->Call.ArgTypes[i]);
		}
		NewFT->Function.Args[i] = TypeIdx;
	}
	u32 TypeIdx = Old->Function.Return;
	if(TypeIdx != INVALID_TYPE)
	{
		NewFT->Function.Return = ToNonGeneric(TypeIdx, ResolvedType, TypeIdx);
		if(NewFT->Function.Return == INVALID_TYPE || IsGeneric(GetType(NewFT->Function.Return)))
		{
			RaiseError(*FnError->ErrorInfo, "Couldn't resolve the generic return type of the function");
		}
	}

	return AddType(NewFT);
}

node *AnalyzeGenericFunction(checker *Checker, node *FnNode, u32 ResolvedType, node *OriginalNode, node *Call,
		u32 NewFnType, string *GenericName)
{
	node *Result = FnNode;
	Result->Fn.Flags = Result->Fn.Flags & ~SymbolFlag_Generic;
	Result->Fn.Name = GenericName;
	Result->Fn.TypeIdx = NewFnType;

	u32 Save = GetGenericReplacement();
	SetGenericReplacement(ResolvedType);

	AnalyzeFunctionBody(Checker, Result->Fn.Body, Result, FnNode->Fn.TypeIdx, OriginalNode);

	SetGenericReplacement(Save);

	return Result;
}

node *AnalyzeGenericExpression(checker *Checker, node *Generic, string *IDOut)
{
	node *Expr = Generic;
	switch(Expr->Type)
	{
		case AST_CALL:
		{
			u32 SaveRepl = GetGenericReplacement();
			SetGenericReplacement(INVALID_TYPE);
			module *MaybeMod = NULL;
			symbol *FnSym = FindSymbolFromNode(Checker, Expr->Call.Fn, &MaybeMod);
			if(!FnSym)
			{
				RaiseError(*Expr->ErrorInfo, "Couldn't resolve generic function");
			}

			const type *T = GetType(FnSym->Type);
			u32 ResolvedType = INVALID_TYPE;
			dynamic<u32> GenericTypes = {};
			string ResolvedName = {};
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
						if(ResolvedType != INVALID_TYPE)
						{
							if(ResolvedName.Data && ResolvedName != *FnSym->Node->Fn.Args[i]->Decl.ID)
								RaiseError(*Arg->ErrorInfo, "Currently multiple generic arguments are not supported");
						}
						ResolvedName = *FnSym->Node->Fn.Args[i]->Decl.ID;

						ResolvedType = GetTypeFromTypeNode(Checker, Arg);
						GenericTypes.Push(ResolvedType);
						if(ResolvedType == INVALID_TYPE)
						{
							RaiseError(*Arg->ErrorInfo,
									"Couldn't resolve generic call with type from argument #%d", i);
						}
					}
				}
				else if(IsGeneric(ArgT))
				{
					node *Arg = Expr->Call.Args[i];
					u32 GenericID = GetGenericPart(T->Function.Args[i], T->Function.Args[i]);
					const type *G = GetType(GenericID);
					if(ResolvedType != INVALID_TYPE)
					{
						if(ResolvedName.Data && ResolvedName != G->Generic.Name)
							RaiseError(*Arg->ErrorInfo, "Currently multiple generic arguments are not supported");
					}
					else
					{
						ResolvedName = G->Generic.Name;
						ResolvedType = Expr->Call.ArgTypes[i];
						const type *CallArgT = GetType(Expr->Call.ArgTypes[i]);
						if(IsUntyped(CallArgT))
						{
							ResolvedType = UntypedGetType(CallArgT);
						}

						ResolvedType = GetGenericPart(ResolvedType, T->Function.Args[i]);
					}
				}

				if(IsUntyped(GetType(Expr->Call.ArgTypes[i])))
				{
					if(ResolvedType == INVALID_TYPE)
						// This should be caught previously
						RaiseError(*Expr->Call.Args[i]->ErrorInfo, "Call to generic expression doesn't resolve generic type before it's use");
					Expr->Call.ArgTypes.Data[i] = ResolvedType;
					FillUntypedStack(Checker, ResolvedType);
				}
			}

			if(ResolvedType != INVALID_TYPE)
			{
				const type *RT = GetType(ResolvedType);
#if 0
				if(IsGeneric(RT))
				{
					RaiseError(*Expr->ErrorInfo,
							"Cannot resolve generic type with another generic type %s", GetTypeName(RT));
				}
#endif
				if(IsUntyped(RT))
				{
					ResolvedType = UntypedGetType(RT);
				}
			}

			for(int i = 0; i < T->Function.ArgCount; ++i)
			{
				if(IsGeneric(T->Function.Args[i]))
				{
					u32 ArgTIdx = ToNonGeneric(T->Function.Args[i], ResolvedType, T->Function.Args[i]);
					const type *ArgT = GetType(ArgTIdx);
					const type *ExprT = GetType(Expr->Call.ArgTypes[i]);
					const type *PromotionType = NULL;
					if(!IsTypeCompatible(ArgT, ExprT, &PromotionType, true))
					{
						RaiseError(*Expr->ErrorInfo, "Argument #%d is of incompatible type %s, tried to pass: %s",
								i, GetTypeName(ArgT), GetTypeName(ExprT));
					}
					if(IsUntyped(ExprT))
					{
						Expr->Call.ArgTypes.Data[i] = ArgTIdx;
					}
					else if(Expr->Call.ArgTypes[i] != ArgTIdx)
					{
						node *Arg = Expr->Call.Args[i];
						Expr->Call.Args.Data[i] = MakeCast(Arg->ErrorInfo, Arg, NULL,
								Expr->Call.ArgTypes[i], ArgTIdx);
						Expr->Call.ArgTypes.Data[i] = ArgTIdx;
					}
				}
				else
				{
				}
			}

			const type *Old = GetType(FnSym->Node->Fn.TypeIdx);
			u32 NewFnType = FunctionTypeGetNonGeneric(Old, ResolvedType, Expr, FnSym->Node);
			string *GenericName = MakeGenericName(FnSym->Name ? *FnSym->Name : STR_LIT(""), NewFnType, FnSym->Node->Fn.TypeIdx, Expr);
			string FnName = *GenericName;
			if(Checker->Module->Name != FnSym->Checker->Module->Name)
				*IDOut = StructToModuleName(FnName, FnSym->Checker->Module->Name);
			else
				*IDOut = FnName;

			//LDEBUG("Generating generic function from module %s\n\tID: %s\n", Checker->Module->Name.Data, IDOut->Data);
			symbol *Found = FnSym->Checker->Module->Globals[*GenericName];
			node *NewFnNode = NULL;
			if(Found)
			{
				NewFnNode = Found->Node;
			}
			else
			{
				// @THREADING: NOT THREAD SAFE
				node *FnNode = CopyASTNode(FnSym->Node);
				FnNode->Fn.FnModule = FnSym->Checker->Module;
				string_builder Builder = MakeBuilder();
				PushBuilderFormated(&Builder, "Error while parsing generic call to %s at %s(%d:%d)\n",
						FnSym->Name->Data, Expr->ErrorInfo->FileName, Expr->ErrorInfo->Line, Expr->ErrorInfo->Character);
				SetBonusMessage(MakeString(Builder));
				NewFnNode = AnalyzeGenericFunction(FnSym->Checker, FnNode, ResolvedType, FnSym->Node, Expr,
						NewFnType, GenericName);
				SetBonusMessage(STR_LIT(""));
				NewFnNode->Fn.AlreadyAnalyzed = true;
				bool Success = FnSym->Checker->Module->Globals.Add(*NewFnNode->Fn.Name, CreateFunctionSymbol(FnSym->Checker, NewFnNode));
				Assert(Success);
				FnSym->Checker->Nodes->Push(NewFnNode);
			}
			Expr->Call.Fn = NewFnNode;
			Expr->Call.Type = NewFnType;
			Expr->Call.GenericTypes = SliceFromArray(GenericTypes);

			SetGenericReplacement(SaveRepl);


			return NewFnNode;
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
			if((Node->Fn.Flags & SymbolFlag_Intrinsic) == 0 && (Node->Fn.Flags & SymbolFlag_Generic) == 0)
				AnalyzeFunctionBody(Checker, Node->Fn.Body, Node, Node->Fn.TypeIdx);
		}
	}
	ForArray(Idx, Checker->GeneratedGlobalNodes)
	{
		Nodes.Push(Checker->GeneratedGlobalNodes[Idx]);
	}
}

