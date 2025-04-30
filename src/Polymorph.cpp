#include "Polymorph.h"
#include "VString.h"
#include "Semantics.h"
#include "Parser.h"
#include "Memory.h"

u32 FunctionTypeGetNonGeneric(const type *Old, dict<u32> DefinedGenerics, node *Call)
{
	type *NewFT = AllocType(TypeKind_Function);
	NewFT->Function.Flags = Old->Function.Flags & ~SymbolFlag_Generic;
	NewFT->Function.ArgCount = Old->Function.ArgCount;
	NewFT->Function.Args = (u32 *)AllocatePermanent(sizeof(u32) * Old->Function.ArgCount);
	for(int ArgI = 0; ArgI < Old->Function.ArgCount; ++ArgI)
	{
		u32 TypeIdx = Old->Function.Args[ArgI];
		const type *T = GetType(TypeIdx);
		if(IsGeneric(T))
		{
			const type *G = GetTypeRaw(GetGenericPart(TypeIdx, TypeIdx));
			u32 Resolved = DefinedGenerics[G->Generic.Name];
			TypeIdx = ToNonGeneric(TypeIdx, Resolved, Call->Call.ArgTypes[ArgI]);
		}
		NewFT->Function.Args[ArgI] = TypeIdx;
	}

	array<u32> Returns(Old->Function.Returns.Count);
	uint At = 0;
	For(Old->Function.Returns)
	{
		if(IsGeneric(*it))
		{
			const type *G = GetTypeRaw(GetGenericPart(*it, *it));
			u32 Resolved = DefinedGenerics[G->Generic.Name];
			Returns[At++] = ToNonGeneric(*it, Resolved, *it);
		}
		else
		{
			Returns[At++] = *it;
		}
	}
	NewFT->Function.Returns = SliceFromArray(Returns);

	
	return AddType(NewFT);
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

				u32 DefineType = GetGenericPart(ExprTypeIdx, ArgTypeIdx);
				if(!DefinedGenerics.Add(GenT->Generic.Name, DefineType))
				{
					// @Note: Unreachable?
					RaiseError(true, *FnSym->Node->Fn.Args[ArgI]->ErrorInfo,
							"Redefined polymorphic type %s", Arg.Name->Data);
				}
			}
		}
	}

	for(int ArgI = 0; ArgI < FnT->Function.ArgCount; ++ArgI)
	{
		auto err_i = Call->Call.Args[ArgI]->ErrorInfo;
		u32 ArgTypeIdx = FnT->Function.Args[ArgI];
		const type *T = GetType(ArgTypeIdx);
		if(IsGeneric(T))
		{
			u32 GenericIdx = GetGenericPart(ArgTypeIdx, ArgTypeIdx);
			const type *GenT = GetTypeRaw(GenericIdx);
			u32 *DefinedPtr = DefinedGenerics.GetUnstablePtr(GenT->Generic.Name);
			if(DefinedPtr == NULL)
			{
				// @Note: probably reachable
				RaiseError(true, *err_i, "Parameter type is an unresolved polymorphic type");
			}
			else
			{
				u32 Defined = *DefinedPtr;
				u32 DefinedArg = ToNonGeneric(ArgTypeIdx, Defined, ArgTypeIdx);
				TypeCheckAndPromote(Checker, err_i, DefinedArg, Call->Call.ArgTypes[ArgI], NULL,
						&Call->Call.Args.Data[ArgI], "Polymorphic argument resolves to type %s but expression is of incompatible type %s");
			}
		}
	}

	For(FnT->Function.Returns)
	{
		if(IsGeneric(*it))
		{
			auto err_i = Call->ErrorInfo;
			u32 GenericIdx = GetGenericPart(*it, *it);
			const type *GenT = GetTypeRaw(GenericIdx);
			u32 *DefinedPtr = DefinedGenerics.GetUnstablePtr(GenT->Generic.Name);
			if(DefinedPtr == NULL)
			{
				// @Note: probably reachable
				RaiseError(true, *err_i, "return of function is an unresolved polymorphic type");
			}
		}
	}

	u32 NonGeneric = FunctionTypeGetNonGeneric(FnT, DefinedGenerics, Call);
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
	AnalyzeFunctionBody(FnSym->Checker, NewFnNode->Fn.Body, NewFnNode, NonGeneric, FnSym->Node);
	NewFnNode->Fn.AlreadyAnalyzed = true;

	ClearGenericReplacement(Before);

	if(FnSym->Checker == Checker)
	{
		Checker->Scope = SaveScopes;
	}

	SetBonusMessage(LastBonusMessage);
	// @THREADING
	symbol *NewSym = CreateFunctionSymbol(FnSym->Checker, NewFnNode);
	FnSym->Generated.Push(generic_generated{.T = NonGeneric, .S = NewSym});
	bool Success = FnSym->Checker->Module->Globals.Add(*NewFnNode->Fn.Name, NewSym);
	if(!Success)
	{
		symbol *s = FnSym->Checker->Module->Globals[*NewFnNode->Fn.Name];
		RaiseError(true, *Call->ErrorInfo, "--- COMPILER BUG ---\nCouldn't add generic function %s.\nTypes %s | %s", NewFnNode->Fn.Name->Data, GetTypeName(NonGeneric), GetTypeName(s->Type));
	}
	Assert(Success);
	// @THREADING
	FnSym->Checker->Nodes->Push(NewFnNode);
	return NewSym;
}

