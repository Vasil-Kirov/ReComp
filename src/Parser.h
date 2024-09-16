#pragma once
#include "ConstVal.h"
#include "Dynamic.h"
#include "Lexer.h"
#include "VString.h"
struct type;

enum class reserved
{
	Null,
	True,
	False,
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
	AST_FOR,
	AST_ID,
	AST_DECL,
	
	AST_PTRTYPE,
	AST_ARRAYTYPE,
	AST_FN,
	AST_GENERIC,

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
	AST_MATCH,
	AST_CASE,
	AST_DEFER,
	AST_SCOPE,
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
			u32 MatchType; // Set by semantic analyzer
			u32 ReturnType; // Set by semantic analyzer
		} Match;
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
		} StructDecl;
		struct {
			node *Operand;
			const string *Member;
			u32 Index;
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
		} For;
		struct {
			node *Operand;
			node *Expression;
			u32 OperandType; // Set by semantic analyzer
			u32 IndexedType; // Set by semantic analyzer
			b32 ForceNotLoad; // Set by semantic analyzer
		} Index;
		struct {
			node *Expression;
			node *TypeNode; // @Nullable, if this is an explicit cast, it's written by the parser
			u32 FromType;
			u32 ToType;
		} Cast;
		struct {
			char C;
		} CharLiteral;
		struct {
			const_value Value;
			u32 Type;
		} Constant;
		struct {
			const string *ID;
			node *Expression; // NULL in fn args
			node *Type; // @Nullable
			u32 TypeIndex; // Set by semantic analyzer
			u32 Flags;
		} Decl;
		struct {
			const string *Name;
			slice<node *> Args;
			node *ReturnType; // @Nullable
			dynamic<node *> Body; // @Note: call IsValid to check if the function has a body
			struct module *FnModule;
			u32 TypeIdx; // Set by semantic analyzer
			u32 Flags;
			b32 AlreadyAnalyzed;
		} Fn; // Used for fn type and fn declaration as it's the same thing
		struct {
			node *Fn;
			slice<node *> Args;
			const string *SymName; // Set by semantic analyzer if not calling a function pointer
			u32 Type; // Set by semantic analyzer
			slice<u32> ArgTypes; // Set by semantic analyzer
			slice<u32> GenericTypes; // Set by semantic analyzer
		} Call;
		struct {
			node *Type;
			node *Expression;
		} ArrayType;
		struct {
			node *Pointed;
			u32 Flags;
		} PointerType;
		struct {
			node *Expression;
			u32 TypeIdx; // Set by semantic analyzer, used by ir generator
		} Return;
	};
	const error_info *ErrorInfo;
};

struct needs_resolving_import
{
	string Name;
	string As;
	error_info *ErrorInfo;
};

struct parser
{
	dynamic<needs_resolving_import> Imported;
	dynamic<string> ConfigIDs;
	string ModuleName;
	token *Tokens;
	token *Current;
	u64 TokenIndex;
	b32 CurrentlyPublic;
	b32 NoStructLists;
	uint ScopeLevel;
	uint ExpectingCloseParen;
};

struct parse_result
{
	dynamic<node *>Nodes;
	slice<needs_resolving_import> Imports;
};

node *AllocateNode(const error_info *ErrorInfo, node_type Type);
parse_result ParseTokens(token *Tokens, string ModuleName);
node *ParseNode(parser *Parser);
node *ParseUnary(parser *Parser);
node *ParseExpression(parser *Parser);
node *ParseFunctionType(parser *Parser);
node *MakeCast(const error_info *ErrorInfo, node *Expression, node *TypeNode, u32 FromType, u32 ToType);
node *MakeFunction(const error_info *ErrorInfo, slice<node *> Args, node *ReturnType, u32 Flags);
node *MakeDecl(const error_info *ErrorInfo, const string *ID, node *Expression, node *MaybeType, u32 Flags);
node *MakeBinary(const error_info *ErrorInfo, node *Left, node *Right, token_type Op);
node *MakeReserve(const error_info *ErrorInfo, reserved ID);
node *MakeIndex(const error_info *ErrorInfo, node *Operand, node *Expression);
node *MakeID(const error_info *ErrorInfo, const string *ID);
node *MakeReturn(const error_info *ErrorInfo, node *Expression);
node *MakeUnary(const error_info *ErrorInfo, node *Operand, token_type Op);
node *ParseTopLevel(parser *Parser);
node *ParseType(parser *Parser, b32 ShouldError = true);
node *CopyASTNode(node *N);
string *StructToModuleNamePtr(string &StructName, string &ModuleName);
string StructToModuleName(string &StructName, string &ModuleName);

// @NOTE: USE THE MACRO DON'T TRY TO TAKE THE POINTERS CUZ YOU MIGHT TAKE A STACK POINTER AND THEN IT GET UUUGLY
#define ERROR_INFO error_info *ErrorInfo = &Parser->Tokens[Parser->TokenIndex].ErrorInfo

