#pragma once
#include "Lexer.h"
struct type;

enum node_type
{
	AST_INVALID,
	AST_NUMBER,
	AST_BINARY,
	AST_UNARY,
	AST_IF,
	AST_FUNCTION,
	AST_ID,
	AST_STRING,
	AST_DECL,
	
	AST_BASICTYPE,
	AST_PTRTYPE,
	AST_FN,
	
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
			node *TypeNode; // @Nullable, if this is an explicit cast, it's written by the parser
			u32 FromType;
			u32 ToType;
		} Cast;
		struct {
			const string *S;
		} String;
		struct {
			u64 Bytes;
			b32 IsFloat;
		} Number;
		struct {
			node *ID;
			node *Expression; // NULL in fn args
			node *Type; // @Nullable
			u32 TypeIndex; // Set by semantic analyzer, used by ir generator
			b32 IsConst;
			b32 IsShadow;
		} Decl;
		struct {
			node **Args;
			node *ReturnType; // @Nullable
			node **Body; // @Nullable // @Note: Dynamic array
		} Fn; // Used for fn type and fn declaration as it's the same thing
		struct {
			node *ID;
		} BasicType;
		struct {
			node *Pointed;
		} PointerType;
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

#define ERROR_INFO error_info *ErrorInfo = &Parser->Tokens[Parser->TokenIndex].ErrorInfo

