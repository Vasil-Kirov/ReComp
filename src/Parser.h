#pragma once
#include "ConstVal.h"
#include "Dynamic.h"
#include "DynamicLib.h"
#include "Lexer.h"
#include "Module.h"
#include "VString.h"
struct type;
struct function;

enum class reserved
{
	Null,
	True,
	False,
	Inf,
	NaN,
};

enum class for_type
{
	C,
	It,
	While,
	Infinite,
};

enum node_type
{
	AST_INVALID,
	AST_NOP,
	AST_CHARLIT,
	AST_CONSTANT,
	AST_BINARY,
	AST_UNARY,
	AST_IF,
	AST_IFX,
	AST_FOR,
	AST_ID,
	AST_DECL,
	
	AST_PTRTYPE,
	AST_ARRAYTYPE,
	AST_GENSTRUCTTYPE,
	AST_FN,
	AST_GENERIC,
	AST_TYPEINFO,

	AST_CALL,
	AST_RETURN,
	AST_CAST,
	AST_TYPELIST,
	AST_INDEX,
	AST_STRUCTDECL,
	AST_ENUM,
	AST_SELECTOR,
	AST_SIZE,
	AST_TYPEOF,
	AST_RESERVED,
	AST_BREAK,
	AST_LISTITEM,
	AST_SWITCH,
	AST_CASE,
	AST_DEFER,
	AST_SCOPE,
	AST_CONTINUE,
	AST_PTRDIFF,
	AST_LIST,
	AST_VAR,

	AST_EMBED,
	AST_ASSERT,
	AST_USING,
	AST_YIELD,
	AST_RUN,
	AST_FILE_LOCATION,
};

struct node
{
	node_type Type;
	union
	{
		struct {
			const string *Name;
			u32 Type; // Only set if it's a type id by the semantic analyzer
		} ID;
		struct {
			node *Expr;
			node *True;
			node *False;
			u32 TypeIdx; // Set by semantic analyzer
		} IfX;
		struct {
			slice<node *> Body;
			u32 TypeIdx;
			b32 IsExprRun;
		} Run;
		struct {
			node *Expr;
			u32 TypeIdx; // Set by semantic analyzer
		} TypedExpr;
		struct {
			node *Expr;
		} Assert;
		struct {
			const string *Name;
			node *TypeNode;
			node *Default;
			u32 Type; // Set by semantic analyzer
			b32 IsAutoDefineGeneric; // Set by semantic analyzer
		} Var;
		struct {
			slice<node *> Nodes;
			slice<u32> Types; // Set by semantic analyzer
			u32 WholeType; // Set by semantic analyzer
		} List;
		struct {
			node *Left;
			node *Right;
			u32 Type; // Set by semantic analyzer
		} PtrDiff;
		struct {

		} Continue;
		struct {
			b32 IsUp; // is {
		} ScopeDelimiter;
		struct {
			slice<node *> Body;
		} Defer;
		struct {
			reserved ID;
			u32 Type; // Set by semantic analyzer
		} Reserved;
		struct {
			b32 IsString;
			const string *FileName;
			string Content; // Set by semantic analyzer
		} Embed;
		struct {
			const string *Name;
		} Generic;
		struct {
			const string *Name;
			slice<node *> Items;
			node *Type;
		} Enum;
		struct {
			const string *Name; // @Nullable
			node *Expression;
		} Item;
		struct {
			node *Expression;
			slice<node *> Cases;
			u32 SwitchType; // Set by semantic analyzer
			u32 ReturnType; // Set by semantic analyzer
		} Switch;
		struct {
			node *Value;
			slice<node *> Body;
		} Case;
		struct {
			node *TypeNode;
			slice<node *> Items;
			u32 Type;    // Set by semantic analyzer
		} TypeList;
		struct {
			node *Expression;
			u32 Type; // Set by semantic analyzer
		} Size;
		struct {
			node *Expression;
			u32 Type; // Set by semantic analyzer
		} TypeOf;
		struct {
			const string *Name;
			slice<node *> Members;
			slice<string> TypeParams;
			b32 IsUnion;
		} StructDecl;
		struct {
			node *Expression;
			u32 Type; // Set by semantic analyzer
		} TypeInfoLookup;
		struct {
			node *Operand;
			const string *Member;
			u32 Index;
			u32 SubIndex;
			u32 Type;
		} Selector;
		struct {
			node *Operand;
			token_type Op;
			u32 Type; // Set by semantic analyzer
		} Unary;
		struct {
			node *Left;
			node *Right;
			u32 ExpressionType;
			token_type Op;
		} Binary;
		struct {
			node *Expression;
			dynamic<node *>Body;
			dynamic<node *>Else;
		} If;
		struct {
			node *Expr1; // @Nullable
			node *Expr2; // @Nullable
			node *Expr3; // @Nullable
			dynamic<node *>Body;
			for_type Kind;
			// The rest are for iterator expressions
			u32 ArrayType;
			u32 ItType; // Set by semantics analyzer
			b32 ItByRef;
		} For;
		struct {
			node *Operand;
			node *Expression;
			u32 OperandType;  // Set by semantic analyzer
			u32 IndexedType;  // Set by semantic analyzer
			u32 IndexExprType;// Set by semantic analyzer
			b32 ForceNotLoad; // Set by semantic analyzer
		} Index;
		struct {
			node *Expression;
			node *TypeNode; // @Nullable, if this is an explicit cast, it's written by the parser
			b32 IsBitCast;
			u32 FromType;
			u32 ToType;
		} Cast;
		struct {
			u32 C;
		} CharLiteral;
		struct {
			const_value Value;
			u32 Type;
		} Constant;
		struct {
			const string *LinkName; // Can only be non null on single global decls
			node *LHS;
			node *Expression; // NULL in fn args
			node *Type; // @Nullable
			u32 TypeIndex; // Set by semantic analyzer
			u32 Flags;
		} Decl;
		struct {
			const string *Name;
			const string *LinkName; // @Nullable til semantic analysis
			slice<node *> Args;
			slice<node *>ReturnTypes; // @Nullable
			dynamic<node *> Body; // @Note: call IsValid to check if the function has a body
			struct module *FnModule;
			function *IR; // @Nullable
			node *ProfileCallback;
			u32 CallbackType;
			u32 TypeIdx; // Set by semantic analyzer
			u32 Flags;
			b32 AlreadyAnalyzed;
		} Fn; // Used for fn type and fn declaration as it's the same thing
		struct {
			node *Fn;
			slice<node *> Args;
			string SymName; // Set by semantic analyzer if calling an intrinsic
			u32 Type; // Set by semantic analyzer
			slice<u32> ArgTypes; // Set by semantic analyzer
		} Call;
		struct {
			node *Type;
			node *Expression;
			u32 Analyzed; // Set by semantic analyzer
		} ArrayType;
		struct {
			node *ID;
			slice<node*> Args;
			u32 Analyzed;
		} GenericStructType;
		struct {
			node *Pointed;
			u32 Flags;
			u32 Analyzed; // Set by semantic analyzer
		} PointerType;
		struct {
			node *Expression;
			u32 TypeIdx; // Set by semantic analyzer, used by ir generator
		} Return;
	};
	const error_info *ErrorInfo;
};

struct parser
{
	dynamic<needs_resolving_import> Imported;
	dynamic<string> ConfigIDs;
	dynamic<DLIB> LoadedDynamicLibs;
	string ModuleName;
	token *Tokens;
	token *Current;
	u64 TokenIndex;
	b32 CurrentlyPublic;
	b32 NoStructLists;
	b32 NoItemLists;
	uint ScopeLevel;
};

struct parse_result
{
	string ModuleName;
	dynamic<node *> Nodes;
	slice<needs_resolving_import> Imports;
	slice<DLIB> DynamicLibraries;
	file *File;
};

node *AllocateNode(const error_info *ErrorInfo, node_type Type);
node *ParseNode(parser *Parser, b32 ExpectSemicolon=true);
node *ParseUnary(parser *Parser);
node *ParseExpression(parser *Parser);
node *ParseFunctionType(parser *Parser);
node *MakeSelector(const error_info *ErrorInfo, node *Operand, const string *Member);
node *MakeCast(const error_info *ErrorInfo, node *Expression, node *TypeNode, u32 FromType, u32 ToType);
node *MakeFunction(const error_info *ErrorInfo, const string *LinkName, slice<node *> Args, slice<node *> ReturnTypes, u32 Flags);
node *MakeDecl(const error_info *ErrorInfo, const string *ID, node *Expression, node *MaybeType, u32 Flags);
node *MakeBinary(const error_info *ErrorInfo, node *Left, node *Right, token_type Op);
node *MakeReserve(const error_info *ErrorInfo, reserved ID);
node *MakeIndex(const error_info *ErrorInfo, node *Operand, node *Expression);
node *MakeID(const error_info *ErrorInfo, const string *ID);
node *MakeReturn(const error_info *ErrorInfo, node *Expression);
node *MakeUnary(const error_info *ErrorInfo, node *Operand, token_type Op);
node *MakeConstant(const error_info *ErrorInfo, const_value Value);
node *MakePointerType(const error_info *ErrorInfo, node *Pointed);
node *MakePointerDiff(const error_info *ErrorInfo, node *Left, node *Right, u32 Type);
node *ParseTopLevel(parser *Parser);
node *ParseType(parser *Parser, b32 ShouldError = true);
node *CopyASTNode(node *N);
string *StructToModuleNamePtr(string &StructName, string &ModuleName);
string StructToModuleName(string &StructName, string &ModuleName);
bool IsOpAssignment(token_type Op);
string MakeLambdaName(const error_info *Info);
parse_result ParseTokens(file *F, slice<string> ConfigIDs);

// @NOTE: USE THE MACRO DON'T TRY TO TAKE THE POINTERS CUZ YOU MIGHT TAKE A STACK POINTER AND THEN IT GET UUUGLY
#define ERROR_INFO error_info *ErrorInfo = &Parser->Tokens[Parser->TokenIndex].ErrorInfo

