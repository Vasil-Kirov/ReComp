#pragma once
#include "Lexer.h"

enum node_type
{
	AST_INVALID,
	AST_BINARY,
	AST_UNARY,
	AST_IF,
	AST_FUNCTION,
	AST_ID,
	AST_STRING,
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
			b32 IsSigned;
		} Number;
	};
	const error_info *ErrorInfo;
};

node **ParseTokens(token *Tokens);
node *ParseNode(token **Tokens);
node *ParseExpression(token **Tokens);

