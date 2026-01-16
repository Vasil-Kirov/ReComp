#include "Polymorph.h"
#include "VString.h"
#include "Semantics.h"
#include "Parser.h"
#include "Memory.h"

string GetGenericName(const type *T)
{
	switch(T->Kind)
	{
		case TypeKind_Generic:
		{
			return T->Generic.Name;
		} break;
		case TypeKind_Struct:
		{
			return T->Struct.Name;
		} break;
		default: unreachable;
	}
}

u32 FunctionTypeGetNonGeneric(const type *Old, dict<u32> DefinedGenerics)
{
	type *NewFT = AllocType(TypeKind_Function);
	NewFT->Function.Flags = Old->Function.Flags & ~SymbolFlag_Generic;
	NewFT->Function.ArgCount = Old->Function.ArgCount;
	NewFT->Function.Args = (u32 *)AllocatePermanent(sizeof(u32) * Old->Function.ArgCount);
	NewFT->Function.DefaultValues = Old->Function.DefaultValues;
	for(int ArgI = 0; ArgI < Old->Function.ArgCount; ++ArgI)
	{
		u32 TypeIdx = Old->Function.Args[ArgI];
		const type *T = GetTypeRaw(TypeIdx);
		if(IsGeneric(T))
		{
			string Name = GetGenericName(GetTypeRaw(GetGenericPart(TypeIdx, TypeIdx)));
			u32 Resolved = DefinedGenerics[Name];
			TypeIdx = ToNonGeneric(TypeIdx, Resolved);
		}
		NewFT->Function.Args[ArgI] = TypeIdx;
	}

	array<u32> Returns(Old->Function.Returns.Count);
	uint At = 0;
	For(Old->Function.Returns)
	{
		if(IsGeneric(*it))
		{
			string Name = GetGenericName(GetTypeRaw(GetGenericPart(*it, *it)));
			u32 Resolved = DefinedGenerics[Name];
			Returns[At++] = ToNonGeneric(*it, Resolved);
		}
		else
		{
			Returns[At++] = *it;
		}
	}
	NewFT->Function.Returns = SliceFromArray(Returns);

	
	return AddType(NewFT);
}

u32 PolymorphGenericStruct(const error_info *err_i, const type *PassedStruct, const type *Gen, dict<u32> &DefinedGenerics)
{
	array<struct_generic_argument> NewArgs(Gen->Struct.GenericArguments.Count);
	ForArray(Idx, Gen->Struct.GenericArguments)
	{
		auto it = &Gen->Struct.GenericArguments.Data[Idx];
		struct_generic_argument Arg = *it;
		if(IsGeneric(it->DefinedAs))
		{
			u32 GenericIdx = GetGenericPart(it->DefinedAs, it->DefinedAs);
			const type *Gen = GetTypeRaw(GenericIdx);
			string Name = GetGenericName(Gen);
			u32 *DefinedPtr = DefinedGenerics.GetUnstablePtr(Name);
			if(DefinedPtr == NULL)
			{
				// @Note: probably reachable
				if(PassedStruct)
					RaiseError(true, *err_i, "Parameter type is an unresolved polymorphic type");
				else
					RaiseError(true, *err_i, "Return type is an unresolved polymorphic type");
			}
			u32 Defined = *DefinedPtr;
			Arg.DefinedAs = ToNonGeneric(it->DefinedAs, Defined);
		}
		else if(it->DefinedAs == INVALID_TYPE)
		{
			if(PassedStruct == nullptr)
			{
				RaiseError(true, *err_i, "Function signature returns a generic struct without a type parameter!");
			}
			Arg.DefinedAs = PassedStruct->Struct.GenericArguments[Idx].DefinedAs;
		}
		NewArgs[Idx] = Arg;
	}
	string_builder B = {};
	for(int i = 0; i < Gen->Struct.Name.Size && Gen->Struct.Name.Data[i] != '<'; ++i)
	{
		B += Gen->Struct.Name.Data[i];
	}
	slice<struct_member> Members = ResolveGenericStruct(Gen, SliceFromArray(NewArgs), &DefinedGenerics);
	u32 AsDefined = MakeStruct(MakeString(B), Members, SliceFromArray(NewArgs), Gen->Struct.Flags & ~StructFlag_Generic);
	DefinedGenerics.Add(Gen->Struct.Name, AsDefined);
	return AsDefined;
}

symbol *GenerateFunctionFromPolymorphicCall(checker *Checker, node *Call)
{
	symbol *FnSym = FindSymbolFromNode(Checker, Call->Call.Fn);
	if(!FnSym)
	{
		RaiseError(true, *Call->ErrorInfo, "Couldn't find polymorphic function symbol");
	}

	dict<u32> DefinedGenerics = {};

	const type *FnT = GetType(FnSym->Type);
	Assert(FnT->Kind == TypeKind_Function);
	Assert(IsGeneric(FnT));
	for(int ArgI = 0; ArgI < FnT->Function.ArgCount; ++ArgI)
	{
		u32 ArgTypeIdx = FnT->Function.Args[ArgI];
		const type *T = GetType(ArgTypeIdx);
		auto Arg = FnSym->Node->Fn.Args[ArgI]->Var;
		if(HasBasicFlag(T, BasicFlag_TypeID))
		{
			u32 T = GetTypeFromTypeNode(Checker, Call->Call.Args[ArgI]);
			if(!DefinedGenerics.Add(*Arg.Name, T))
			{
				// @Note: Unreachable?
				RaiseError(true, *FnSym->Node->Fn.Args[ArgI]->ErrorInfo,
						"Redefined polymorphic type %s", Arg.Name->Data);
			}
		}
		else if(IsGeneric(T))
		{
			u32 GenericIdx = GetGenericPart(ArgTypeIdx, ArgTypeIdx);
			const type *GenT = GetTypeRaw(GenericIdx);
			Assert(IsGeneric(GenT));
			if(Arg.IsAutoDefineGeneric)
			{
				u32 ExprTypeIdx = Call->Call.ArgTypes[ArgI];
				if(IsUntyped(ExprTypeIdx))
				{
					ExprTypeIdx = UntypedGetType(ExprTypeIdx);
					FillUntypedStack(Checker, ExprTypeIdx);
				}

				if(GenT->Kind == TypeKind_Struct)
				{
					const type *Defined = GetType(GetGenericPart(ExprTypeIdx, ArgTypeIdx));
					ForArray(Idx, GenT->Struct.GenericArguments)
					{
						auto it = GenT->Struct.GenericArguments[Idx];
						if(!it.IsAutoDefine)
							continue;


						string Name = GetGenericName(GetTypeRaw(GetGenericPart(it.DefinedAs, it.DefinedAs)));
						if(!DefinedGenerics.Add(Name, Defined->Struct.GenericArguments[Idx].DefinedAs))
						{
							// @Note: Unreachable?
							RaiseError(true, *FnSym->Node->Fn.Args[ArgI]->ErrorInfo,
									"Redefined polymorphic type %s", Arg.Name->Data);
						}
					}
				}
				else
				{
					u32 DefineType = GetGenericPart(ExprTypeIdx, ArgTypeIdx);
					if(!DefinedGenerics.Add(GetGenericName(GenT), DefineType))
					{
						// @Note: Unreachable?
						RaiseError(true, *FnSym->Node->Fn.Args[ArgI]->ErrorInfo,
								"Redefined polymorphic type %s", Arg.Name->Data);
					}
				}
			}
		}
	}

	for(int ArgI = 0; ArgI < FnT->Function.ArgCount; ++ArgI)
	{
		if(ArgI >= Call->Call.Args.Count)
			break;

		auto err_i = Call->Call.Args[ArgI]->ErrorInfo;
		u32 ArgTypeIdx = FnT->Function.Args[ArgI];
		const type *T = GetTypeRaw(ArgTypeIdx);
		if(IsGeneric(T))
		{
			u32 GenericIdx = GetGenericPart(ArgTypeIdx, ArgTypeIdx);
			const type *Gen = GetTypeRaw(GenericIdx);
			u32 AsDefined = INVALID_TYPE;
			if(Gen->Kind == TypeKind_Struct)
			{
				const type *PassedStruct = GetType(GetGenericPart(Call->Call.ArgTypes[ArgI], ArgTypeIdx));
				AsDefined = PolymorphGenericStruct(err_i, PassedStruct, Gen, DefinedGenerics);
				AsDefined = ToNonGeneric(ArgTypeIdx, AsDefined);
			}
			else
			{
				string Name = GetGenericName(Gen);
				u32 *DefinedPtr = DefinedGenerics.GetUnstablePtr(Name);
				if(DefinedPtr == NULL)
				{
					// @Note: probably reachable
					RaiseError(true, *err_i, "Parameter type is an unresolved polymorphic type");
				}
				else
				{
					u32 Defined = *DefinedPtr;
					AsDefined = ToNonGeneric(ArgTypeIdx, Defined);
				}
			}
			TypeCheckAndPromote(Checker, err_i, AsDefined, Call->Call.ArgTypes[ArgI], NULL,
					&Call->Call.Args.Data[ArgI], "Polymorphic argument resolves to type %s but expression is of incompatible type %s");
		}
	}

	For(FnT->Function.Returns)
	{
		if(IsGeneric(*it))
		{
			auto err_i = Call->ErrorInfo;
			u32 GenericIdx = GetGenericPart(*it, *it);
			const type *GenT = GetTypeRaw(GenericIdx);
			if(GenT->Kind == TypeKind_Struct)
			{
				PolymorphGenericStruct(err_i, nullptr, GenT, DefinedGenerics);
			}
			else
			{
				string Name = GetGenericName(GenT);
				u32 *DefinedPtr = DefinedGenerics.GetUnstablePtr(Name);
				if(DefinedPtr == NULL)
				{
					// @Note: probably reachable
					RaiseError(true, *err_i, "return of function is an unresolved polymorphic type");
				}
			}
		}
	}

	u32 NonGeneric = FunctionTypeGetNonGeneric(FnT, DefinedGenerics);
	// @THREADING
	For(FnSym->Generated)
	{
		if(it->T == NonGeneric)
			return it->S;
	}

	node *NewFnNode = CopyASTNode(FnSym->Node);
	NewFnNode->Fn.FnModule = FnSym->Checker->Module;
	string_builder Builder = MakeBuilder();
	Builder.printf("While parsing generic call to %s at %s(%d:%d)\n",
			FnSym->Name->Data, Call->ErrorInfo->FileName, Call->ErrorInfo->Range.StartLine, Call->ErrorInfo->Range.StartChar);
	
	string LastBonusMessage = GetBonusMessage();
	SetBonusMessage(MakeString(Builder));

	stack<scope*> SaveScopes = Checker->Scope;
	if(FnSym->Checker == Checker)
	{
		Checker->Scope = {};
	}

	size_t Before = GenericReplacements.Count;
	for(int i = 0; i < DefinedGenerics.Data.Count; ++i)
	{
		string Key = DefinedGenerics.Keys[i].N;
		u32 T = DefinedGenerics.Data[i];
		AddGenericReplacement(Key, T);
	}

	auto b = MakeBuilder();
	b.printf("%s<%s>", FnSym->Name->Data, GetTypeName(NonGeneric));
	string GenericName = MakeString(b);

	NewFnNode->Fn.Flags = NewFnNode->Fn.Flags & ~SymbolFlag_Generic;
	NewFnNode->Fn.Name = DupeType(GenericName, string);
	NewFnNode->Fn.TypeIdx = NonGeneric;

	// @THREADING
	symbol *NewSym = CreateFunctionSymbol(FnSym->Checker, NewFnNode);
	FnSym->Generated.Push(generic_generated{.T = NonGeneric, .S = NewSym});
	bool Success = FnSym->Checker->Module->Globals.Add(GenericName, NewSym);
	if(!Success)
	{
		symbol *s = FnSym->Checker->Module->Globals[GenericName];
		RaiseError(true, *Call->ErrorInfo, "--- COMPILER BUG ---\nCouldn't add generic function %s.\nTypes %s | %s", GenericName.Data, GetTypeName(NonGeneric), GetTypeName(s->Type));
	}
	Assert(Success);
	// @THREADING

	AnalyzeFunctionBody(FnSym->Checker, NewFnNode->Fn.Body, NewFnNode, NonGeneric, FnSym->Node);

	symbol *s = FnSym->Checker->Module->Globals[GenericName];
	s->Node->Fn.AlreadyAnalyzed = true;
	//NewFnNode->Fn.AlreadyAnalyzed = true;

	ClearGenericReplacement(Before);

	if(FnSym->Checker == Checker)
	{
		Checker->Scope = SaveScopes;
	}

	SetBonusMessage(LastBonusMessage);
	FnSym->Checker->Nodes->Push(NewFnNode);
	return NewSym;
}

slice<struct_member> ResolveGenericStruct(const type *StructT, slice<struct_generic_argument> GenArgs, dict<u32> *ParameterTypes)
{
	array<struct_member> Members(StructT->Struct.Members.Count);
	ForArray(Idx, StructT->Struct.Members)
	{
		auto m = StructT->Struct.Members[Idx];
		if(IsGeneric(m.Type))
		{
			const type *G = GetTypeRaw(GetGenericPart(m.Type, m.Type));
			Assert(G->Kind == TypeKind_Generic);

			u32 Resolved = INVALID_TYPE;
			int i = 0;
			if(ParameterTypes && ParameterTypes->Contains(G->Generic.Name))
			{
				Resolved = ParameterTypes->operator[](G->Generic.Name);
			}
			if(Resolved == INVALID_TYPE)
			{
				For(StructT->Struct.GenericArguments)
				{
					if(it->Name == G->Generic.Name)
					{
						Resolved = GenArgs[i].DefinedAs;
						break;
					}
					i++;
				}
			}
			Assert(Resolved != INVALID_TYPE);
			m.Type = ToNonGeneric(m.Type, Resolved);
		}
		Members[Idx] = m;
	}

	return SliceFromArray(Members);
}

