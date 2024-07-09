#include "Parser.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Lexer.h"
#include "Memory.h"

node *AllocateNode(const error_info *ErrorInfo)
{
	node *Result = (node *)AllocatePermanent(sizeof(node));
	Result->ErrorInfo = ErrorInfo;
	return Result;
}

// @NOTE: The body is not initialized here and it's the job of the caller to do it
node *MakeIf(const error_info *ErrorInfo, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_IF;
	Result->If.Expression = Expression;
	
	return Result;
}

node *MakeFor(const error_info *ErrorInfo, node *ForInit, node *ForExpr, node *ForIncr)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_FOR;
	Result->For.Init = ForInit;
	Result->For.Expr = ForExpr;
	Result->For.Incr = ForIncr;

	return Result;
}

node *MakeCast(const error_info *ErrorInfo, node *Expression, node *TypeNode, u32 FromType, u32 ToType)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_CAST;
	Result->Cast.Expression = Expression;
	Result->Cast.TypeNode = TypeNode;
	Result->Cast.FromType = FromType;
	Result->Cast.ToType = ToType;

	return Result;
}

node *MakeReturn(const error_info *ErrorInfo, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_RETURN;
	Result->Return.Expression = Expression;

	return Result;
}

node *MakeFunction(error_info *ErrorInfo, node **Args, node *ReturnType)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_FN;
	Result->Fn.Args = Args;
	Result->Fn.ReturnType = ReturnType;
	// Result->Fn.Body; Not needed with dynamic, the memory is cleared and when you push, it does everything it needs
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

node *MakePointerType(error_info *ErrorInfo, node *Pointed)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_PTRTYPE;
	Result->PointerType.Pointed = Pointed;
	return Result;
}

node *MakeBasicType(error_info *ErrorInfo, node *ID)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_BASICTYPE;
	Result->BasicType.ID = ID;
	return Result;
}

node *MakeDecl(error_info *ErrorInfo, node *ID, node *Expression, node *MaybeType, b32 IsConst, b32 IsShadow)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_DECL;
	Result->Decl.ID = ID;
	Result->Decl.Expression = Expression;
	Result->Decl.Type = MaybeType;
	Result->Decl.IsConst = IsConst;
	Result->Decl.IsShadow = IsShadow;
	return Result;
}

node *MakeASTString(error_info *ErrorInfo, const string *String)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_STRING;
	Result->String.S = String;
	return Result;
}

node *MakeNumber(error_info *ErrorInfo, u64 Number, b32 IsFloat)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_NUMBER;
	Result->Number.Bytes = Number;
	Result->Number.IsFloat = IsFloat;
	return Result;
}

token GetToken(parser *Parser)
{
	token Token = Parser->Tokens[Parser->TokenIndex++];
	if(Token.Type == T_EOF)
	{
		RaiseError(Token.ErrorInfo, "Unexpected End of File");
	}
	return Token;
}

token PeekToken(parser *Parser, int Depth)
{
	token Token = Parser->Tokens[Parser->TokenIndex + Depth];
	return Token;
}

token PeekToken(parser *Parser)
{
	return PeekToken(Parser, 0);
}

token EatToken(parser *Parser, token_type Type)
{
	token Token = PeekToken(Parser);
	if(Token.Type != Type)
	{
		RaiseError(Token.ErrorInfo, "Unexpected token!\nExpected: %s\nGot %s", GetTokenName(Type), GetTokenName(Token.Type));
	}
	GetToken(Parser);
	return Token;
}

token EatToken(parser *Parser, char C)
{
	return EatToken(Parser, (token_type)C);
}

node **ParseTokens(token *Tokens)
{
	node **Result = ArrCreate(node *);

	parser Parser;
	Parser.Tokens = Tokens;
	Parser.TokenIndex = 0;
	Parser.IsInBody = false;

	size_t TokenCount = ArrLen(Tokens);
	// @Note: + 1 because the last token is EOF, we don't want to try and parse it
	while(Parser.TokenIndex + 1 < TokenCount)
	{
		node *Node = ParseNode(&Parser);
		ArrPush(Result, Node);
	}
	return Result;
}

node *ParseNumber(parser *Parser)
{
	ERROR_INFO;
	token NumberToken = GetToken(Parser);
	const string *NumberString = NumberToken.ID;

	b32 IsFloat = false;
	for(int I = 0; I < NumberString->Size; ++I)
	{
		if(NumberString->Data[I] == '.')
			IsFloat = true;
	}

	u64 Bytes = 0;
	const char *Start = NumberToken.ID->Data;
	if(IsFloat)
	{
		f64 Typed = strtod(Start, NULL);
		Bytes = *(u64 *)&Typed;
	}
	else
	{
		Bytes = strtoul(Start, NULL, 10);
	}
	return MakeNumber(ErrorInfo, Bytes, IsFloat);
}

node *ParseAtom(parser *Parser, node *Operand)
{
	// @TODO: Implement
	return Operand;
}

// @Todo: arrays and type types
node *ParseType(parser *Parser)
{
	token FirstToken = PeekToken(Parser);
	switch(FirstToken.Type)
	{
		case T_ID:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *ID = MakeID(ErrorInfo, FirstToken.ID);
			return MakeBasicType(ErrorInfo, ID);
		} break;
		case T_PTR:
		{
			ERROR_INFO;
			GetToken(Parser);
			return MakePointerType(ErrorInfo, ParseType(Parser));
		} break;
		case T_FN:
		{
			return ParseFunctionType(Parser);
		} break;
		default:
		{
			RaiseError(FirstToken.ErrorInfo, "Expected a type. Found %s", GetTokenName(FirstToken.Type));
		} break;
	}
	return NULL; // @Note: Unreachable
}

node *ParseFunctionArgument(parser *Parser)
{
	ERROR_INFO;
	token ID = EatToken(Parser, T_ID);
	EatToken(Parser, ':');
	node *IDNode = MakeID(ErrorInfo, ID.ID);
	node *Type   = ParseType(Parser);
	return MakeDecl(ErrorInfo, IDNode, NULL, Type, true, false);
}

node **Delimited(parser *Parser, token_type Deliminator, node *(*Fn)(parser *))
{
	node **Nodes = ArrCreate(node *);
	while(true)
	{
		node *Result = Fn(Parser);
		ArrPush(Nodes, Result);
		if(PeekToken(Parser).Type != Deliminator)
			break;
		GetToken(Parser);
	}
	return Nodes;
}

node **Delimited(parser *Parser, char Deliminator, node *(*Fn)(parser *))
{
	return Delimited(Parser, (token_type)Deliminator, Fn);
}

node *ParseFunctionType(parser *Parser)
{
	ERROR_INFO;
	EatToken(Parser, T_FN);
	EatToken(Parser, '(');
	node **Args = Delimited(Parser, ',', ParseFunctionArgument);
	EatToken(Parser, ')');

	node *ReturnType = NULL;
	if(PeekToken(Parser).Type == T_ARR)
	{
		GetToken(Parser);
		ReturnType = ParseType(Parser);
	}

	return MakeFunction(ErrorInfo, Args, ReturnType);
}

void ParseBody(parser *Parser, dynamic<node *> &OutBody)
{
	b32 WasInBody = Parser->IsInBody;
	Parser->IsInBody = true;

	EatToken(Parser, T_STARTSCOPE);
	while(true)
	{
		node *Node = ParseNode(Parser);
		if(Node == NULL)
		{
			EatToken(Parser, T_ENDSCOPE);
			break;
		}
		OutBody.Push(Node);
	}

	Parser->IsInBody = WasInBody;
}

node *ParseOperand(parser *Parser)
{
	token Token = PeekToken(Parser);
	node *Result = NULL;
	switch((int)Token.Type)
	{

		// @NOTE: This allows to cast as a prefix AND postfix, should it be allowed?
		case T_AUTOCAST:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = Result;
			if(Expr == NULL)
				Expr = ParseExpression(Parser);
			Result = MakeCast(ErrorInfo, Expr, NULL, 0, 0);
		} break;
		case T_CAST:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Type = ParseType(Parser);
			node *Expr = Result;
			if(Expr == NULL)
				Expr = ParseExpression(Parser);
			Result = MakeCast(ErrorInfo, Expr, Type, 0, 0);
		} break;
		case T_FN:
		{
			Result = ParseFunctionType(Parser);
			if((int)PeekToken(Parser).Type == T_STARTSCOPE)
			{
				ParseBody(Parser, Result->Fn.Body);
			}
		} break;
		case T_ID:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeID(ErrorInfo, Token.ID);
		} break;
		case T_NUM:
		{
			Result = ParseNumber(Parser);
		} break;
		case T_STR:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeASTString(ErrorInfo, Token.ID);
		} break;
		case '(':
		{
			GetToken(Parser);
			Result = ParseExpression(Parser);
		} break;
	}
	return Result;
}

node *ParseUnary(parser *Parser)
{
	token Token = PeekToken(Parser);

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

	node *Operand = ParseOperand(Parser);

	node *Atom = ParseAtom(Parser, Operand);
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
		case '.':
			return MakePrecedence(90, 91);

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

		case '=':
		case T_PEQ:
		case T_MEQ:
		case T_TEQ:
		case T_DEQ:
		case T_MODEQ:
		case T_ANDEQ:
		case T_XOREQ:
		case T_OREQ:
			return MakePrecedence(-20, -21); // right to left precedence


		// Non binary operator, abort the loop
		default:
			return MakePrecedence(-1000, -1000);
	}
}

node *ParseExpression(parser *Parser, int CurrentPrecedence)
{
	node *LHS = ParseUnary(Parser);
	
	while(true)
	{
		token BinaryOp = PeekToken(Parser);
		precedence Prec = GetPrecedence(BinaryOp.Type);
		int LeftP  = Prec.Left;
		int RightP = Prec.Right;
		if(LeftP < CurrentPrecedence)
			break;
		ERROR_INFO;
		GetToken(Parser);
		node *RHS = ParseExpression(Parser, RightP);
		LHS = MakeBinary(ErrorInfo, LHS, RHS, BinaryOp.Type);
	}
	return LHS;
}

node *ParseExpression(parser *Parser)
{
	return ParseExpression(Parser, -999);
}

node *ParseDeclaration(parser *Parser, b32 IsShadow)
{
	b32 IsConst = false;

	ERROR_INFO;
	token ID = GetToken(Parser);
	if(ID.Type != T_ID)
		RaiseError(*ErrorInfo, "Expected decleration!");

	node *IDNode = MakeID(ErrorInfo, ID.ID);
	token Decl = GetToken(Parser);
	switch(Decl.Type)
	{
		case T_DECL:
		{
			IsConst = false;
		} break;
		case T_CONST:
		{
			IsConst = true;
		} break;
		default:
		{
			RaiseError(*ErrorInfo, "Expected decleration!");
		} break;
	}

	token MaybeType = PeekToken(Parser);
	node *MaybeTypeNode = NULL;
	if(!IsConst)
	{
		if(MaybeType.Type != T_EQ)
		{
			MaybeTypeNode = ParseType(Parser);
		}
		EatToken(Parser, T_EQ);
	}
	node *Expression = ParseExpression(Parser);
	return MakeDecl(ErrorInfo, IDNode, Expression, MaybeTypeNode, IsConst, IsShadow);
}

void ParseMaybeBody(parser *Parser, dynamic<node *> &OutBody)
{
	if(PeekToken(Parser).Type == T_STARTSCOPE)
	{
		ParseBody(Parser, OutBody);
	}
	else
	{
		OutBody.Push(ParseNode(Parser));
	}
}

node *ParseNode(parser *Parser)
{
	token Token = PeekToken(Parser);
	node *Result = NULL;
	b32 ExpectSemicolon = true;
	switch(Token.Type)
	{
		case T_SHADOW:
		{
			GetToken(Parser);
			if(PeekToken(Parser).Type != T_ID)
			{
				RaiseError(Token.ErrorInfo, "Expected variable declaration after compiler directive #shadow\n"
						"Example: #shadow MyVar := 10;");
			}
			token Next = PeekToken(Parser, 1);
			if(Next.Type != T_DECL && Next.Type != T_CONST)
			{
				RaiseError(Token.ErrorInfo, "Expected variable declaration after compiler directive #shadow\n"
						"Example: #shadow MyVar := 10;");
			}

			Result = ParseDeclaration(Parser, true);
		} break;
		case T_ID:
		{
			token Next = PeekToken(Parser, 1);
			switch(Next.Type)
			{
				case T_DECL:
				case T_CONST:
				{
					Result = ParseDeclaration(Parser, false);
				} break;
				default:
				{
					Result = ParseExpression(Parser);
				} break;
			}
		} break;
		case T_RETURN:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeReturn(ErrorInfo, ParseExpression(Parser));
		} break;
		case T_IF:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *IfExpression = ParseExpression(Parser);
			Result = MakeIf(ErrorInfo, IfExpression);
			ParseMaybeBody(Parser, Result->If.Body);
			
			if(PeekToken(Parser).Type == T_ELSE)
			{
				GetToken(Parser);
				ParseMaybeBody(Parser, Result->If.Body);
			}

			ExpectSemicolon = false;
		} break;
		case T_FOR:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *ForInit = NULL;
			node *ForExpr = NULL;
			node *ForIncr = NULL;
			if(PeekToken(Parser).Type != ';')
				ForInit = ParseDeclaration(Parser, false);
			EatToken(Parser, ';');

			if(PeekToken(Parser).Type != ';')
				ForExpr = ParseExpression(Parser);
			EatToken(Parser, ';');

			if(PeekToken(Parser).Type != ';')
				ForIncr = ParseExpression(Parser);

			Result = MakeFor(ErrorInfo, ForInit, ForExpr, ForIncr);
			ParseMaybeBody(Parser, Result->For.Body);
			
			ExpectSemicolon = false;
		} break;
		case T_ENDSCOPE:
		{
			if(!Parser->IsInBody)
			{
				RaiseError(Token.ErrorInfo, "No matching opening '{' for '}'");
			}
			ExpectSemicolon = false;
		} break;
		default:
		{
			Result = ParseExpression(Parser);
		} break;
	}
	if(ExpectSemicolon)
		EatToken(Parser, ';');
	return Result;
}

