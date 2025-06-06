#pragma once
#include "Basic.h"
#include "Dict.h"
#include "Module.h"
#include "Parser.h"
#include "Type.h"
#include "Stack.h"

enum SymbolFlag
{
	SymbolFlag_None = 0,
	SymbolFlag_Public  = BIT(0),
	SymbolFlag_Const   = BIT(1),
	SymbolFlag_Shadow  = BIT(2),
	SymbolFlag_Function= BIT(3),
	SymbolFlag_Foreign = BIT(4),
	SymbolFlag_VarFunc = BIT(5),
	SymbolFlag_Generic = BIT(6),
	SymbolFlag_Intrinsic = BIT(7),
	SymbolFlag_Extern  = BIT(8),
	SymbolFlag_Inline  = BIT(9),
};

struct generic_generated
{
	u32 T;
	symbol *S;
};

struct symbol
{
	const string *Name;
	const string *LinkName;
	node *Node;
	checker *Checker;
	dynamic<generic_generated> Generated;
	u32 Type;
	u32 Flags;
	u32 Register;
};

struct scope
{
	node *ScopeNode;
	scope *Parent;
	dict<symbol> Symbols;
};

struct checker
{
	stack<scope *> Scope;
	stack<scope *> OutOfRun;
	module *Module;
	slice<import> Imported;
	stack<u32 *> UntypedStack;
	stack<u32> AutoEnum;
	dynamic<node *> *Nodes;
	slice<u32> CurrentFnReturnTypeIdx;
	string File;
	u32 YieldT;
	node **YieldExpr;
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
b32 IsScopeInOrEq(scope *SearchingFor, scope *S);
string MakeNonGenericName(string GenericName);
void AnalyzeInnerBody(checker *Checker, slice<node *> Body);
u32 AnalyzeBooleanExpression(checker *Checker, node **NodePtr);
void AnalyzeStructDeclaration(checker *Checker, node *Node);
void AnalyzeForUserDefinedTypes(checker *Checker, slice<node *> Nodes);
bool CheckIntrinsic(string Name);
symbol *FindSymbolFromNode(checker *Checker, node *Node, module **OutModule = NULL);
u32 GetTypeFromTypeNode(checker *Checker, node *TypeNode, b32 Error=true, b32 *OutIsAutoDefineGeneric=NULL);
void FillUntypedStack(checker *Checker, u32 Type);
void AnalyzeFunctionBody(checker *Checker, dynamic<node *> &Body, node *FnNode, u32 FunctionTypeIdx, node *ScopeNode = NULL);
symbol *CreateFunctionSymbol(checker *Checker, node *Node);
void AnalyzeForModuleStructs(slice<node *>Nodes, module *Module);
void AnalyzeEnumDefinitions(slice<node *> Nodes, module *Module);
void AnalyzeForUserDefinedTypes(checker *Checker, slice<node *> Nodes);
void AnalyzeDefineStructs(checker *Checker, slice<node *> Nodes);
void CheckForRecursiveStructs(checker *Checker, slice<node *> Nodes);
void AnalyzeEnums(checker *Checker, slice<node *> Nodes);
void AnalyzeFillStructCaches(checker *Checker, slice<node *> Nodes);
void AnalyzeFunctionDecls(checker *Checker, dynamic<node *> *NodesPtr, module *ThisModule);

void Analyze(checker *Checker, dynamic<node *> &Nodes);

