#pragma once
#include "Basic.h"
#include "Parser.h"
#include "Type.h"
#include "Stack.h"

enum SymbolFlag
{
	SymbolFlag_Private = 0b0000,
	SymbolFlag_Public  = 0b0001,
	SymbolFlag_Const   = 0b0010,
	SymbolFlag_Shadow  = 0b0100,
	SymbolFlag_Function= 0b1000,
};

struct symbol
{
	const string *Name;
	u32 Type;
	u32 Hash;
	u32 Depth;
	u32 Flags;
};

struct checker
{
	symbol *Symbols;
	stack<u32 *> UntypedStack;
	u32 SymbolCount;
	u32 CurrentDepth;
	u32 CurrentFnReturnTypeIdx;
};

struct locals_for_next_scope
{
	const string *ID;
	const error_info *ErrorInfo;
	u32 Type;
};

void AddFunctionToModule(checker *Checker, node *FnNode);
node *FindFunction(checker *Checker, string *Name);
void Analyze(const node **Nodes);
void AnalyzeNode(checker *Checker, node *Node);
void AddVariable(checker *Checker, const error_info *ErrorInfo, u32 Type, const string *ID, node *Node, u32 Flags);
u32 AnalyzeExpression(checker *Checker, node *Expr);
typedef struct {
	u32 From;
	u32 To;
	const type *FromT;
	const type *ToT;
} promotion_description;

promotion_description PromoteType(const type *Promotion, const type *Left, const type *Right, u32 LeftIdx, u32 RightIdx);
u32 TypeCheckAndPromote(checker *Checker, const error_info *ErrorInfo, u32 Left, u32 Right, node **LeftNode, node **RightNode);

