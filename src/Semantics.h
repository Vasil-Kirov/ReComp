#pragma once
#include "Basic.h"
#include "Parser.h"
#include "Type.h"

struct local
{
	const string *Name;
	u32 Type;
	u32 Hash;
	u32 Depth;
	b32 IsConst;
};

struct checker
{
	local *Locals;
	u32 LocalCount;
	u32 CurrentDepth;
	u32 CurrentFnReturnTypeIdx;
};

struct locals_for_next_scope
{
	const string *ID;
	const error_info *ErrorInfo;
	u32 Type;
};

void Analyze(const node **Nodes);
u32 AnalyzeNode(checker *Checker, node *Node);
void AddVariable(checker *Checker, const error_info *ErrorInfo, u32 Type, const string *ID, b32 IsShadow,
		b32 IsConst);
