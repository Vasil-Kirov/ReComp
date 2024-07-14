#pragma once
#include "ConstVal.h"
#include "Dynamic.h"
#include "Lexer.h"
#include "VString.h"
struct type;

enum node_type
{
	AST_INVALID,
	AST_CONSTANT,
	AST_BINARY,
	AST_UNARY,
	AST_IF,
	AST_FOR,
	AST_FUNCTION,
	AST_ID,
	AST_DECL,
	
	AST_BASICTYPE,
	AST_PTRTYPE,
	AST_FN,
	AST_CALL,
	AST_RETURN,
	
	AST_CAST,
};

struct node
{
	node_type Type;
	union
	{
		struct {
			const string *Name;
		} ID;
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
			node *Init; // @Nullable
			node *Expr; // @Nullable
			node *Incr; // @Nullable
			dynamic<node *>Body;
		} For;
		struct {
			node *Expression;
			node *TypeNode; // @Nullable, if this is an explicit cast, it's written by the parser
			u32 FromType;
			u32 ToType;
		} Cast;
		struct {
			const_value Value;
			u32 Type;
		} Constant;
		struct {
			node *ID;
			node *Expression; // NULL in fn args
			node *Type; // @Nullable
			u32 TypeIndex; // Set by semantic analyzer, used by ir generator
			b32 IsConst;
			b32 IsShadow;
		} Decl;
		struct {
			const string *Name;
			slice<node *> Args;
			node *ReturnType; // @Nullable
			dynamic<node *> Body; // @Note: call IsValid to check if the function has a body
			u32 TypeIdx; // Set by semantic analyzer, used by ir generator
		} Fn; // Used for fn type and fn declaration as it's the same thing
		struct {
			node *Fn;
			slice<node *> Args;
			const string *SymName; // Set by semantic analyzer if not calling a function pointer
			u32 Type; // Set by semantic analyzer
		} Call;
		struct {
			node *ID;
		} BasicType;
		struct {
			node *Pointed;
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
	token *Tokens;
	u64 TokenIndex;
	b32 IsInBody;
};

node **ParseTokens(token *Tokens);
node *ParseNode(parser *Parser);
node *ParseExpression(parser *Parser);
node *ParseFunctionType(parser *Parser);
node *MakeCast(const error_info *ErrorInfo, node *Expression, node *TypeNode, u32 FromType, u32 ToType);
node *MakeBinary(const error_info *ErrorInfo, node *Left, node *Right, token_type Op);
node *ParseTopLevel(parser *Parser);

// @NOTE: USE THE MACRO DON'T TRY TO TAKE THE POINTERS CUZ YOU MIGHT TAKE A STACK POINTER AND THEN IT GET UUUGLY
#define ERROR_INFO error_info *ErrorInfo = &Parser->Tokens[Parser->TokenIndex].ErrorInfo

