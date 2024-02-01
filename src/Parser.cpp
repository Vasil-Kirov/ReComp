#include "Parser.h"
#include "Memory.h"

node *AllocateNode(error_info *ErrorInfo)
{
	node *Result = (node *)AllocatePermanent(sizeof(node));
	Result->ErrorInfo = ErrorInfo;
	return Result;
}

node *MakeBinary(error_info *ErrorInfo, node *Left, node *Right, token_type Op)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_BINARY;
	Result->Binary.Left = Left;
	Result->Binary.Right = Right;
	Result->Binary.Op = Op;
	return Result;
}

node *MakeID(error_info *ErrorInfo, const string *ID)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_ID;
	Result->ID.Name = ID;
	return Result;
}

node *MakeASTString(error_info *ErrorInfo, const string *String)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_STRING;
	Result->String.S = String;
	return Result;
}

node *MakeNumber(error_info *ErrorInfo, u64 Number, b32 IsFloat, b32 IsSigned)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Number.Bytes = Number;
	Result->Number.IsFloat = IsFloat;
	Result->Number.IsSigned = IsSigned;
	return Result;
}

token GetToken(token **Tokens)
{
	token Token = **Tokens;
	*Tokens = *Tokens + 1;
	if(Token.Type == T_EOF)
	{
		RaiseError(Token.ErrorInfo, "Unexpected End of File");
	}
	return Token;
}

token PeekToken(token **Tokens)
{
	token Token = **Tokens;
	return Token;
}

node **ParseTokens(token *Tokens)
{
	node **Result = ArrCreate(node *);
	size_t TokenCount = ArrLen(Tokens);
	for(int I = 0; I < TokenCount; ++I)
	{
		node *Node = ParseNode(&Tokens);
		ArrPush(Result, Node);
	}
	return Result;
}

node *ParseNumber(token **Tokens)
{
	token NumberToken = GetToken(Tokens);
	const string *NumberString = NumberToken.ID;

	b32 IsFloat = false;
	b32 IsSigned = false;
	for(int I = 0; I < NumberString->Size; ++I)
	{
		if(NumberString->Data[I] == '-')
			IsSigned = true;
		else if(NumberString->Data[I] == '.')
			IsFloat = true;
	}

	u64 Bytes = 0;
	const char *Start = NumberToken.ID->Data;
	if(IsFloat)
	{
		f64 Typed = strtod(Start, NULL);
		Bytes = *(u64 *)&Typed;
	}
	else if(IsSigned)
	{
		i64 Typed = strtoll(Start, NULL, 10);
		Bytes = *(u64 *)&Typed;
	}
	else
	{
		Bytes = strtoul(Start, NULL, 10);
	}
	return MakeNumber(&NumberToken.ErrorInfo, Bytes, IsFloat, IsSigned);
}

node *ParseAtom(token **Tokens, node *Operand)
{
	// @TODO: Implement
	return Operand;
}

node *ParseOperand(token **Tokens)
{
	token Token = PeekToken(Tokens);
	node *Result = NULL;
	switch((int)Token.Type)
	{
		case T_ID:
		{
			GetToken(Tokens);
			Result = MakeID(&Token.ErrorInfo, Token.ID);
		} break;
		case T_NUM:
		{
			Result = ParseNumber(Tokens);
		} break;
		case T_STR:
		{
			GetToken(Tokens);
			Result = MakeASTString(&Token.ErrorInfo, Token.ID);
		} break;
		case '(':
		{
			GetToken(Tokens);
			Result = ParseExpression(Tokens);
		} break;
	}
	return Result;
}

node *ParseUnary(token **Tokens)
{
	token Token = PeekToken(Tokens);

	node *Current = NULL;

	b32 UnaryProcessing = true;
	while(UnaryProcessing)
	{
		switch((int)Token.Type)
		{
			default:
			{
				UnaryProcessing = false;
			} break;
		}
	}

	node *Operand = ParseOperand(Tokens);

	node *Atom = ParseAtom(Tokens, Operand);
	if(!Atom)
	{
		RaiseError(Token.ErrorInfo, "Expected operand in expression");
	}
	return Atom;
}

struct precedence
{
	int Left;
	int Right;
};

precedence MakePrecedence(int Left, int Right)
{
	precedence Prec;
	Prec.Left = Left;
	Prec.Right = Right;
	return Prec;
}

precedence GetPrecedence(token_type Op)
{
	switch ((int)Op)
	{
		case '*':
		case '/':
		case '%':
			return MakePrecedence(80, 81);

		case '+':
		case '-':
			return MakePrecedence(70, 71);

		case T_SLEFT:
		case T_SRIGHT:
			return MakePrecedence(60, 61);

		case '<':
		case '>':
		case T_GEQ:
		case T_LEQ:
			return MakePrecedence(50, 51);

		case T_EQEQ:
		case T_NEQ:
			return MakePrecedence(40, 41);

		case '&':
			return MakePrecedence(30, 31);

		case '^':
			return MakePrecedence(20, 21);

		case '|':
			return MakePrecedence(10, 11);

		case T_LAND:
			return MakePrecedence(0, 1);

		case T_LOR:
			return MakePrecedence(-10, -9);

		// Non binary operator, abort the loop
		default:
			return MakePrecedence(-999, -999);
	}
}

node *ParseExpression(token **Tokens, int CurrentPrecedence)
{
	node *LHS = ParseUnary(Tokens);
	
	while(true)
	{
		token BinaryOp = PeekToken(Tokens);
		precedence Prec = GetPrecedence(BinaryOp.Type);
		int LeftP  = Prec.Left;
		int RightP = Prec.Right;
		if(LeftP < CurrentPrecedence)
			break;
		GetToken(Tokens);
		node *RHS = ParseExpression(Tokens, RightP);
		LHS = MakeBinary(&BinaryOp.ErrorInfo, LHS, RHS, BinaryOp.Type);
	}
	return LHS;
}

node *ParseExpression(token **Tokens)
{
	return ParseExpression(Tokens, 99);
}

node *ParseNode(token **Tokens)
{
	token Token = GetToken(Tokens);
	switch(Token.Type)
	{
		default:
		{
			return ParseExpression(Tokens);
		} break;
	}
}

