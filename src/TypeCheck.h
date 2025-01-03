#pragma once

#include "Linearize.h"
#include <Semantics.h>

enum CheckFlags: u32
{
	Check_Done = BIT(0),
	Check_NeedsUnification  = BIT(1),
	Check_TypeLiteral		= BIT(2),
	Check_NotAssignable 	= BIT(3),
};

struct unification_entry
{
	int ExprIndex;
	int Binding;
	u32 UntypedType;
};

struct checked
{
	u32 Type;
	u32 Flags;
};

struct cast_inserts
{
	int At;
	u32 To;
};

struct _symbol
{
	u32 T;
};

struct _scope
{
	dict<_symbol> Syms;
};

struct TypeCheck
{
	slice<LNode> Nodes;
	array<checked> Types;
	dynamic<unification_entry> UnificationTable;
	dynamic<cast_inserts> Casts;
	stack<_scope *> Scope;
	TypeCheck(slice<LNode> Nodes_) : Types(Nodes_.Count)
	{
		Nodes = Nodes_;
	}

	void CheckNode(int Idx);
	void DoTypeChecking();
	int AddUnificationType(u32 T, int Idx);
	void BindUnificationType(int UniIdx, int BindIdx);
	void ResolveUnification(u32 T, int ExprIdx);
	void ResolveBinding(u32 T, int Binding);
	void FillType(int Idx, u32 Type, u32 BonusFlags=0);
	u32 FindType(int Idx, u32 *Flags = NULL);
	b32 IsDone(int Idx);

	u32 CheckTypes(int Idx, u32 LT, u32 RT, b32 IsAssignment, const char *ErrorFmt, b32 RightFirst=false);
	void CheckDecl(int Idx);
	void CheckBinary(int Idx);
	void CheckUnary(int Idx, u32 T, u32 OperandFlags);
};

