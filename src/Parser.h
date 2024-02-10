#pragma once
#include "Lexer.h"

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
			token_type Op;
		} Binary;
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
			b32 IsConst;
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

