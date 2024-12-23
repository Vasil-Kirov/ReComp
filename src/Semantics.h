#pragma once
#include "Basic.h"
#include "Dict.h"
#include "Module.h"
#include "Parser.h"
#include "Type.h"
#include "Stack.h"

enum SymbolFlag
{
	SymbolFlag_Public  = BIT(0),
	SymbolFlag_Const   = BIT(1),
	SymbolFlag_Shadow  = BIT(2),
	SymbolFlag_Function= BIT(3),
	SymbolFlag_Foreign = BIT(4),
	SymbolFlag_VarFunc = BIT(5),
	SymbolFlag_Generic = BIT(6),
	SymbolFlag_Intrinsic = BIT(7),
	SymbolFlag_Extern  = BIT(8),
};

struct symbol
{
	const string *Name;
	const string *LinkName;
	node *Node;
	checker *Checker;
	u32 Type;
	u32 Flags;
	u32 IRRegister;
};

struct scope
{
	node *ScopeNode;
	scope *Parent;
	uint LastGeneric;
	dict<symbol> Symbols;
};

struct checker
{
	stack<scope *> Scope;
	module *Module;
	slice<import> Imported;
	stack<u32 *> UntypedStack;
	dynamic<node *> *Nodes;
	dynamic<node *> GeneratedGlobalNodes;
	slice<u32> CurrentFnReturnTypeIdx;
};

void AddFunctionToModule(checker *Checker, node *FnNode);
node *FindFunction(checker *Checker, string *Name);
void AnalyzeNode(checker *Checker, node *Node);
void AddVariable(checker *Checker, const error_info *ErrorInfo, u32 Type, const string *ID, node *Node, u32 Flags);
u32 AnalyzeExpression(checker *Checker, node *Expr);
symbol *AnalyzeFunctionDecl(checker *Checker, node *Node);
typedef struct {
	u32 From;
	u32 To;
	const type *FromT;
	const type *ToT;
} promotion_description;

promotion_description PromoteType(const type *Promotion, const type *Left, const type *Right, u32 LeftIdx, u32 RightIdx);
u32 TypeCheckAndPromote(checker *Checker, const error_info *ErrorInfo, u32 Left, u32 Right, node **LeftNode, node **RightNode, const char *ErrorFmt);
scope *AllocScope(node *Node, scope *Parent = NULL);
b32 ScopesMatch(scope *A, scope *B);
void CheckBodyForUnreachableCode(slice<node *> Body);
node *AnalyzeGenericExpression(checker *Checker, node *Generic, string *IDOut);
b32 IsScopeInOrEq(scope *SearchingFor, scope *S);
string MakeNonGenericName(string GenericName);
void AnalyzeInnerBody(checker *Checker, slice<node *> Body);
u32 AnalyzeBooleanExpression(checker *Checker, node **NodePtr);


