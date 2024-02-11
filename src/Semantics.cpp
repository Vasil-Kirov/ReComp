#include "Semantics.h"
extern const type BasicTypes[];
extern const int  BasicTypesCount;

extern const type *BasicBool;
extern const type *UntypedInteger;
extern const type *UntypedFloat;
extern const type *BasicUint;
extern const type *BasicInt;
extern const type *BasicF32;

uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed);
uint32_t murmur3_32(const char* key, size_t len, uint32_t seed)
{
	return murmur3_32((uint8_t *)key, len, seed);
}

const int HASH_SEED = 0;

void RaiseBinaryTypeError(const error_info *ErrorInfo, const type *Left, const type *Right)
{
	RaiseError(*ErrorInfo, "Incompatible types in binary expression: %s and %s", GetTypeName(Left), GetTypeName(Right));
}

const type *FindType(checker *Checker, const string *Name)
{
	FOR_ARRAY(Checker->TypeTable.Types, Checker->TypeTable.TypeCount)
	{
		switch((*It)->Kind)
		{
			case TypeKind_Basic:
			{
				if(*Name == (*It)->Basic.Name)
				{
					return *It;
				}
			} break;
			default:
			{
				Assert(false);
			} break;
		}
	}
	return NULL;
}

const type *GetTypeFromTypeNode(checker *Checker, node *TypeNode)
{
	if(TypeNode == NULL)
		return NULL;

	switch(TypeNode->Type)
	{
		case AST_BASICTYPE:
		{
			const string *Name = TypeNode->BasicType.ID->ID.Name;
			const type *Type = FindType(Checker, Name);
			if(Type == NULL)
			{
				RaiseError(*TypeNode->ErrorInfo, "Type \"%s\" is not defined", Name->Data);
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

b32 IsVariableConst(checker *Checker, const string *ID)
{
	int FirstAtRightDepth = -1;
	for(int I = 0; I < Checker->LocalCount; ++I)
	{
		if(Checker->CurrentDepth == Checker->Locals[I].Depth)
		{
			FirstAtRightDepth = I;
			break;
		}
	}

	const local *Found = NULL;
	if(FirstAtRightDepth != -1)
	{
		u32 Hash = murmur3_32(ID->Data, ID->Size, HASH_SEED);
		for(int I = FirstAtRightDepth; I < Checker->LocalCount; ++I)
		{
			const local *Local = &Checker->Locals[I];
			if(Hash == Local->Hash && *ID == *Local->Name)
				Found = Local;
		}
	}

	// @Note: can never get here since the variable has already been checked
	return Found->IsConst;
}

b32 IsLHSAssignable(checker *Checker, node *LHS)
{
	switch(LHS->Type)
	{
		case AST_ID:
		{
			b32 Found = true;
			return !IsVariableConst(Checker, LHS->ID.Name);
		} break;
		default:
		{
			return false;
		};
	}
}

const type *GetVariable(checker *Checker, const string *ID)
{
	int FirstAtRightDepth = -1;
	for(int I = 0; I < Checker->LocalCount; ++I)
	{
		if(Checker->CurrentDepth == Checker->Locals[I].Depth)
		{
			FirstAtRightDepth = I;
			break;
		}
	}
	if(FirstAtRightDepth == -1)
		return NULL;

	// @Note: find the last instance of the variable (for shadowing)
	const local *Found = NULL;
	const local *Locals = Checker->Locals;
	u32 LocalCount = Checker->LocalCount;
	u32 Hash = murmur3_32(ID->Data, ID->Size, HASH_SEED);
	for(int I = FirstAtRightDepth; I < LocalCount; ++I)
	{
		const local *Local = &Locals[I];
		if(Hash == Local->Hash && *ID == *Local->Name)
			Found = Local;
	}

	if(Found)
		return Found->Type;
	return NULL;
}


const int MAX_ARGS = 512;
locals_for_next_scope LocalsNextScope[MAX_ARGS];
int LocalNextCount = 0;

const type *CreateFunctionType(checker *Checker, node *FnNode)
{
	type *NewType = NewType(type);
	NewType->Kind = TypeKind_Function;
	function_type Function;
	Function.Return = GetTypeFromTypeNode(Checker, FnNode->Fn.ReturnType);
	Function.ArgCount = ArrLen(FnNode->Fn.Args);
	Function.Args = (const type **)AllocatePermanent(sizeof(type *) * Function.ArgCount);

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
	Checker->TypeTable.Types[Checker->TypeTable.TypeCount++] = NewType;
	return NewType;
}

const type *GetBasicTypeFromKind(checker *Checker, basic_kind Kind)
{
	const type_table *TypeTable = &Checker->TypeTable;
	for(int I = 0; I < TypeTable->TypeCount; ++I)
	{
		if(TypeTable->Types[I]->Kind == TypeKind_Basic && TypeTable->Types[I]->Basic.Kind == Kind)
			return TypeTable->Types[I];
	}

	Assert(false);
	return NULL;
}

void PopScope(checker *Checker)
{
	Checker->CurrentDepth--;
	for(int I = 0; I < Checker->LocalCount; ++I)
	{
		if(Checker->Locals[I].Depth > Checker->CurrentDepth)
		{
			Checker->LocalCount = I;
			break;
		}
	}
}

void AnalyzeBody(checker *Checker, node **Body)
{
	Checker->CurrentDepth++;
	for(int I = 0; I < LocalNextCount; ++I)
	{
		locals_for_next_scope Local = LocalsNextScope[I];
		AddVariable(Checker, Local.ErrorInfo, Local.Type, Local.ID, false, true);
	}
	LocalNextCount = 0;

	int BodyCount = ArrLen(Body);
	for(int I = 0; I < BodyCount; ++I)
	{
		AnalyzeNode(Checker, Body[I]);
	}
	PopScope(Checker);
}

const type *AnalyzeAtom(checker *Checker, node *Expr)
{
	switch(Expr->Type)
	{
		case AST_ID:
		{
			const type *Type = GetVariable(Checker, Expr->ID.Name);
			if(Type == NULL)
			{
				RaiseError(*Expr->ErrorInfo, "Refrenced variable %s is not declared", Expr->ID.Name->Data);
			}
			return Type;
		} break;
		case AST_FN:
		{
			const type *Result = CreateFunctionType(Checker, Expr);
			if(Expr->Fn.Body)
			{
				AnalyzeBody(Checker, Expr->Fn.Body);
			}
			return Result;
		} break;
		case AST_NUMBER:
		{
			if(Expr->Number.IsFloat)
				return UntypedFloat;
			return UntypedInteger;
		} break;
		default:
		{
			Assert(false);
			return NULL;
		} break;
	}
}

const type *AnalyzeUnary(checker *Checker, node *Expr)
{
	switch(Expr->Type)
	{
		default:
		{
			return AnalyzeAtom(Checker, Expr);
		} break;
	}
}

const type *AnalyzeExpression(checker *Checker, node *Expr)
{
	if(Expr->Type == AST_BINARY)
	{
		const type *Left  = AnalyzeExpression(Checker, Expr->Binary.Left);
		const type *Right = AnalyzeExpression(Checker, Expr->Binary.Right);
		const type *Promotion = NULL;
		if(!IsTypeCompatible(Left, Right, &Promotion, false))
		{
			RaiseBinaryTypeError(Expr->ErrorInfo, Left, Right);
		}

		const type *Result = Left;
		switch(Expr->Binary.Op)
		{
			case T_PEQ:
			case T_MEQ:
			case T_DEQ:
			case T_MODEQ:
			case T_ANDEQ:
			case T_XOREQ:
			case T_OREQ:
			case T_EQ:
			{
				if(!IsLHSAssignable(Checker, Expr->Binary.Left))
					RaiseError(*Expr->ErrorInfo, "Left-hand side of assignment is not assignable");
				if(Promotion == Right)
				{
					RaiseError(*Expr->ErrorInfo, "Incompatible types in assignment expression!\n"
							"Right-hand side doesn't fit in the left-hand side");
				}
			} break;

			case T_NEQ:
			case T_GEQ:
			case T_LEQ:
			case T_EQEQ:
			case T_LOR:
			case T_LAND:
			{
				Result = BasicBool;
			} break;
			default:
			{
			} break;
		}

		if(Promotion)
		{
			if(Promotion == Left)
			{
				Expr->Binary.Right = MakeCast(Expr->ErrorInfo, Expr->Binary.Right, NULL, Promotion);
			}
			else if(Promotion == Right)
			{
				Expr->Binary.Left = MakeCast(Expr->ErrorInfo, Expr->Binary.Left,   NULL, Promotion);
			}
			else
				Assert(false);
		}
		return Result;
	}
	else
	{
		return AnalyzeUnary(Checker, Expr);
	}
}

void AddVariable(checker *Checker, const error_info *ErrorInfo, const type *Type, const string *ID, b32 IsShadow,
		b32 IsConst)
{
	u32 Hash = murmur3_32(ID->Data, ID->Size, HASH_SEED);
	if(!IsShadow)
	{
		for(int I = 0; I < Checker->LocalCount; ++I)
		{
			if(Hash == Checker->Locals[I].Hash && *ID == *Checker->Locals[I].Name)
			{
				RaiseError(*ErrorInfo,
						"Redeclaration of variable %s.\n"
						"If this is intentional mark it as a shadow like this:\n\t#shadow %s := 0;",
						ID->Data, ID->Data);
			}
		}
	}
	local Local;
	Local.Hash    = Hash;
	Local.Depth   = Checker->CurrentDepth;
	Local.Name    = ID;
	Local.Type    = Type;
	Local.IsConst = IsConst;

	Checker->Locals[Checker->LocalCount++] = Local;
}

void AnalyzeDeclerations(checker *Checker, node *Node)
{
	Assert(Node->Type == AST_DECL);
	const node *ID = Node->Decl.ID;
	const type *Type = GetTypeFromTypeNode(Checker, Node->Decl.Type);
	if(Node->Decl.Expression)
	{
		const type *ExprType = AnalyzeExpression(Checker, Node->Decl.Expression);
		if(Type != NULL)
		{
			if(!IsTypeCompatible(Type, ExprType, NULL, true))
			{
				RaiseError(*Node->ErrorInfo, "Cannot assign expression of type %s to variable of type %s",
						GetTypeName(ExprType), GetTypeName(Type));
			}
		}
		else
		{
			Type = ExprType;
		}
	}
	else
	{
		if(Type == NULL)
		{
			RaiseError(*Node->ErrorInfo, "Expected either type or expression in variable declaration");
		}
	}
	AddVariable(Checker, Node->ErrorInfo, Type, ID->ID.Name, Node->Decl.IsShadow, Node->Decl.IsConst);
}

void AnalyzeNode(checker *Checker, node *Node)
{
	switch(Node->Type)
	{
		case AST_DECL:
		{
			AnalyzeDeclerations(Checker, Node);
		} break;
		default:
		{
			AnalyzeExpression(Checker, Node);
		} break;
	}
}

size_t MAX_TYPES = MB(1);
type_table CreateTypeTable()
{
	type_table Result;
	Result.TypeCount = 0;
	Result.Types = (const type **)AllocateVirtualMemory(sizeof(type *) * MAX_TYPES);
	for(int I = 0; I < BasicTypesCount; ++I)
	{
		Result.Types[Result.TypeCount++] = &BasicTypes[I];
	}

	return Result;
}

void Analyze(node **Nodes)
{
	checker Checker;
	Checker.Locals = (local *)AllocateVirtualMemory(MB(1) * sizeof(local));
	Checker.LocalCount = 0;
	Checker.CurrentDepth = 0;
	Checker.TypeTable = CreateTypeTable();

	int NodeCount = ArrLen(Nodes);
	for(int I = 0; I < NodeCount; ++I)
	{
		AnalyzeNode(&Checker, Nodes[I]);
	}

	FreeVirtualMemory(Checker.Locals);
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

