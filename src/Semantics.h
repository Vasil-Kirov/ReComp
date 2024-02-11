#pragma once
#include "Basic.h"
#include "Parser.h"
#include "Type.h"

struct local
{
	const string *Name;
	const type   *Type;
	u32 Hash;
	u32 Depth;
	b32 IsConst;
};

struct type_table
{
	const type **Types;
	int TypeCount;
};

struct checker
{
	local *Locals;
	u32 LocalCount;
	type_table TypeTable;
	u32 CurrentDepth;
};

struct locals_for_next_scope
{
	const string *ID;
	const type *Type;
	const error_info *ErrorInfo;
};

void Analyze(const node **Nodes);
void AnalyzeNode(checker *Checker, node *Node);
void AddVariable(checker *Checker, const error_info *ErrorInfo, const type *Type, const string *ID, b32 IsShadow,
		b32 IsConst);

