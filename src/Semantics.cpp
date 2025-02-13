#include "Semantics.h"
#include "CommandLine.h"
#include "ConstVal.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Globals.h"
#include "Lexer.h"
#include "Log.h"
#include "Module.h"
#include "Parser.h"
#include "Platform.h"
#include "Type.h"
#include "Memory.h"
#include "VString.h"
#include "Polymorph.h"
extern u32 TypeCount;

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

u32 FindEnumTypeNoModuleRenaming(checker *Checker, const string *NamePtr)
{
	string Name = *NamePtr;
	for(int I = 0; I < TypeCount; ++I)
	{
		const type *Type = GetType(I);
		if(Type->Kind == TypeKind_Enum)
		{
			if(Name == Type->Enum.Name)
			{
				return I;
			}
		}
	}
	return INVALID_TYPE;
}

u32 FindStructTypeNoModuleRenaming(checker *Checker, const string *NamePtr)
{
	string Name = *NamePtr;
	for(int I = 0; I < TypeCount; ++I)
	{
		const type *Type = GetType(I);
		if(Type->Kind == TypeKind_Struct)
		{
			if(Name == Type->Struct.Name)
			{
				return I;
			}
		}
	}
	return INVALID_TYPE;
}

u32 FindType(checker *Checker, const string *Name, const string *ModuleNameOptional=NULL)
{
	string N = *Name;
	string ModuleName;
	if(ModuleNameOptional == NULL)
	{
		ModuleName = Checker->Module->Name;
	}
	else
	{
		ModuleName = *ModuleNameOptional;
	}

	string AsModule = StructToModuleName(N, ModuleName);
	u32 FoundOnMap = LookupNameOnTypeMap(&AsModule);
	if(FoundOnMap != INVALID_TYPE)
		return FoundOnMap;

	FoundOnMap = LookupNameOnTypeMap(Name);
	if(FoundOnMap != INVALID_TYPE)
		return FoundOnMap;

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
						for(int i = GenericReplacements.Count-1; i >= 0; --i)
						{
							if(GenericReplacements[i].Generic == N)
							{
								return GenericReplacements[i].TypeID;
							}
						}
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

symbol *FindSymbolFromNode(checker *Checker, node *Node, module **OutModule)
{
	switch(Node->Type)
	{
		case AST_ID:
		{
			symbol *s = Checker->Module->Globals[*Node->ID.Name];
			if(s)
				return s;
			string NoNamespace = STR_LIT("*");
			For(Checker->Imported)
			{
				if(it->As == NoNamespace)
				{
					// @THREADING: NOT THREAD SAFE (maybe)
					auto Sym = it->M->Globals[*Node->ID.Name];
					if(Sym)
					{
						return Sym;
					}
				}
			}
			return NULL;
		} break;
		case AST_SELECTOR:
		{
			if(Node->Selector.Operand == NULL)
			{
				RaiseError(true, *Node->ErrorInfo, "Invalid context for automatic enum selector");
			}

			Node->Selector.Index = -1;
			if(Node->Selector.Operand->Type != AST_ID)
			{
				RaiseError(true, *Node->Selector.Operand->ErrorInfo,
						"Invalid use of module");
			}
			string ModuleName = *Node->Selector.Operand->ID.Name;
			import Import;
			if(!FindImportedModule(Checker->Imported, ModuleName, &Import))
			{
				RaiseError(true, *Node->ErrorInfo, "Couldn't find module %s\n", ModuleName.Data);
				return NULL;
			}
			if(OutModule)
				*OutModule = Import.M;

			module *m = Import.M;
			symbol *s = Import.M->Globals[*Node->Selector.Member];
			if(s)
			{
				if((s->Flags & SymbolFlag_Public) == 0)
				{
					RaiseError(false, *Node->ErrorInfo,
							"Cannot access private member %s in module %s",
							Node->Selector.Member->Data, ModuleName.Data);
				}
				Node->Selector.Operand->ID.Name = DupeType(m->Name, string);
				return s;
			}
			//FindType(Checker, Node->Selector.Member, &m->Name);
			Node->Selector.Operand->ID.Name = DupeType(m->Name, string);
			return NULL;
		} break;
		default: unreachable;
	}
}

u32 FindTypeNoNamespaceImport(checker *Checker, const string *Name)
{
	string NoNamespace = STR_LIT("*");
	For(Checker->Imported)
	{
		if(it->As == NoNamespace)
		{
			u32 Got = FindType(Checker, Name, &it->M->Name);
			if(Got != INVALID_TYPE)
			{
				return Got;
			}
		}
	}
	return INVALID_TYPE;
}

u32 GetTypeFromTypeNode(checker *Checker, node *TypeNode, b32 Error, b32 *OutAutoDef)
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
				Type = FindTypeNoNamespaceImport(Checker, Name);

			if(Type == INVALID_TYPE)
			{

				if(!Error)
					return INVALID_TYPE;
				// @NOTE: is this a good idea?
				RaiseError(false, *TypeNode->ErrorInfo, "Type \"%s\" is not defined", Name->Data);
				return Basic_int;
			}
			return Type;
		} break;
		case AST_PTRTYPE:
		{
			u32 Pointed = GetTypeFromTypeNode(Checker, TypeNode->PointerType.Pointed, Error, OutAutoDef);
			if(TypeNode->PointerType.Pointed != NULL && Pointed == INVALID_TYPE && !Error)
				return INVALID_TYPE;
			return GetPointerTo(Pointed, TypeNode->PointerType.Flags);
		} break;
		case AST_ARRAYTYPE:
		{
			uint Size = 0;
			u32 MemberType = GetTypeFromTypeNode(Checker, TypeNode->ArrayType.Type, Error, OutAutoDef);
			if(MemberType == INVALID_TYPE)
				return INVALID_TYPE;
			if(TypeNode->ArrayType.Expression)
			{
				node *Expr = TypeNode->ArrayType.Expression;
				bool Failed = false;
				if(Expr->Type != AST_CONSTANT || Expr->Constant.Value.Type != const_type::Integer)
				{
					RaiseError(false, *Expr->ErrorInfo, "Expected constant integer for array type size");
					Failed = true;
				}
				if(Expr->Constant.Value.Int.IsSigned && Expr->Constant.Value.Int.Signed <= 0)
				{
					RaiseError(false, *Expr->ErrorInfo, "Expected positive integer for array type size");
					Failed = true;
				}
				if(Expr->Constant.Value.Int.IsSigned && Expr->Constant.Value.Int.Unsigned >= MB(1))
				{
					RaiseError(false, *Expr->ErrorInfo, "Value given for array type size is too big, cannot reliably allocate it on the stack");
					Failed = true;
				}

				if(!Failed)
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
			if(TypeNode->Fn.ReturnTypes.IsValid())
			{
				array<u32> Returns = array<u32>(TypeNode->Fn.ReturnTypes.Count);
				ForArray(Idx, TypeNode->Fn.ReturnTypes)
				{
					Returns[Idx] = GetTypeFromTypeNode(Checker, TypeNode->Fn.ReturnTypes[Idx], Error, OutAutoDef);
					if(Returns[Idx] == INVALID_TYPE)
						return INVALID_TYPE;
				}

				FnType->Function.Returns = SliceFromArray(Returns);
			}
			else
			{
				FnType->Function.Returns = {};
			}
			FnType->Function.Flags = TypeNode->Fn.Flags;
			FnType->Function.ArgCount = TypeNode->Fn.Args.Count;
			FnType->Function.Args = (u32 *)VAlloc(TypeNode->Fn.Args.Count * sizeof(u32));
			ForArray(Idx, TypeNode->Fn.Args)
			{
				FnType->Function.Args[Idx] = GetTypeFromTypeNode(Checker, TypeNode->Fn.Args[Idx]->Var.TypeNode, Error, OutAutoDef);
				if(FnType->Function.Args[Idx] == INVALID_TYPE)
					return INVALID_TYPE;
			}
			
			return AddType(FnType);
		} break;
		case AST_SELECTOR:
		{
			node *Operand = TypeNode->Selector.Operand;
			if(Operand == NULL)
			{
				if(!Error)
					return INVALID_TYPE;
				RaiseError(true, *TypeNode->ErrorInfo, "Cannot infer type for `.` selector");
			}
			if(Operand->Type != AST_ID)
			{
				u32 Type = GetTypeFromTypeNode(Checker, Operand, Error, OutAutoDef);
				if(Type != INVALID_TYPE)
				{
					if(GetType(Type)->Kind != TypeKind_Enum)
					{
						if(!Error)
							return INVALID_TYPE;
						RaiseError(false, *TypeNode->ErrorInfo, "Cannot use `.` selector on type %s", GetTypeName(Type));
					}

					return Type;
				}
				else if(!Error)
					return INVALID_TYPE;
				RaiseError(true, *Operand->ErrorInfo, "Expected module name in selector");
			}
			import Import;
			string SearchName = *Operand->ID.Name;
			if(!FindImportedModule(Checker->Imported, SearchName, &Import))
			{
				u32 Type = FindType(Checker, &SearchName, &Checker->Module->Name);
				if(Type == INVALID_TYPE)
					Type = FindTypeNoNamespaceImport(Checker, &SearchName); 

				if(Type != INVALID_TYPE)
				{
					if(GetType(Type)->Kind != TypeKind_Enum)
					{
						if(!Error)
							return INVALID_TYPE;
						RaiseError(false, *TypeNode->ErrorInfo, "Cannot use `.` selector on type %s", GetTypeName(Type));
					}

					return Type;
				}

				if(!Error)
					return INVALID_TYPE;
				RaiseError(true, *Operand->ErrorInfo, "Couldn't find module `%s`", Operand->ID.Name->Data);
			}
			u32 Type = FindType(Checker, TypeNode->Selector.Member, &Import.M->Name);
			if(Type == INVALID_TYPE)
			{
				if(!Error)
					return INVALID_TYPE;
				RaiseError(true, *TypeNode->ErrorInfo, "Type \"%s\" is not defined in module %s", TypeNode->Selector.Member->Data, TypeNode->Selector.Operand->ID.Name->Data);
			}
			return Type;
		} break;
		case AST_GENERIC:
		{
			// @HACK
			if(!Error)
				return INVALID_TYPE;

			if(!Checker->Scope.TryPeek() || (Checker->Scope.Peek()->ScopeNode->Type != AST_FN))
			{
				if(!Error)
					return INVALID_TYPE;
				RaiseError(false, *TypeNode->ErrorInfo, "Declaring generic type outside of function arguments is not allowed");
			}
			if(OutAutoDef)
				*OutAutoDef = true;
			return MakeGeneric(Checker->Scope.Peek(), *TypeNode->Generic.Name);
		} break;
		case AST_TYPEOF:
		{
			if(!Checker->OutOfRun.IsEmpty())
			{
				auto WasScope = Checker->Scope;
				Checker->Scope = Checker->OutOfRun;
				u32 T = AnalyzeExpression(Checker, TypeNode->TypeOf.Expression);
				TypeNode->TypeOf.Type = T;
				Checker->Scope = WasScope;
				return T;
			}
			else
			{
				u32 T = AnalyzeExpression(Checker, TypeNode->TypeOf.Expression);
				TypeNode->TypeOf.Type = T;
				return T;
			}
		} break;
		case AST_STRUCTDECL:
		{
			// @TODO: Cleanup
			type *New = AllocType(TypeKind_Struct);
			New->Struct.Name = *TypeNode->StructDecl.Name;
			u32 Result = AddType(New);
			AnalyzeStructDeclaration(Checker, TypeNode);
			return Result;
		} break;
		default:
		{
			if(Error)
			{
				RaiseError(true, *TypeNode->ErrorInfo, "Expected valid type!");
			}
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
				RaiseError(false, *LHS->ErrorInfo, "Undeclared identifier %s", LHS->ID.Name->Data);
				return false;
			}
			return (Sym->Flags & SymbolFlag_Const) == 0;
		} break;
		case AST_INDEX:
		{
			return IsLHSAssignable(Checker, LHS->Index.Operand);
		} break;
		case AST_UNARY:
		{
			if(LHS->Unary.Op != T_PTR && LHS->Unary.Op != T_QMARK)
				return false;
			return IsLHSAssignable(Checker, LHS->Unary.Operand);
		} break;
		case AST_SELECTOR:
		{
			if(LHS->Selector.Operand == NULL)
			{
				RaiseError(false, *LHS->ErrorInfo, "Cannot infer type for `.` selector");
				return false;
			}

			u32 TypeIdx = AnalyzeExpression(Checker, LHS->Selector.Operand);
			if(TypeIdx == Basic_module)
			{
				if(LHS->Selector.Operand->Type != AST_ID)
				{
					RaiseError(true, *LHS->Selector.Operand->ErrorInfo,
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
					RaiseError(false, *LHS->ErrorInfo,
							"Cannot find public symbol %s in module %s",
							LHS->Selector.Member->Data, ModuleName.Data);
					return false;
				}
				return false;
			}
			else
			{
				return IsLHSAssignable(Checker, LHS->Selector.Operand);
			}
		} break;
		case AST_TYPEINFO:
		{
			return false;
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
		if(FnNode->Fn.Args[Idx]->Var.TypeNode == NULL)
		{
			Flags |= SymbolFlag_VarFunc;
			if(Idx + 1 != FnNode->Fn.Args.Count)
			{
				RaiseError(true, *FnNode->Fn.Args[Idx]->ErrorInfo, "Variadic arguments needs to be last in function type, but it is #%d", Idx);
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

	bool NeedToAddGeneric = false;
	for(int I = 0; I < Function.ArgCount; ++I)
	{
		u32 T = GetTypeFromTypeNode(Checker, FnNode->Fn.Args[I]->Var.TypeNode, false);
		if(T == INVALID_TYPE)
		{
			NeedToAddGeneric = true;
			break;
		}
	}
	if(!NeedToAddGeneric)
	{
		For(FnNode->Fn.ReturnTypes)
		{
			u32 T = GetTypeFromTypeNode(Checker, *it, false);
			if(T == INVALID_TYPE)
			{
				NeedToAddGeneric = true;
				break;
			}
		}
	}

	for(int I = 0; I < Function.ArgCount; ++I)
	{
		b32 IsAutoDefine = false;
		Function.Args[I] = GetTypeFromTypeNode(Checker, FnNode->Fn.Args[I]->Var.TypeNode, true, &IsAutoDefine);
		FnNode->Fn.Args[I]->Var.IsAutoDefineGeneric = IsAutoDefine;
		const type *T = GetType(Function.Args[I]);
		if(T->Kind == TypeKind_Function)
			Function.Args[I] = GetPointerTo(Function.Args[I]);
		else if(HasBasicFlag(T, BasicFlag_TypeID))
		{
			if(NeedToAddGeneric)
				MakeGeneric(FnScope, *FnNode->Fn.Args[I]->Var.Name);
		}
		else if(IsGeneric(T))
		{
			FnNode->Fn.Flags |= SymbolFlag_Generic;
			Function.Flags |= SymbolFlag_Generic;
		}
	}

	if(FnNode->Fn.ReturnTypes.IsValid())
	{
		array<u32> Returns = array<u32>(FnNode->Fn.ReturnTypes.Count);
		ForArray(Idx, FnNode->Fn.ReturnTypes)
		{
			Returns[Idx] = GetTypeFromTypeNode(Checker, FnNode->Fn.ReturnTypes[Idx]);
		}

		Function.Returns = SliceFromArray(Returns);
	}
	else
	{
		Function.Returns = {};
	}

	if(Function.Returns.IsValid())
	{
		ForArray(Idx, Function.Returns)
		{
			if(IsGeneric(Function.Returns[Idx]))
			{
				FnNode->Fn.Flags |= SymbolFlag_Generic;
				Function.Flags |= SymbolFlag_Generic;
			}
		}
	}
	
	NewType->Function = Function;
	Checker->Scope.Pop();
	FnNode->Fn.Flags |= Function.Flags;
	return AddType(NewType);
}

void AnalyzeFunctionBody(checker *Checker, dynamic<node *> &Body, node *FnNode, u32 FunctionTypeIdx, node *ScopeNode)
{
	if(FnNode->Fn.AlreadyAnalyzed)
		return;

	slice<u32> Save = Checker->CurrentFnReturnTypeIdx;
	if(!ScopeNode)
		Checker->Scope.Push(AllocScope(FnNode, Checker->Scope.TryPeek()));
	else
		Checker->Scope.Push(AllocScope(ScopeNode, Checker->Scope.TryPeek()));

	const type *FunctionType = GetType(FunctionTypeIdx);
	Checker->CurrentFnReturnTypeIdx = FunctionType->Function.Returns;

	for(int I = 0; I < FunctionType->Function.ArgCount; ++I)
	{
		node *Arg = FnNode->Fn.Args[I];
		u32 flags = SymbolFlag_Const;
		const type *ArgT = GetType(FunctionType->Function.Args[I]);
		if(IsFnOrPtr(ArgT))
			flags |= SymbolFlag_Function;
		AddVariable(Checker, Arg->ErrorInfo, FunctionType->Function.Args[I], Arg->Var.Name, Arg, flags);
		//Arg->Decl.Flags = flags;
	}
	if(FunctionType->Function.Flags & SymbolFlag_VarFunc && !IsForeign(FunctionType))
	{
		int I = FunctionType->Function.ArgCount;
		node *Arg = FnNode->Fn.Args[I];
		u32 flags = SymbolFlag_Const;
		u32 ArgType = FindStruct(STR_LIT("base.Arg"));
		u32 Type = GetSliceType(ArgType);
		AddVariable(Checker, Arg->ErrorInfo, Type, Arg->Var.Name, NULL, flags);

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
		if(FunctionType->Function.Returns.Count != 0)
		{
			RaiseError(false, *Body[Body.Count-1]->ErrorInfo, "Missing a return statement in function that returns a type");
		}
		Body.Push(MakeReturn(Body[Body.Count-1]->ErrorInfo, NULL));
	}

	Checker->Scope.Pop();
	Checker->CurrentFnReturnTypeIdx = Save;
}

int FindVectorAccessor(string Name, const type *T, const error_info *ErrorInfo)
{
	string Names[] = {
		STR_LIT("x"),
		STR_LIT("y"),
		STR_LIT("z"),
		STR_LIT("w"),
	};

	int Found = -1;
	auto len = ARR_LEN(Names);
	for(int i = 0; i < len; ++i)
	{
		if (Names[i] == Name)
		{
			Found = i;
			break;
		}
	}

	if(Found == -1 || Found >= T->Vector.ElementCount)
	{
		RaiseError(false, *ErrorInfo, "%s is not a member of %s", Name.Data, GetTypeName(T));
		Found = -1;
	}
	return Found;
}

b32 IsConstant(checker *Checker, node *Expr)
{
	if(Expr->Type == AST_ID)
	{
		if(Checker->Module->Globals[*Expr->ID.Name]) return true;

		string NoNamespace = STR_LIT("*");
		For(Checker->Imported)
		{
			if(it->As == NoNamespace)
			{
				if(it->M->Globals[*Expr->ID.Name]) return true;
			}
		}
		return false;

	}

	if(Expr->Type == AST_RESERVED)
		return true;
	if(Expr->Type == AST_CONSTANT || Expr->Type == AST_CHARLIT)
		return true;
	if(Expr->Type == AST_EMBED)
		return true;
	if(Expr->Type == AST_RUN)
		return true;
	if(Expr->Type == AST_SIZE || Expr->Type == AST_TYPEOF)
		return true;

	if(Expr->Type == AST_SELECTOR)
	{
		if(Expr->Selector.Operand == NULL)
			return true;

		u32 T = AnalyzeExpression(Checker, Expr->Selector.Operand);
		if(T == Basic_type)
			return true;
		return IsConstant(Checker, Expr->Selector.Operand);
	}

	if(Expr->Type == AST_BINARY)
	{
		return IsConstant(Checker, Expr->Binary.Left) && IsConstant(Checker, Expr->Binary.Right);
	}

	if(Expr->Type == AST_CAST)
		return IsConstant(Checker, Expr->Cast.Expression);
	if(Expr->Type == AST_UNARY)
		return IsConstant(Checker, Expr->Unary.Operand);
	if(Expr->Type == AST_INDEX)
		return IsConstant(Checker, Expr->Index.Operand) && IsConstant(Checker, Expr->Index.Expression);
	if(Expr->Type == AST_TYPELIST)
	{
		For(Expr->TypeList.Items)
		{
			Assert((*it)->Type == AST_LISTITEM);
			if(!IsConstant(Checker, (*it)->Item.Expression))
				return false;
		}
		return true;
	}

	return false;
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

				string NoNamespace = STR_LIT("*");
				if(Result == INVALID_TYPE)
				{
					For(Checker->Imported)
					{
						if(it->As == NoNamespace)
						{
							// @THREADING: NOT THREAD SAFE (maybe)
							auto Sym = it->M->Globals[*Expr->ID.Name];
							if(Sym)
							{
								Result = Sym->Type;
								break;
							}
						}
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
					else
					{
						For(Checker->Imported)
						{
							if(it->As == NoNamespace)
							{
								u32 Got = FindType(Checker, Expr->ID.Name, &it->M->Name);
								if(Got != INVALID_TYPE)
								{
									Result = Basic_type;
									Expr->ID.Type = Got;
									break;
								}
							}
						}
					}
				}
				if(Result == INVALID_TYPE)
				{
					RaiseError(false, *Expr->ErrorInfo, "Refrenced variable %s is not declared", Expr->ID.Name->Data);
					return Basic_int;
				}
			}
			const type *Type = GetType(Result);
			if(Type->Kind == TypeKind_Function)
				Result = GetPointerTo(Result);
		} break;
		case AST_IFX:
		{
			AnalyzeBooleanExpression(Checker, &Expr->IfX.Expr);
			u32 IfTrue = AnalyzeExpression(Checker, Expr->IfX.True);
			u32 IfFalse = AnalyzeExpression(Checker, Expr->IfX.False);
			Result = TypeCheckAndPromote(Checker, Expr->IfX.True->ErrorInfo, IfTrue, IfFalse, &Expr->IfX.True, &Expr->IfX.False, "if expression left and right side are incompatible. Left: %s Right: %s.");
			Expr->IfX.TypeIdx = Result;
			if(IsUntyped(Result))
				Checker->UntypedStack.Push(&Expr->IfX.TypeIdx);
		} break;
		case AST_RUN:
		{
			if(Expr->Run.Body.Count != 1)
			{
				RaiseError(true, *Expr->ErrorInfo, "#run in expression expected a single expression as argument");
			}
			string WasBonus = BonusErrorMessage;
			SetBonusMessage(STR_LIT("While evaluating compile time #run. Keep in mind that local variables and function arguments are not available."));

			auto CurrentScope = Checker->Scope;
			auto WasOutOfRun = Checker->OutOfRun;

			Checker->Scope = {};
			Checker->Scope.Push(AllocScope(Expr));

			if(Checker->OutOfRun.IsEmpty())
				Checker->OutOfRun = CurrentScope;

			Result = AnalyzeExpression(Checker, Expr->Run.Body[0]);

			Checker->Scope = CurrentScope;
			Checker->OutOfRun = WasOutOfRun;

			Expr->Run.TypeIdx = Result;
			Expr->Run.IsExprRun = true;

			SetBonusMessage(WasBonus);
#if 0
			if(Result != INVALID_TYPE)
			{
				const type *T = GetType(Result);
				if(T->Kind != TypeKind_Basic && T->Kind != TypeKind_Pointer)
				{
					RaiseError(false, *Expr->ErrorInfo, "#run cannot give a value of type %s", GetTypeName(T));
				}
			}
#endif
		} break;
		case AST_LIST:
		{
			array<u32> Ts(Expr->List.Nodes.Count);
			uint At = 0;
			For(Expr->List.Nodes)
			{
				Ts[At] = AnalyzeExpression(Checker, *it);
				const type *T = GetType(Ts[At]);
				if(IsUntyped(T))
				{
					RaiseError(false, *Expr->ErrorInfo, "Untyped expressions are not allowed in `,` lists. "
							"You can cast the untyped values.");
				}
				At++;
			}
			Expr->List.Types = SliceFromArray(Ts);
			Expr->List.WholeType = ReturnsToType(SliceFromArray(Ts));
			Result = Expr->List.WholeType;
		} break;
		case AST_TYPEINFO:
		{
#if 0
			if(CompileFlags & CF_Standalone)
			{
				RaiseError(true, *Expr->ErrorInfo, "Cannot use type_info in a standalone build");
			}
#endif

			u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->TypeInfoLookup.Expression);
			const type *ExprType = GetType(ExprTypeIdx);
			if(!HasBasicFlag(ExprType, BasicFlag_TypeID))
			{
				RaiseError(false, *Expr->ErrorInfo, "type_info expected an expression with a type of `type`, got: %s",
						GetTypeName(ExprType));
			}
			u32 TypeInfoType = FindStruct(STR_LIT("base.TypeInfo"));
			Expr->TypeInfoLookup.Type = TypeInfoType;
			Result = GetPointerTo(TypeInfoType);
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

			if(!IsTypeMatchable(ExprType))
			{
				RaiseError(true, *Expr->ErrorInfo, "Type %s cannot be used for a match expression", GetTypeName(ExprType));
			}
			if(Expr->Match.Cases.Count == 0)
			{
				RaiseError(false, *Expr->ErrorInfo, "`match` expression has no cases");
			}

			Checker->AutoEnum.Push(ExprTypeIdx);

			b32 FoundDefault = false;
			ForArray(Idx, Expr->Match.Cases)
			{
				node *Case = Expr->Match.Cases[Idx];
				if(Case->Case.Value->Type == AST_ID && *Case->Case.Value->ID.Name == STR_LIT("_"))
				{
					if(FoundDefault)
					{
						RaiseError(false, *Case->ErrorInfo, "Multiple default cases in match");
					}
					FoundDefault = true;
					// default:
					Case->Case.Value = NULL;
				}
				else
				{
					if(!IsConstant(Checker, Case->Case.Value))
					{
						RaiseError(true, *Case->Case.Value->ErrorInfo, "Case value must be constant");
					}

					u32 CaseTypeIdx = AnalyzeExpression(Checker, Case->Case.Value);
					TypeCheckAndPromote(Checker, Case->ErrorInfo, ExprTypeIdx, CaseTypeIdx, NULL, &Case->Case.Value, "Cannot match expression of type %s with case of type %s");
				}
			}

			Checker->AutoEnum.Pop();

			Result = INVALID_TYPE;

			node **WasYieldExpr = Checker->YieldExpr;
			u32 WasYieldT = Checker->YieldT;

			Checker->Scope.Push(AllocScope(Expr, Checker->Scope.TryPeek()));
			ForArray(Idx, Expr->Match.Cases)
			{
				Checker->YieldT = INVALID_TYPE;
				node *Case = Expr->Match.Cases[Idx];
				if(!Case->Case.Body.IsValid())
				{
					RaiseError(false, *Case->ErrorInfo, "Missing body for case in match statement");
					continue;
				}

				Checker->Scope.Push(AllocScope(Case, Checker->Scope.TryPeek()));
				for(int BodyIdx = 0; BodyIdx < Case->Case.Body.Count; ++BodyIdx)
				{
					AnalyzeNode(Checker, Case->Case.Body[BodyIdx]);
				}
				if(Idx == 0)
				{
					Result = Checker->YieldT;
				}
				else
				{
					if(Result == INVALID_TYPE && Checker->YieldT != INVALID_TYPE)
					{
						RaiseError(false, *Case->ErrorInfo, "Trying to yield a value in case when previous case doesn't");
					}
					else if(Checker->YieldT == INVALID_TYPE && Result != INVALID_TYPE)
					{
						RaiseError(false, *Case->ErrorInfo, "Not yielding a value in case when previous case yields value of type %s", GetTypeName(Result));
					}
					else if(Checker->YieldT != INVALID_TYPE && Result != INVALID_TYPE)
					{
						Result = TypeCheckAndPromote(Checker, Case->ErrorInfo, Result, Checker->YieldT, NULL, Checker->YieldExpr, "match expected a yield of type %s based on previous cases, but got type %s");
					}
				}
				CheckBodyForUnreachableCode(Case->Case.Body);
				Checker->Scope.Pop();
			}
			Checker->Scope.Pop();

			Checker->YieldT = WasYieldT;
			Checker->YieldExpr = WasYieldExpr;
			Expr->Match.MatchType = ExprTypeIdx;
			Expr->Match.ReturnType = Result;

			if(IsUntyped(Expr->Match.ReturnType))
			{
				Checker->UntypedStack.Push(&Expr->Match.ReturnType);
			}
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
				case rs::Inf:
				case rs::NaN:
				{
					Result = Basic_UntypedFloat;
					Checker->UntypedStack.Push(&Expr->Reserved.Type);
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
				RaiseError(true, *Expr->ErrorInfo, "Trying to call a non function type \"%s\"", GetTypeName(CallType));
			}
			if(CallType->Kind == TypeKind_Pointer)
				CallType = GetType(CallType->Pointer.Pointed);
			Assert(CallType->Kind == TypeKind_Function);

			if(CallType->Function.Flags & SymbolFlag_Intrinsic)
			{
				if(Expr->Call.Fn->Type != AST_ID)
				{
					RaiseError(false, *Expr->Call.Fn->ErrorInfo, "Indirect call to intrinsic is not allowed");
				}
				else if(!CheckIntrinsic(*Expr->Call.Fn->ID.Name))
				{
					RaiseError(false, *Expr->Call.Fn->ErrorInfo, "Unknown intrinsic %s. Are you taking a function pointer to an intrinsic? That is not allowed.", Expr->Call.Fn->ID.Name->Data);
				}
				else
				{
					Expr->Call.SymName = *Expr->Call.Fn->ID.Name;
				}
			}

			if(Expr->Call.Args.Count != CallType->Function.ArgCount)
			{
				if(CallType->Function.Flags & SymbolFlag_VarFunc && Expr->Call.Args.Count > CallType->Function.ArgCount)
				{}
				else
				{
					RaiseError(false, *Expr->ErrorInfo, "Incorrect number of arguments, needed %d got %d",
							CallType->Function.ArgCount, Expr->Call.Args.Count);
					Result = GetReturnType(CallType);
					break;
				}
			}

			{
				dynamic<u32> ArgTypes = {};
				ForArray(Idx, Expr->Call.Args)
				{
					if(CallType->Function.ArgCount > Idx)
					{
						Checker->AutoEnum.Push(CallType->Function.Args[Idx]);
					}
					u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->Call.Args[Idx]);
					const type *ExprType = GetType(ExprTypeIdx);

					if(CallType->Function.ArgCount > Idx)
					{
						Checker->AutoEnum.Pop();
					}

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
						RaiseError(false, *Expr->Call.Args[Idx]->ErrorInfo, "Argument #%d is of incompatible type %s, tried to pass: %s",
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
				symbol *s = GenerateFunctionFromPolymorphicCall(Checker, Expr);
				const string *FnName = s->Name;

				if(Checker->Module->Name != s->Checker->Module->Name)
				{
					string Name = *FnName;
					FnName = StructToModuleNamePtr(Name, s->Checker->Module->Name);
				}

				Expr->Call.Fn = MakeID(Expr->ErrorInfo, FnName);
				Expr->Call.Type = s->Type;
				CallType = GetType(Expr->Call.Type);
			}

			Result = GetReturnType(CallType);
		} break;
		case AST_EMBED:
		{
			string File = Checker->File;
			auto b = MakeBuilder();
			int end = File.Size-1;
			for(; end >= 0
					&& File.Data[end] != '/'
					&& File.Data[end] != '\\'
					; --end);
			string Tmp = {File.Data, (size_t)end+1};

			b += Tmp;
			b += *Expr->Embed.FileName;
			string FileName = MakeString(b);
			string Read = ReadEntireFile(FileName);
			if(Read.Data == NULL)
				RaiseError(false, *Expr->ErrorInfo, "Couldn't open #embed_%s file %s", Expr->Embed.IsString ? "str" : "bin", FileName.Data);

			Expr->Embed.Content = Read;
			Result = Expr->Embed.IsString ? Basic_string : GetPointerTo(Basic_u8);
		} break;
		case AST_TYPEOF:
		{
			if(!Checker->OutOfRun.IsEmpty())
			{
				auto WasScope = Checker->Scope;
				Checker->Scope = Checker->OutOfRun;

				u32 T = AnalyzeExpression(Checker, Expr->TypeOf.Expression);
				Expr->TypeOf.Type = T;
				Result = Basic_type;

				Checker->Scope = WasScope;
			}
			else
			{
				u32 ExprType = AnalyzeExpression(Checker, Expr->Size.Expression);
				Expr->TypeOf.Type = ExprType;
				Result = Basic_type;
			}
		} break;
		case AST_SIZE:
		{
			u32 ExprType;

			if(!Checker->OutOfRun.IsEmpty())
			{
				auto WasScope = Checker->Scope;
				Checker->Scope = Checker->OutOfRun;

				ExprType = AnalyzeExpression(Checker, Expr->Size.Expression);

				Checker->Scope = WasScope;
			}
			else
			{
				ExprType = AnalyzeExpression(Checker, Expr->Size.Expression);
			}

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
			b32 Failed = false;

			u32 From = AnalyzeExpression(Checker, Expr->Cast.Expression);
			Assert(To != INVALID_TYPE && From != INVALID_TYPE);
			const type *ToType = GetType(To);
			const type *FromType = GetType(From);
			if(!IsCastValid(FromType, ToType))
			{
				RaiseError(false, *Expr->ErrorInfo, "Cannot cast %s to %s", GetTypeName(FromType), GetTypeName(ToType));
				Failed = true;
			}

			if(!Failed && IsCastRedundant(FromType, ToType))
			{
				*Expr = *Expr->Cast.Expression;
				Result = From;
			}
			else
			{
				Expr->Cast.FromType = From;
				Expr->Cast.ToType = To;
				Result = To;

				if(!Failed && IsUntyped(FromType))
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
			bool Failed = false;
			u64 MaxElements = 0;
			switch(Type->Kind)
			{
				case TypeKind_Array: 
				MaxElements = Type->Array.MemberCount;
				break;
				case TypeKind_Slice:
				MaxElements = UINT64_MAX;
				break;
				case TypeKind_Struct:
				MaxElements = Type->Struct.Members.Count;
				break;
				case TypeKind_Vector:
				MaxElements = Type->Vector.ElementCount;
				break;

				default:
				{
					if(!IsString(Type))
					{
						RaiseError(false, *Expr->ErrorInfo, "Cannot create a list of type %s, not a struct, array, slice or string", GetTypeName(Type));
						Result = TypeIdx;
						Failed = true;
					}
					else
					{
						MaxElements = 2;
					}
				} break;
			}

			if(Failed)
				break;

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
						RaiseError(false, *Item->ErrorInfo, "Name parameter in a list with an unnamed parameter, mixing is not allowed");
						Failed = true;
						continue;
					}
					NamedStatus = NS_NAMED;
				}
				else
				{
					if(NamedStatus == NS_NAMED)
					{
						RaiseError(false, *Item->ErrorInfo, "Unnamed parameter in a list with a named parameter, mixing is not allowed");
						Failed = true;
						continue;
					}
					NamedStatus = NS_NOT_NAMED;

				}
			}

			if(Failed)
			{
				Result = TypeIdx;
				break;
			}

			if(NamedStatus == NS_NAMED && (Type->Kind == TypeKind_Array || Type->Kind == TypeKind_Slice))
			{
				RaiseError(false, *Expr->ErrorInfo, "Still haven't implemented named array lists");
				Failed = true;
			}

			if(Expr->TypeList.Items.Count == 0 && Type->Kind == TypeKind_Struct &&
					(Type->Struct.Flags & StructFlag_Generic))
			{
				RaiseError(false, *Expr->ErrorInfo, "Cannot 0 initialize generic struct %s", GetTypeName(Type));
				Failed = true;
			}

			if(Failed)
			{
				Result = TypeIdx;
				break;
			}

			node *Filled[4096] = {};
			u32 ExprTypes[4096] = {};
			ForArray(Idx, Expr->TypeList.Items)
			{
				node *Item = Expr->TypeList.Items[Idx];
				const string *NamePtr = Item->Item.Name;
				Assert(Item->Type == AST_LISTITEM);
				u32 WantType = INVALID_TYPE;
				int MemberIdx = Idx;
				switch(Type->Kind)
				{
					case TypeKind_Array:
					{
						WantType = Type->Array.Type;
					} break;
					case TypeKind_Slice:
					{
						WantType = Type->Slice.Type;
					} break;
					case TypeKind_Vector:
					{
						if(NamePtr)
						{
							int Found = FindVectorAccessor(*NamePtr, Type, Item->ErrorInfo);
							if(Found == -1)
							{
								Failed = true;
								continue;
							}
							MemberIdx = Found;
						}
						WantType = GetVecElemType(Type);
						switch(Type->Vector.Kind)
						{
							case Vector_Float: WantType = Basic_f32; break;
							case Vector_Int: WantType = Basic_i32; break;
						}
					} break;
					case TypeKind_Struct:
					{
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
								RaiseError(false, *Item->ErrorInfo, "No member named %s in struct %s",
										Name.Data, GetTypeName(Type));
								Failed = true;
								continue;
							}
							MemberIdx = Found;
						}
						WantType = Type->Struct.Members[MemberIdx].Type;
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
								MemberIdx = 1;
							}
							else if(Name == STR_LIT("count"))
							{
								MemberIdx = 0;
							}
							else
							{
								RaiseError(false, *Item->ErrorInfo, "No member named %s in string",
										Name.Data);
								Failed = true;
								continue;
							}
						}
						u32 Type = INVALID_TYPE;
						if(MemberIdx == 1)
							Type = GetPointerTo(Basic_u8);
						else
							Type = Basic_int;

						WantType = Type;
					} break;
					default: unreachable;
				}

				if(MemberIdx >= MaxElements)
				{
					RaiseError(false, *Item->ErrorInfo, "item at index %d but type only accept %d elements", MemberIdx, MaxElements);
					Failed = true;
					break;
				}

				Checker->AutoEnum.Push(WantType);
				u32 ItemType = AnalyzeExpression(Checker, Item->Item.Expression);
				Checker->AutoEnum.Pop();

				u32 PromotedUntyped = INVALID_TYPE;
				switch(Type->Kind)
				{
					case TypeKind_Vector:
					case TypeKind_Slice:
					case TypeKind_Array: 
					{
						Filled[Idx] = Item;
						PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, WantType, ItemType, NULL, &Item->Item.Expression, "Type list expected items of type %s but got incompatible type %s");
					} break;
					case TypeKind_Struct:
					{
						Filled[MemberIdx] = Item;
						ExprTypes[MemberIdx] = ItemType;
						PromotedUntyped = INVALID_TYPE;
						if(!IsGeneric(WantType))
							PromotedUntyped = TypeCheckAndPromote(Checker, Item->ErrorInfo, WantType, ItemType, NULL, &Item->Item.Expression, "Struct member in type list is of type %s but the expression is of type %s");
					} break;
					case TypeKind_Basic:
					{
						Assert(HasBasicFlag(Type, BasicFlag_String));

						Filled[MemberIdx] = Item;
						ExprTypes[MemberIdx] = ItemType;
						PromotedUntyped = TypeCheckAndPromote(Checker, Expr->ErrorInfo, WantType, ItemType, NULL, &Item->Item.Expression, "string type list expected type %s but got %s");
					} break;
					default: unreachable;
				}

				// @Note: Is there a point to this check?
				if(PromotedUntyped != INVALID_TYPE)
					FillUntypedStack(Checker, PromotedUntyped);
			}

			if(Failed)
			{
				Result = TypeIdx;
				break;
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
						if(Filled[Idx] == NULL)
						{
							RaiseError(false, *Expr->ErrorInfo,
									"Type field needs to be specified in initialization of struct %s",
									GetTypeName(Type));
							Failed = true;
						}
						else
						{
							node *Expr = Filled[Idx]->Item.Expression;
							GenericResolved = GetTypeFromTypeNode(Checker, Expr);
							FillUntypedStack(Checker, GenericResolved);
						}
					}
					Members.Push(NewMember);
				}

				if(Failed)
				{
					Result = TypeIdx;
					break;
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
								&Filled[Idx]->Item.Expression, "Generic struct member should be type %s but got type %s");
						Members.Data[Idx].Type = NonGeneric;
					}
				}

				TypeIdx = MakeStruct(SliceFromArray(Members), Type->Struct.Name, Type->Struct.Flags & (~StructFlag_Generic));
			}
			else if(Type->Kind == TypeKind_Struct)
			{
				// Check for 0 initializing non nullable pointer
				ForArray(Idx, Type->Struct.Members)
				{
					struct_member Member = Type->Struct.Members[Idx];
					const type *MT = GetType(Member.Type);
					if(MT->Kind == TypeKind_Pointer && (MT->Pointer.Flags & PointerFlag_Optional) == 0 &&
							Filled[Idx] == NULL)
					{
						RaiseError(false, *Expr->ErrorInfo, "Trying to 0 initialize struct member %s which is a non nullable pointer, to use a struct literal of type %s, specify the member with a valid address.", Member.ID.Data, GetTypeName(Type));
					}
				}
			}
			Expr->TypeList.Type = TypeIdx;
			Result = TypeIdx;
		} break;
		case AST_SELECTOR:
		{
			u32 TypeIdx = INVALID_TYPE;
			if(Expr->Selector.Operand)
			{
				TypeIdx = AnalyzeExpression(Checker, Expr->Selector.Operand);
			}
			else
			{
				u32 EnumT = INVALID_TYPE;
				for(ssize_t i = ((ssize_t)Checker->AutoEnum.Data.Count)-1; i >= 0; --i)
				{
					const type *T = GetType(Checker->AutoEnum.Data[i]);
					if(T->Kind == TypeKind_Enum)
					{
						EnumT = Checker->AutoEnum.Data[i];
						break;
					}
				}

				if(EnumT == INVALID_TYPE)
				{
					RaiseError(true, *Expr->ErrorInfo, "Cannot infer enum type from this expression");
				}

				TypeIdx = EnumT;
			}

			const type *Type = NULL;
			Expr->Selector.SubIndex = -1;
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
						RaiseError(false, *Expr->ErrorInfo,
								"Cannot find public symbol %s in module %s",
								Expr->Selector.Member->Data, Expr->Selector.Operand->ID.Name->Data);
					}
					Result = Basic_type;
					Expr->Selector.Type = Basic_type;
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
					case TypeKind_Vector:
					{
						int Found = FindVectorAccessor(*Expr->Selector.Member, Type, Expr->ErrorInfo);
						if(Found == -1)
						{
							Result = GetVecElemType(Type);
							break;
						}
						Expr->Selector.Index = Found;
						Result = GetVecElemType(Type);
					} break;
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
								RaiseError(true, *Expr->ErrorInfo, "Invalid `.`! Cannot use selector on a typeid");
							}

							const type *T = GetType(TIdx);
							if(T->Kind != TypeKind_Enum)
							{
								RaiseError(false, *Expr->ErrorInfo, "Invalid `.`! Cannot use selector on a direct type %s", GetTypeName(T));
								Result = TIdx;
								break;
							}

							Result = INVALID_TYPE;
							ForArray(Idx, T->Enum.Members)
							{
								if(T->Enum.Members[Idx].Name == *Expr->Selector.Member)
								{
									//Expr->Selector.Operand = NULL;
									Expr->Selector.Index = Idx;
									Expr->Selector.Type = TIdx;
									Result = TIdx;
									break;
								}
							}
							if(Result == INVALID_TYPE)
							{
								RaiseError(false, *Expr->ErrorInfo, "No member named %s in enum %s; Invalid `.` selector",
										Expr->Selector.Member->Data, GetTypeName(T));
								Result = TIdx;
							}
						}
						else if(IsString(Type))
						{
							if(*Expr->Selector.Member == STR_LIT("count"))
							{
								Expr->Selector.Index = 0;
								Result = Basic_int;
							}
							else if(*Expr->Selector.Member == STR_LIT("data"))
							{
								Expr->Selector.Index = 1;
								Result = GetPointerTo(Basic_u8);
							}
							else
							{
								RaiseError(true, *Expr->ErrorInfo, "Only .data and .count can be accessed on a string");
							}
						}
						else
						{
							RaiseError(false, *Expr->ErrorInfo, "Cannot use `.` selector operator on %s", GetTypeName(Type));
							Result = TypeIdx;;
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
							RaiseError(true, *Expr->ErrorInfo, "Only .count and .data can be accessed on this type");
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
							RaiseError(false, *Expr->ErrorInfo, "No member named %s in enum %s; Invalid `.` selector",
									Expr->Selector.Member->Data, GetTypeName(Type));
							Result = TypeIdx;
						}
					} break;
					case TypeKind_Pointer:
					{

						const type *Pointed = NULL;
						if(Type->Pointer.Pointed != INVALID_TYPE)
							Pointed = GetType(Type->Pointer.Pointed);
						if(!Pointed || (Pointed->Kind != TypeKind_Struct && Pointed->Kind != TypeKind_Slice))
						{
							RaiseError(true, *Expr->ErrorInfo, "Cannot use `.` selector operator on a pointer that doesn't directly point to a struct. %s",
									GetTypeName(Type));
						}
						if(Type->Pointer.Flags & PointerFlag_Optional)
						{
							RaiseError(false, *Expr->ErrorInfo, "Cannot derefrence optional pointer, check for null and then mark it non optional with the ? operator");
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
							if(Type->Struct.SubType != INVALID_TYPE)
							{
								const type *Sub = GetType(Type->Struct.SubType);
								ForArray(Idx, Sub->Struct.Members)
								{
									if(Sub->Struct.Members[Idx].ID == *Expr->Selector.Member)
									{
										Expr->Selector.Index = 0;
										Expr->Selector.SubIndex = Idx;
										Result = Sub->Struct.Members[Idx].Type;
										break;
									}
								}
							}

							if(Result == INVALID_TYPE)
							{
								RaiseError(true, *Expr->ErrorInfo,
										"No member named %s in struct %s; Invalid `.` selector",
										Expr->Selector.Member->Data, GetTypeName(Type));
							}
						}
					} break;
					default:
					{
						RaiseError(false, *Expr->ErrorInfo, "Cannot use `.` selector operator on %s",
								GetTypeName(Type));
						Result = TypeIdx;
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
						RaiseError(false, *Expr->ErrorInfo, "Cannot index optional pointer. Check if it's null and then use the ? operator");
					}
					if(OperandType->Pointer.Pointed == INVALID_TYPE)
					{
						RaiseError(true, *Expr->ErrorInfo, "Cannot index opaque pointer");
					}
					const type *Pointed = GetType(OperandType->Pointer.Pointed);
					if(Pointed->Kind == TypeKind_Function)
					{
						RaiseError(true, *Expr->ErrorInfo, "Cannot index function pointer");
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
					RaiseError(false, *Expr->ErrorInfo, "Cannot index type %s", GetTypeName(OperandType));
					Result = OperandTypeIdx;
				} break;
				default:
				{
					RaiseError(false, *Expr->ErrorInfo, "Cannot index type %s", GetTypeName(OperandType));
					Result = OperandTypeIdx;
				} break;
			}
			FillUntypedStack(Checker, Result);

			u32 ExprTypeIdx = AnalyzeExpression(Checker, Expr->Index.Expression);
			const type *ExprType = GetType(ExprTypeIdx);
			if(ExprType->Kind != TypeKind_Basic || (ExprType->Basic.Flags & BasicFlag_Integer) == 0)
			{
				RaiseError(false, *Expr->ErrorInfo, "Indexing expression needs to be of an integer type");
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
			if(IsGeneric(Expr->Fn.TypeIdx))
				RaiseError(true, *Expr->ErrorInfo, "Lambdas currently cannot be generic");
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
			u32 Result = INVALID_TYPE;
			switch(Expr->Unary.Op)
			{
				case T_MINUS:
				{
					u32 TypeIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *T = GetType(TypeIdx);
					if(!HasBasicFlag(T, BasicFlag_Integer) && !HasBasicFlag(T, BasicFlag_Float))
					{
						RaiseError(false, *Expr->ErrorInfo, "Unary `-` can only be used on integers and floats, but here it is used on %s",
								GetTypeName(T));
					}
					if(HasBasicFlag(T, BasicFlag_Unsigned))
					{
						RaiseError(false, *Expr->ErrorInfo, "Cannot use a unary `-` on an unsigned type %s", GetTypeName(T));
					}
					Expr->Unary.Type = TypeIdx;

					Result = TypeIdx;
				} break;
				case T_BANG:
				{
					AnalyzeBooleanExpression(Checker, &Expr->Unary.Operand);
					Expr->Unary.Type = Basic_bool;
					Result = Basic_bool;
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
							RaiseError(false, *Expr->ErrorInfo, "Cannot declare optional non pointer type: %s", GetTypeName(Opt));
							return Basic_type;
						}
						Assert(Expr->Unary.Operand->Type == AST_PTRTYPE);
						Expr->Unary.Operand->PointerType.Analyzed = GetOptional(GetType(Expr->Unary.Operand->PointerType.Analyzed));
						Expr->Unary.Operand->PointerType.Flags |= PointerFlag_Optional;
						*Expr = *Expr->Unary.Operand;
						// @Note: Early return
						return Basic_type;
					}
					if(Pointer->Kind != TypeKind_Pointer)
					{
						RaiseError(false, *Expr->ErrorInfo, "Cannot use ? operator on non pointer type %s", GetTypeName(Pointer));
						Result = PointerIdx;
					}
					if((Pointer->Pointer.Flags & PointerFlag_Optional) == 0)
					{
						RaiseError(false, *Expr->ErrorInfo, "Pointer is not optional, remove the ? operator");
					}
					if(Result == INVALID_TYPE)
						Result = GetNonOptional(Pointer);
				} break;
				case T_PTR:
				{
					u32 PointerIdx = AnalyzeExpression(Checker, Expr->Unary.Operand);
					const type *Pointer = GetType(PointerIdx);
					if(PointerIdx == Basic_type)
					{
						*Expr = *MakePointerType(Expr->ErrorInfo, Expr->Unary.Operand);
						Expr->PointerType.Analyzed = GetTypeFromTypeNode(Checker, Expr);
						// @Note: Early return
						return Basic_type;
					}

					if(Pointer->Kind != TypeKind_Pointer)
					{
						RaiseError(false, *Expr->ErrorInfo, "Cannot derefrence operand. It's not a pointer");
						Result = PointerIdx;
					}
					if(Pointer->Pointer.Flags & PointerFlag_Optional)
					{
						RaiseError(false, *Expr->ErrorInfo, "Cannot derefrence optional pointer, check for null and then mark it non optional with the ? operator");
					}
					if(Pointer->Pointer.Pointed == INVALID_TYPE)
					{
						RaiseError(true, *Expr->ErrorInfo, "Cannot derefrence opaque pointer");
					}
					Expr->Unary.Type = Pointer->Pointer.Pointed;
					Result = Pointer->Pointer.Pointed;
				} break;
				case T_ADDROF:
				{
					u32 Pointed = AnalyzeExpression(Checker, Expr->Unary.Operand);
					if(!IsLHSAssignable(Checker, Expr->Unary.Operand))
					{
						RaiseError(false, *Expr->ErrorInfo, "Cannot take address of operand");
					}
					Expr->Unary.Type = GetPointerTo(Pointed);
					Result = Expr->Unary.Type;
				} break;
				default: unreachable;
			}
			if(IsUntyped(Expr->Unary.Type))
				Checker->UntypedStack.Push(&Expr->Unary.Type);
			return Result;
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

u32 TypeCheckAndPromote(checker *Checker, const error_info *ErrorInfo, u32 Left, u32 Right, node **LeftNode, node **RightNode, const char *ErrorFmt)
{
	u32 Result = Left;

	b32 IsAssignment = LeftNode == NULL;
	const type *LeftType  = GetType(Left);
	const type *RightType = GetType(Right);
	const type *Promotion = NULL;
	if(!IsTypeCompatible(LeftType, RightType, &Promotion, IsAssignment))
	{
TYPE_ERR:
		RaiseError(false, *ErrorInfo, ErrorFmt,
				GetTypeName(LeftType), GetTypeName(RightType));
		return Left;
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
		u32 Left  = INVALID_TYPE;
		u32 Right = INVALID_TYPE;
		if(Expr->Binary.Op == T_LAND || Expr->Binary.Op == T_LOR)
		{
			Left = AnalyzeBooleanExpression(Checker, &Expr->Binary.Left);
			Right = AnalyzeBooleanExpression(Checker, &Expr->Binary.Right);
			Left = Basic_bool;
			Right = Basic_bool;
			Expr->Binary.ExpressionType = Basic_bool;
		}
		else
		{
			Left  = AnalyzeExpression(Checker, Expr->Binary.Left);

			Checker->AutoEnum.Push(Left);
			Right = AnalyzeExpression(Checker, Expr->Binary.Right);
			Checker->AutoEnum.Pop();

			Expr->Binary.ExpressionType = Left;
		}

		const type *LeftType = GetType(Left);
		const type *RightType = GetType(Right);

		if(!CanTypePerformBinExpression(LeftType, Expr->Binary.Op))
		{
			RaiseError(false, *Expr->ErrorInfo, "Cannot perform a binary %s with %s",
					GetTokenName(Expr->Binary.Op), GetTypeName(LeftType));
			return Left;
		}

		if(!CanTypePerformBinExpression(RightType, Expr->Binary.Op))
		{
			RaiseError(false, *Expr->ErrorInfo, "Cannot perform a binary %s with %s",
					GetTokenName(Expr->Binary.Op), GetTypeName(RightType));
			return Left;
		}

		// @TODO: Check how type checking and casting here works with +=, -=, etc... substitution
		u32 Promoted;
		const type *X = OneIsXAndTheOtherY(LeftType, RightType, TypeKind_Pointer, TypeKind_Basic);
		if(X)
		{
			token_type T = Expr->Binary.Op;
			if(T != '+' && T != '-' && T != T_PEQ && T != T_MEQ)
			{
				RaiseError(true, *Expr->ErrorInfo, "Invalid operator between pointer and basic type");
			}
			if(X->Pointer.Pointed == INVALID_TYPE)
			{
				RaiseError(true, *Expr->ErrorInfo, "Cannot perform pointer arithmetic on an opaque pointer");
			}
		}


		if(Expr->Binary.Op != '=')
			Promoted = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Left, Right, &Expr->Binary.Left, &Expr->Binary.Right,  "Cannot perform binary expression with types.\nLeft: %s\nRight: %s");
		else
			Promoted = TypeCheckAndPromote(Checker, Expr->ErrorInfo, Left, Right, NULL, &Expr->Binary.Right, "Cannot assign to type %s from type %s");

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
					RaiseError(false, *Expr->ErrorInfo, "Left-hand side of assignment is not assignable");
				if(Promoted == Right && Promoted != Left)
				{
					RaiseError(false, *Expr->ErrorInfo, "Incompatible types in assignment expression!\n"
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
				RaiseError(true, *BinaryExpression->ErrorInfo, "Invalid binary op between pointer and integer!\n"
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
				RaiseError(false, *Expr->ErrorInfo, "Cannot do a pointer diff between 2 pointers of different types %s and %s",
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

		if(IsUntyped(Expr->Binary.ExpressionType))
			Checker->UntypedStack.Push(&Expr->Binary.ExpressionType);

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
				RaiseError(true, *ErrorInfo,
						"Redeclaration of variable %s.\n",
						//"If this is intentional mark it as a shadow like this:\n\t#shadow %s := 0;",
						ID->Data);
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
	//const string *ID = Node->Decl.ID;
	u32 Type = GetTypeFromTypeNode(Checker, Node->Decl.Type);
	if(Node->Decl.Expression)
	{
		if(Type != INVALID_TYPE)
			Checker->AutoEnum.Push(Type);

		u32 ExprType = AnalyzeExpression(Checker, Node->Decl.Expression);

		if(Type != INVALID_TYPE)
			Checker->AutoEnum.Pop();

		if(ExprType == INVALID_TYPE)
		{
			RaiseError(true, *Node->ErrorInfo, "Expression does not give a value for the assignment");
		}
		const type *ExprTypePointer = GetType(ExprType);

		if(Type != INVALID_TYPE)
		{
			const type *TypePointer = GetType(Type);
			const type *Promotion = NULL;
			if(!IsTypeCompatible(TypePointer, ExprTypePointer, &Promotion, true))
			{
				RaiseError(false, *Node->ErrorInfo, "Cannot assign expression of type %s to variable of type %s",
						GetTypeName(ExprTypePointer), GetTypeName(TypePointer));
				Promotion = NULL;
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
			RaiseError(true, *Node->ErrorInfo, "Expected either type or expression in variable declaration");
		}
	}

	const type *T = GetType(Type);
	if(IsUntyped(T))
	{
		// @TODO: This being signed integer could result in some problems
		// like:
		// Foo := 0xFF_FF_FF_FF;
		// Bar := @i32 Foo;
		// This also happens in the AST_MATCH type checking
		Type = UntypedGetType(T);
		FillUntypedStack(Checker, Type);
	}
	Node->Decl.TypeIndex = Type;
	if(IsFnOrPtr(T))
		Node->Decl.Flags |= SymbolFlag_Function;

	if(Node->Decl.LHS->Type == AST_LIST)
	{
		if(T->Kind != TypeKind_Struct || (T->Struct.Flags & StructFlag_FnReturn) == 0)
		{
			// @TODO: This shouldn't be an error
			// a, b := 10;
			// should just give both the value 10
			RaiseError(true, *Node->ErrorInfo,
					"Left-hand side is a declaration list but right-hand does not yield multiple values");
		}
		if(Node->Decl.LHS->List.Nodes.Count != T->Struct.Members.Count)
		{
			const char *l = "values";
			if(Node->Decl.LHS->List.Nodes.Count == 1)
				l = "value";
			RaiseError(true, *Node->ErrorInfo, "Left-hand side expects %d %s but right-hand side gives %d", Node->Decl.LHS->List.Nodes.Count, l, T->Struct.Members.Count);
		}
		slice<node *> Nodes = Node->Decl.LHS->List.Nodes;
		uint At = 0;
		For(Nodes)
		{
			if((*it)->Type != AST_ID)
			{
				RaiseError(false, *(*it)->ErrorInfo,
						"Only identifiers are allowed in the left-hand side list of declaration");
			}
			else
			{
				AddVariable(Checker, (*it)->ErrorInfo, T->Struct.Members[At++].Type, (*it)->ID.Name, Node, Node->Decl.Flags);
			}
		}
	}
	else if(Node->Decl.LHS->Type == AST_ID)
	{
		if(T->Kind == TypeKind_Struct && T->Struct.Flags & StructFlag_FnReturn)
		{
			RaiseError(true, *Node->ErrorInfo,
					"Right-hand side of expression gives multiple values but left-hand side handles only 1");
		}

		if(!NoAdd)
		{
			AddVariable(Checker, Node->ErrorInfo, Type, Node->Decl.LHS->ID.Name, Node, Node->Decl.Flags);
		}
	}
	else
	{
		// @NOTE: I don't think there is any way to get here
		RaiseError(true, *Node->ErrorInfo, "Invalid left-hand side of declaration");
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
		RaiseError(false, *Node->ErrorInfo, "Expected boolean type for condition expression, got %s.",
				GetTypeName(ExprType));
		return Basic_bool;
	}
	if(IsString(ExprType))
	{
		string CountMem = STR_LIT("count");
		node *Selector = MakeSelector(Node->ErrorInfo, Node, DupeType(CountMem, string));
		Selector->Selector.Index = 0;
		Selector->Selector.Type = Basic_string;

		const_value ZeroValue = {};
		ZeroValue.Type = const_type::Integer;
		node *Zero = MakeConstant(Node->ErrorInfo, ZeroValue);
		Zero->Constant.Type = Basic_int;
		*NodePtr = MakeBinary(Node->ErrorInfo, Selector, Zero, T_NEQ);

	}
	else if(ExprType->Kind == TypeKind_Basic && ((ExprType->Basic.Flags & BasicFlag_Boolean) == 0))
	{
		const_value ZeroValue = {};
		ZeroValue.Type = const_type::Integer;
		node *Zero = MakeConstant(Node->ErrorInfo, ZeroValue);
		Zero->Constant.Type = ExprTypeIdx;
		if(IsUntyped(ExprTypeIdx))
		{
			Checker->UntypedStack.Push(&Zero->Constant.Type);
		}
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
				AnalyzeNode(Checker, Node->For.Expr1);
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
				RaiseError(false, *Node->For.Expr2->ErrorInfo,
						"Expression is of non iteratable type %s", GetTypeName(T));
				return;
			}
			if(IsUntyped(T))
			{
				TypeIdx = Basic_int;
				T = GetType(TypeIdx);
				FillUntypedStack(Checker, TypeIdx);
			}
			u32 ItType = INVALID_TYPE;
			if(T->Kind == TypeKind_Array)
				ItType = T->Array.Type;
			else if(T->Kind == TypeKind_Slice)
				ItType = T->Slice.Type;
			else if(HasBasicFlag(T, BasicFlag_String))
			{
				if(CompileFlags & CF_Standalone)
				{
					RaiseError(true, *Node->ErrorInfo, "Cannot perform utf-8 string iteration in a standalone build");
				}
				ItType = Basic_u32;
			}
			else if(HasBasicFlag(T, BasicFlag_Integer))
				ItType = TypeIdx;
			else
				Assert(false);

			if(Node->For.Expr1->Type == AST_ID)
			{
				AddVariable(Checker, Node->For.Expr1->ErrorInfo, ItType,
						Node->For.Expr1->ID.Name, Node->For.Expr1, 0);
			}
			else if(Node->For.Expr1->Type == AST_UNARY)
			{
				auto it = Node->For.Expr1;
				if(it->Unary.Op == '&' && it->Unary.Operand->Type == AST_ID)
				{
					Node->For.ItByRef = true;
					if(IsString(T))
						ItType = GetPointerTo(Basic_u8);
					else
						ItType = GetPointerTo(ItType);

					AddVariable(Checker, Node->For.Expr1->ErrorInfo, ItType,
							it->Unary.Operand->ID.Name, Node->For.Expr1, 0);
				}
				else
				{
					RaiseError(true, *it->ErrorInfo, "Invalid expression at for in loop. Expected a name for the iterator");
				}
			}
			else
			{
				Assert(Node->For.Expr1->Type == AST_LIST);
				auto List = Node->For.Expr1->List;
				if(List.Nodes.Count != 2)
				{
					RaiseError(true, *Node->For.Expr1->ErrorInfo, "Expected exactly 2 names in list in for in iteration. First is the name of the index, second is the name of the iterator");
				}

				const string *Names[2] = {};
				ForArray(Idx, List.Nodes)
				{
					auto it = List.Nodes[Idx];
					if(it->Type != AST_ID)
					{
						bool Error = true;
						if(it->Type == AST_UNARY)
						{
							if(it->Unary.Op == '&' && it->Unary.Operand->Type == AST_ID)
							{
								Node->For.ItByRef = true;
								if(Idx == 0)
								{
									RaiseError(false, *it->ErrorInfo, "Index cannot be iterated by pointer");
								}
								else if(HasBasicFlag(T, BasicFlag_Integer))
								{
									RaiseError(false, *it->ErrorInfo, "Integer cannot be iterated by pointer");
								}
								else
								{
									if(IsString(T))
										ItType = GetPointerTo(Basic_u8);
									else
										ItType = GetPointerTo(ItType);
								}
								Error = false;
								Names[Idx] = it->Unary.Operand->ID.Name;
							}
						}
						if(Error)
							RaiseError(true, *it->ErrorInfo, "Expected a name for the iterator");
					}
					else
					{
						Names[Idx] = it->ID.Name;
					}
				}
				if(T->Kind == TypeKind_Array || T->Kind == TypeKind_Slice || HasBasicFlag(T, BasicFlag_String))
				{}
				else
				{
					RaiseError(true, *Node->For.Expr1->ErrorInfo, "Iterating variable of type %s doesn't accept multiple iteration names", GetTypeName(T));
				}
				AddVariable(Checker, List.Nodes[0]->ErrorInfo, Basic_int, Names[0], List.Nodes[0], 0);
				AddVariable(Checker, List.Nodes[1]->ErrorInfo, ItType,    Names[1], List.Nodes[1], 0);
			}
			Node->For.ItType = ItType;
			Node->For.ArrayType = TypeIdx;
		} break;
		case ft::While:
		{
			AnalyzeBooleanExpression(Checker, &Node->For.Expr1);
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
	u32 OpaqueType = FindEnumTypeNoModuleRenaming(Checker, Node->StructDecl.Name);
	Assert(OpaqueType != INVALID_TYPE);
	if(Node->Enum.Items.Count == 0)
	{
		RaiseError(false, *Node->ErrorInfo, "Empty enums are not allowed");
		return;
	}

	// @NOTE: Probably not needed?
	//u32 AlreadyDefined = FindType(Checker, Node->Enum.Name);
	//if(AlreadyDefined != INVALID_TYPE)
	//{
	//	RaiseError(true, *Node->ErrorInfo, "Enum %s is a redefinition, original type is %s",
	//			Node->Enum.Name->Data, GetTypeName(AlreadyDefined));
	//}


	u32 Type = INVALID_TYPE;
	if(Node->Enum.Type)
	{
		Type = GetTypeFromTypeNode(Checker, Node->Enum.Type);
		Assert(Type != INVALID_TYPE);
		const type *T = GetType(Type);
		if(!HasBasicFlag(T, BasicFlag_Integer))
		{
			RaiseError(false, *Node->ErrorInfo, "Enum type must be integral, cannot use %s",
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
				RaiseError(false, *Item->ErrorInfo, "Missing value. Other members in the enum use values and mixing is not allowed");
			}
		}
		else
		{
			if(Item->Item.Expression)
			{
				RaiseError(false, *Item->ErrorInfo, "Using expression in an enum in which other members don't use expressions is not allowed");

			}
		}
	}

	ForArray(Idx, Node->Enum.Items)
	{
		enum_member Member = {};
		auto Item = Node->Enum.Items[Idx]->Item;
		Member.Name = *Item.Name;
		Member.Module = Checker->Module;
		if(Item.Expression)
		{
			u32 T = AnalyzeExpression(Checker, Item.Expression);
			TypeCheckAndPromote(Checker, Item.Expression->ErrorInfo, Type, T, NULL,
					&Node->Enum.Items.Data[Idx]->Item.Expression, "Enum of type %s cannot contains value of type %s");
			Member.Expr = Item.Expression;
		}
		else
		{
			const_value Value = {};
			Value.Type = const_type::Integer;
			Value.Int.IsSigned = false;
			Value.Int.Unsigned = Idx;
			Member.Expr = MakeConstant(Node->ErrorInfo, Value);
			Member.Expr->Constant.Type = Type;
		}
		Members.Push(Member);
	}

	FillOpaqueEnum(*Node->Enum.Name, SliceFromArray(Members), Type, OpaqueType);
}

void AnalyzeStructDeclaration(checker *Checker, node *Node)
{
	u32 OpaqueType = FindStructTypeNoModuleRenaming(Checker, Node->StructDecl.Name);
	Assert(OpaqueType != INVALID_TYPE);
	scope *StructScope = AllocScope(Node, Checker->Scope.TryPeek());
	Checker->Scope.Push(StructScope);

	u32 SubTypes = INVALID_TYPE;

	type New = {};
	New.Kind = TypeKind_Struct;
	New.Struct.Name = *Node->StructDecl.Name;

	array<struct_member> Members {Node->StructDecl.Members.Count};
	bool NeedToAddGeneric = false;

	For(Node->StructDecl.Members)
	{
		u32 Type = GetTypeFromTypeNode(Checker, (*it)->Var.TypeNode);
		if(Type == INVALID_TYPE)
		{
			NeedToAddGeneric = true;
			break;
		}
	}

	ForArray(Idx, Node->StructDecl.Members)
	{
		node *Member = Node->StructDecl.Members[Idx];
		u32 Type = GetTypeFromTypeNode(Checker, Member->Var.TypeNode);
		//Type = FixPotentialFunctionPointer(Type);
		const type *T = GetType(Type);
		if(T->Kind == TypeKind_Function)
		{
			RaiseError(false, *Member->ErrorInfo, "Cannot put function as struct member, if you want a function pointer declare it as *fn(...)");
			Type = GetPointerTo(Type);
			T = GetType(Type);
		}
		if(Member->Var.Name == NULL)
		{
			if(Idx != 0)
			{
				RaiseError(true, *Member->ErrorInfo, "Subtype declaration needs to come before any struct members, the base type is layed out at the start of the struct");
			}
			if(T->Kind != TypeKind_Struct)
			{
				RaiseError(true, *Member->ErrorInfo, "Cannot subtype non struct type %s", GetTypeName(T));
			}
			if(T->Struct.SubType != INVALID_TYPE)
			{
				RaiseError(true, *Member->ErrorInfo, "Multi-level subtyping is not allowed");
			}
			if(SubTypes != INVALID_TYPE)
			{
				RaiseError(true, *Member->ErrorInfo, "Cannot subtype multiple types! Trying to subtype %s while already subtyping %s", GetTypeName(T), GetTypeName(SubTypes));
			}
			if(IsGeneric(SubTypes))
			{
				RaiseError(true, *Member->ErrorInfo, "Cannot subtype a generic");
			}

			SubTypes = Type;
			Members.Data[Idx].ID = STR_LIT("base");
			Members.Data[Idx].Type = Type;
			continue;
		}

		if(HasBasicFlag(T, BasicFlag_TypeID))
		{
			if(NeedToAddGeneric)
			{
				MakeGeneric(StructScope, *Node->StructDecl.Members[Idx]->Var.Name);
			}
		}
		else if(IsGeneric(T))
		{
			if(New.Struct.Flags & StructFlag_Generic)
			{
				RaiseError(true, *Node->ErrorInfo, "Structs cannot have more than 1 generic type");
			}
			New.Struct.Flags |= StructFlag_Generic;
		}

		Node->StructDecl.Members[Idx]->Var.Type = Type;
		Members.Data[Idx].ID = *Node->StructDecl.Members[Idx]->Var.Name;
		Members.Data[Idx].Type = Type;
	}

	ForArray(Idx, Node->StructDecl.Members)
	{
		if(Node->StructDecl.Members[Idx]->Var.Name == NULL)
			continue;
		for(uint j = Idx + 1; j < Node->StructDecl.Members.Count; ++j)
		{
			if(*Node->StructDecl.Members[Idx]->Var.Name == *Node->StructDecl.Members[j]->Var.Name)
			{
				RaiseError(true, *Node->ErrorInfo, "Invalid struct declaration, members #%d and #%d have the same name `%s`",
						Idx, j, Node->StructDecl.Members[Idx]->Var.Name->Data);
			}
		}
	}

	New.Struct.Members = SliceFromArray(Members);
	New.Struct.SubType = SubTypes;
	if(Node->StructDecl.IsUnion)
	{
		if(Node->StructDecl.Members.Count == 0)
			RaiseError(false, *Node->ErrorInfo, "Empty unions are not allowed");
		if(SubTypes != INVALID_TYPE)
			RaiseError(true, *Node->ErrorInfo, "Unions cannot use subtyping");
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
		if(Node->Type == AST_RETURN || Node->Type == AST_YIELD)
		{
			b32 DeadCode = false;
			if(Idx + 1 != Body.Count)
			{
				if(!IsNodeEndScope(Body[Idx+1]) || Idx + 2 != Body.Count)
					DeadCode = true;
			}
			if(DeadCode)
			{
				const char *Name = "return";
				if(Node->Type == AST_YIELD)
					Name = "yield";

				RaiseError(false, *Body[Idx + 1]->ErrorInfo, "Unreachable code after %s statement", Name);
			}
		}
		if(Node->Type == AST_BREAK && Idx + 1 != Body.Count)
		{
			if(!IsNodeEndScope(Body[Idx+1]) || Idx + 2 != Body.Count)
				RaiseError(false, *Body[Idx + 1]->ErrorInfo, "Unreachable code after break statement");
		}
		if(Node->Type == AST_CONTINUE && Idx + 1 != Body.Count)
		{
			if(!IsNodeEndScope(Body[Idx+1]) || Idx + 2 != Body.Count)
				RaiseError(false, *Body[Idx + 1]->ErrorInfo, "Unreachable code after continue statement");
		}
	}
}

void AnalyzeUsing(checker *Checker, node *Node)
{
	Assert(Node->Type == AST_USING);
	node *Expr = Node->TypedExpr.Expr;
	u32 TIdx = AnalyzeExpression(Checker, Expr);
	const type *T = GetType(TIdx);
	if(HasBasicFlag(T, BasicFlag_TypeID))
	{
		TIdx = GetTypeFromTypeNode(Checker, Expr);
		T = GetType(TIdx);
		if(T->Kind != TypeKind_Enum)
		{
			RaiseError(false, *Expr->ErrorInfo, "Invalid type for using expression. `using` is only valid on enum types but this is %s", GetTypeName(T));
			return;
		}
	}
	else if(T->Kind != TypeKind_Struct)
	{
		if(T->Kind == TypeKind_Pointer && T->Pointer.Pointed != INVALID_TYPE && GetType(T->Pointer.Pointed)->Kind == TypeKind_Struct)
		{
			T = GetType(T->Pointer.Pointed);
		}
		else
		{
			RaiseError(false, *Expr->ErrorInfo, "Invalid type on using expression, need an enum, struct, or pointer to struct");
			return;
		}
	}
	Node->TypedExpr.TypeIdx = TIdx;

	switch(Expr->Type)
	{
		case AST_ID:
		{
			if(T->Kind == TypeKind_Enum)
			{
				For(T->Enum.Members)
				{
					AddVariable(Checker, Node->ErrorInfo, TIdx, DupeType(it->Name, string), Node, 0);
				}
			}
			else
			{
				For(T->Struct.Members)
				{
					AddVariable(Checker, Node->ErrorInfo, it->Type, DupeType(it->ID, string), Node, 0);
					// @Cleanup: Useless DupeType? Maybe taking a pointer from T->Struct.Members is safe
				}
			}
		} break;
		default:
		{
			RaiseError(false, *Expr->ErrorInfo, "Invalid expression for using");
		} break;
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
		case AST_ASSERT:
		{
			if(CompileFlags & CF_Standalone)
			{
				// @TODO: Have a way to specify assert needed functionality to enable it
				RaiseError(true, *Node->ErrorInfo, "Cannot use #assert in a standalone build");
			}
			AnalyzeBooleanExpression(Checker, &Node->Assert.Expr);
		} break;
		case AST_USING:
		{
			AnalyzeUsing(Checker, Node);
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
				RaiseError(false, *Node->ErrorInfo, "Invalid context for continue, not a for loop");
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
					RaiseError(false, *Node->ErrorInfo, "Invalid context for break, not a for loop to break out of a match statement use return instead");
				}
				else
				{
					RaiseError(false, *Node->ErrorInfo, "Invalid context for break, not a for loop");
				}
			}
		} break;
		case AST_FOR:
		{
			AnalyzeFor(Checker, Node);
		} break;
		case AST_RETURN:
		{
			u32 FnRetTypeID = ReturnsToType(Checker->CurrentFnReturnTypeIdx);
			if(Node->Return.Expression)
			{
				if(Checker->CurrentFnReturnTypeIdx.Count == 0)
				{
					RaiseError(false, *Node->ErrorInfo, "Trying to return a value in a void function");
					break;
				}
				u32 Result = AnalyzeExpression(Checker, Node->Return.Expression);
				const type *Type = GetType(Result);
				const type *Return = GetType(FnRetTypeID);
				const type *Promotion = NULL;

				if(Checker->CurrentFnReturnTypeIdx.Count > 1)
				{
					if(Type->Kind != TypeKind_Struct || (Type->Struct.Flags & StructFlag_FnReturn) == 0)
					{
						RaiseError(false, *Node->ErrorInfo, "Function expects %d values to be returned but only 1 was provided", Checker->CurrentFnReturnTypeIdx.Count);
					}
					else if(Type->Struct.Members.Count != Checker->CurrentFnReturnTypeIdx.Count)
					{
						RaiseError(false, *Node->ErrorInfo, "Function expects %d values to be returned but %d were provided", Checker->CurrentFnReturnTypeIdx.Count, Type->Struct.Members.Count);
					}
				}

				if(!IsTypeCompatible(Return, Type, &Promotion, true))
				{
RetErr:
					RaiseError(false, *Node->ErrorInfo, "Type of return expression does not match function return type!\n"
							"Expected: %s\n"
							"Got: %s",
							GetTypeName(Return),
							GetTypeName(Type));
				}
				if(Promotion)
				{
					promotion_description Promote = PromoteType(Promotion, Return, Type, FnRetTypeID, Result);
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
			else if(Checker->CurrentFnReturnTypeIdx.Count != 0)
			{
				RaiseError(false, *Node->ErrorInfo, "Function expects a return value, invalid empty return!");
			}
			Node->Return.TypeIdx = FnRetTypeID;
		} break;
		case AST_YIELD:
		{
			u32 T = INVALID_TYPE;
			if(Node->TypedExpr.Expr)
			{
				T = AnalyzeExpression(Checker, Node->TypedExpr.Expr);
			}
			Node->TypedExpr.TypeIdx = T;
			Checker->YieldT = T;
			Checker->YieldExpr = &Node->TypedExpr.Expr;

			b32 FoundYieldScope = false;
			scope *Current = Checker->Scope.TryPeek();

			while(Current)
			{
				if(Current->ScopeNode->Type == AST_MATCH)
				{
					FoundYieldScope = true;
					break;
				}
				Current = Current->Parent;
			}
			if(!FoundYieldScope)
			{
				RaiseError(false, *Node->ErrorInfo, "yield outside of match statement is not valid");
			}
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
					RaiseError(false, *Node->ErrorInfo, "Unexpected scope closing }");
				}
				Checker->Scope.Pop();
			}
		} break;
		case AST_RUN:
		{
			if(Node->Run.Body.Count == 0)
			{
				RaiseError(false, *Node->ErrorInfo, "#run doesn't have an expression");
			}
			else
			{
				string WasBonus = BonusErrorMessage;
				SetBonusMessage(STR_LIT("While evaluating compile time #run. Keep in mind that local variables and function arguments are not available."));

				auto CurrentScope = Checker->Scope;
				auto WasOutOfRun = Checker->OutOfRun;

				Checker->Scope = {};
				Checker->Scope.Push(AllocScope(Node));

				if(Checker->OutOfRun.IsEmpty())
					Checker->OutOfRun = CurrentScope;

				AnalyzeInnerBody(Checker, Node->Run.Body);

				Checker->Scope = CurrentScope;
				Checker->OutOfRun = WasOutOfRun;

				SetBonusMessage(WasBonus);
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
			type *New = AllocType(TypeKind_Struct);
			New->Struct.Name = Name;

			// @TODO: Cleanup
			uint Count = GetTypeCount();
			string SymbolName = StructToModuleName(Name, Module->Name);
			for(int i = 0; i < Count; ++i)
			{
				const type *T = GetType(i);
				if(T->Kind == TypeKind_Struct)
				{
					if(T->Struct.Name == SymbolName)
					{
						RaiseError(true, *Nodes[I]->ErrorInfo, "Redifinition of struct %s", Name.Data);
					}
				}
				else if(T->Kind == TypeKind_Enum)
				{
					if(T->Enum.Name == SymbolName)
					{
						RaiseError(true, *Nodes[I]->ErrorInfo, "Redifinition of enum %s as struct", Name.Data);
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

bool CheckIntrinsic(string Name)
{
	string Intrinsics[] = {
		STR_LIT("compare_exchange"),
		STR_LIT("debug_break"),
	};
	size_t Len = ARR_LEN(Intrinsics);
	for(int i = 0; i < Len; ++i)
	{
		if(Intrinsics[i] == Name)
		{
			return true;
		}
	}
	return false;
}

symbol *AnalyzeFunctionDecl(checker *Checker, node *Node)
{
	Assert(Node->Type == AST_FN);
	if(Node->Fn.Flags & SymbolFlag_Intrinsic)
	{
		if(!CheckIntrinsic(*Node->Fn.Name))
		{
			RaiseError(false, *Node->ErrorInfo, "Declaration for an unknown intrinsic: %s", Node->Fn.Name->Data);
		}
	}
	u32 FnType = CreateFunctionType(Checker, Node);
	Node->Fn.TypeIdx = FnType;
	return CreateFunctionSymbol(Checker, Node);
}

void AnalyzeFunctionDecls(checker *Checker, dynamic<node *> *NodesPtr, module *ThisModule)
{
	Checker->Nodes = NodesPtr;
	Checker->Module = ThisModule;
	Checker->Scope = {};
	Checker->CurrentFnReturnTypeIdx = {};
	Checker->Scope.Push(AllocScope(NULL));

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
				RaiseError(true, *Nodes[I]->ErrorInfo, "Function %s redifines other symbol in file %s at (%d:%d)",
						Node->Fn.Name->Data,
						Redifined->Node->ErrorInfo->FileName, Redifined->Node->ErrorInfo->Range.StartLine, Redifined->Node->ErrorInfo->Range.StartChar);
			}
		}
	}

	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_DECL)
		{
			node *Node = Nodes[I];
			u32 Type = AnalyzeDeclerations(Checker, Node, true);
			if(Node->Decl.Expression)
			{
				if(!IsConstant(Checker, Node->Decl.Expression))
				{
					RaiseError(false, *Node->Decl.Expression->ErrorInfo, "Expression for global variable must be constant");
					continue;
				}
			}
			if(Node->Decl.LHS->Type == AST_ID)
			{
				string Name = *Node->Decl.LHS->ID.Name;
				symbol *Sym = NewType(symbol);
				Sym->Checker = Checker;
				Sym->Name = Node->Decl.LHS->ID.Name;
				Sym->LinkName = StructToModuleNamePtr(Name, ThisModule->Name);
				Sym->Type = Type;
				Sym->Flags = Node->Decl.Flags;
				Sym->Node = Node;

				Sym->Flags &= ~SymbolFlag_Function;
				bool Success = Checker->Module->Globals.Add(Name, Sym);
				if(!Success)
				{
					symbol *Redifined = Checker->Module->Globals[Name];
					Assert(Redifined);
					RaiseError(true, *Nodes[I]->ErrorInfo, "Variable %s redifines other symbol in file %s at (%d:%d)",
							Name.Data,
							Redifined->Node->ErrorInfo->FileName, Redifined->Node->ErrorInfo->Range.StartLine, Redifined->Node->ErrorInfo->Range.StartChar);
				}
			}
			else if(Node->Decl.LHS->Type == AST_LIST)
			{
				RaiseError(true, *Node->ErrorInfo, "Multi-variable declaration is not allowed in global scope"); 
				const type *T = GetType(Type);
				if(T->Kind != TypeKind_Struct || (T->Struct.Flags & StructFlag_FnReturn) == 0)
				{
					RaiseError(false, *Node->ErrorInfo,
							"Left-hand side is a declaration list but right-hand does not yield multiple values");
					continue;
				}

				slice<node *> Nodes = Node->Decl.LHS->List.Nodes;
				For(Nodes)
				{
					if((*it)->Type != AST_ID)
					{
						RaiseError(true, *(*it)->ErrorInfo,
								"Only identifiers are allowed in the left-hand side list of declaration");
					}
					string Name = *(*it)->ID.Name;
					symbol *Sym = NewType(symbol);
					Sym->Checker = Checker;
					Sym->Name = (*it)->ID.Name;
					Sym->LinkName = StructToModuleNamePtr(Name, ThisModule->Name);
					Sym->Type = Type;
					Sym->Flags = Node->Decl.Flags;
					Sym->Node = Node;
					bool Success = Checker->Module->Globals.Add(Name, Sym);
					if(!Success)
					{
						symbol *Redifined = Checker->Module->Globals[Name];
						Assert(Redifined);
						RaiseError(true, *Nodes[I]->ErrorInfo, "Variable %s redifines other symbol in file %s at (%d:%d)",
								Name.Data,
								Redifined->Node->ErrorInfo->FileName, Redifined->Node->ErrorInfo->Range.StartLine, Redifined->Node->ErrorInfo->Range.StartChar);
					}

				}

			}
			else
			{
				// @NOTE: I don't think there is any way to get here
				RaiseError(true, *Node->ErrorInfo, "Invalid left-hand side of declaration");
			}
		}
	}
}

void AnalyzeEnumDefinitions(slice<node *> Nodes, module *Module)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_ENUM)
		{
			string Name = *Nodes[I]->Enum.Name;
			type *New = AllocType(TypeKind_Enum);
			New->Enum.Name = Name;

			// @TODO: Cleanup
			uint Count = GetTypeCount();
			string SymbolName = StructToModuleName(Name, Module->Name);
			for(int i = 0; i < Count; ++i)
			{
				const type *T = GetType(i);
				if(T->Kind == TypeKind_Struct)
				{
					if(T->Struct.Name == SymbolName)
					{
						RaiseError(true, *Nodes[I]->ErrorInfo, "Redifinition of struct %s", Name.Data);
					}
				}
				else if(T->Kind == TypeKind_Enum)
				{
					if(T->Enum.Name == SymbolName)
					{
						RaiseError(true, *Nodes[I]->ErrorInfo, "Redifinition of enum %s as struct", Name.Data);
					}
				}
			}

			AddType(New);
		}
	}
}

void AnalyzeForUserDefinedTypes(checker *Checker, slice<node *> Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_DECL)
		{
			node *Node = Nodes[I];
			if(Node->Decl.Type)
			{
				u32 T = GetTypeFromTypeNode(Checker, Node->Decl.Type);
				if(T != Basic_type)
					continue;
			}
			if(Node->Decl.LHS->Type != AST_ID)
				continue;
			u32 T = GetTypeFromTypeNode(Checker, Node->Decl.Expression, false);
			if(T == INVALID_TYPE)
				continue;

			string Name = *Node->Decl.LHS->ID.Name;
			string *TypeName = StructToModuleNamePtr(Name, Checker->Module->Name);

			AddNameToTypeMap(TypeName, T);
			Nodes[I]->Type = AST_NOP;
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

void AnalyzeFillStructCaches(checker *Checker, slice<node *> Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_STRUCTDECL)
		{
			node *Node = Nodes[I];
			u32 TypeIdx = FindStructTypeNoModuleRenaming(Checker, Node->StructDecl.Name);
			if((GetType(TypeIdx)->Struct.Flags & StructFlag_Generic) == 0)
			{
				SetStructCache(TypeIdx);
			}
		}
	}

}

void CheckForRecursiveStructs(checker *Checker, slice<node *> Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		if(Nodes[I]->Type == AST_STRUCTDECL)
		{
			node *Node = Nodes[I];
			u32 TypeIdx = FindStructTypeNoModuleRenaming(Checker, Node->StructDecl.Name);
			if((GetType(TypeIdx)->Struct.Flags & StructFlag_Generic) == 0)
			{
				int Failed = -1;
				if(!VerifyNoStructRecursion(TypeIdx, &Failed))
				{
					node *Member = Node->StructDecl.Members[Failed];
					string Name = GetTypeNameAsString(TypeIdx);
					RaiseError(false, *Member->ErrorInfo, "Member %s of struct %s recursively uses the struct's type, you can try to replace it with a pointer %s: *%s",
							Member->Var.Name->Data, Name.Data, Member->Var.Name->Data, GetTypeName(Member->Var.Type));
				}
			}
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

string *MakeGenericName(checker *Checker, string BaseName, u32 FnTypeNonGeneric, u32 FnTypeGeneric, node *ErrorNode, slice<node *>Arguments)
{
	const type *FG = GetType(FnTypeGeneric);
	const type *T = GetType(FnTypeNonGeneric);
	string_builder Builder = MakeBuilder();
	Builder += BaseName;
	Builder += ':';
	Builder += '(';
	for(int i = 0; i < T->Function.ArgCount; ++i)
	{
		if(i != 0)
			Builder += ',';

		b32 DontPrint = false;
		if(IsGeneric(FG->Function.Args[i]))
		{
			u32 ResolvedGenericID = GetGenericPart(T->Function.Args[i], FG->Function.Args[i]);
			if(ResolvedGenericID == INVALID_TYPE)
			{
				// @NOTE: I think this is checked earilier but just to be sure
				RaiseError(true, *ErrorNode->ErrorInfo, "Invalid type for generic declaration");
			}
			const type *RG = GetType(ResolvedGenericID);
			if(RG->Kind == TypeKind_Struct)
			{
				DontPrint = true;
				Builder += GetTypeNameAsString(RG);
				Builder += " {";
				ForArray(Idx, RG->Struct.Members)
				{
					if(Idx != 0)
						Builder += ',';
					Builder += GetTypeNameAsString(RG->Struct.Members[Idx].Type);
				}
				Builder += "}";
			}
		}
		else if(HasBasicFlag(GetType(FG->Function.Args[i]), BasicFlag_TypeID))
		{
			DontPrint = true;
			u32 T = GetTypeFromTypeNode(Checker, Arguments[i]);
			Builder += GetTypeNameAsString(T);
		}

		if(!DontPrint)
			Builder += GetTypeNameAsString(T->Function.Args[i]);
	}
	Builder += ')';
	if(T->Function.Returns.Count == 0)
		Builder += "->void";
	else
	{
		Builder += "->";
		Builder += GetTypeNameAsString(ReturnsToType(T->Function.Returns));
	}
	string Result = MakeString(Builder);
	return DupeType(Result, string);
}

b32 IsFunctionCorrectProfileCallback(const type *T)
{
	if(T->Kind == TypeKind_Pointer)
	{
		if(T->Pointer.Pointed == INVALID_TYPE)
			return false;
		T = GetType(T->Pointer.Pointed);
	}
	if(T->Kind != TypeKind_Function)
		return false;

	if(T->Function.ArgCount != 2
			|| !IsString(GetType(T->Function.Args[0]))
			|| T->Function.Args[1] != Basic_int)
		return false;

	if(T->Function.Returns.Count != 0)
		return false;

	return true;
}

void Analyze(checker *Checker, dynamic<node *> &Nodes)
{
	for(int I = 0; I < Nodes.Count; ++I)
	{
		node *Node = Nodes[I];
		if(Node->Type == AST_FN)
		{
			if((Node->Fn.Flags & SymbolFlag_Intrinsic) == 0 && (Node->Fn.Flags & SymbolFlag_Generic) == 0)
			{
				if(Node->Fn.ProfileCallback)
				{
					u32 TIdx = AnalyzeExpression(Checker, Node->Fn.ProfileCallback);
					Node->Fn.CallbackType = TIdx;
					const type *T = GetType(TIdx);
					if(!IsFunctionCorrectProfileCallback(T))
					{
						RaiseError(false, *Node->ErrorInfo, 
								"@profile callback is of incorrect type %s\n"
								"Expected fn(fn_name: string, cycles_taken: int);",
								GetTypeName(T));
					}
				}
				AnalyzeFunctionBody(Checker, Node->Fn.Body, Node, Node->Fn.TypeIdx);
			}
		}
		else if(Node->Type == AST_RUN)
		{
			if(Node->Run.Body.Count == 0)
			{
				RaiseError(false, *Node->ErrorInfo, "#run doesn't have an expression");
			}
			AnalyzeInnerBody(Checker, Node->Run.Body);
		}
	}
}

