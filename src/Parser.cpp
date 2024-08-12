#include "Parser.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Lexer.h"
#include "Memory.h"
#include "Semantics.h"
#include "Type.h"
#include "VString.h"

node *AllocateNode(const error_info *ErrorInfo)
{
	node *Result = (node *)AllocatePermanent(sizeof(node));
	Result->ErrorInfo = ErrorInfo;
	return Result;
}

node *MakeSize(const error_info *ErrorInfo, node *Expr)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_SIZE;
	Result->Size.Expression = Expr;

	return Result;
}

node *MakeSelector(const error_info *ErrorInfo, node *Operand, const string *Member)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_SELECTOR;
	Result->Selector.Operand = Operand;
	Result->Selector.Member = Member;

	return Result;
}

node *MakeStructList(const error_info *ErrorInfo, slice<const string *> Names,
		slice<node *> Expressions, node *StructType)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_STRUCTLIST;
	Result->StructList.StructType = StructType;
	Result->StructList.Names = Names;
	Result->StructList.Expressions = Expressions;

	return Result;
}

node *MakeStructDecl(const error_info *ErrorInfo, const string *Name, slice<node *> Members)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_STRUCTDECL;
	Result->StructDecl.Name = Name;
	Result->StructDecl.Members = Members;

	return Result;
}

node *MakeIndex(const error_info *ErrorInfo, node *Operand, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_INDEX;
	Result->Index.Expression = Expression;
	Result->Index.Operand = Operand;
	Result->Index.ForceNotLoad = false;

	return Result;
}

node *MakeArrayList(const error_info *ErrorInfo, slice<node *> Expressions)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_ARRAYLIST;
	Result->ArrayList.Expressions = Expressions;

	return Result;
}

node *MakeCall(const error_info *ErrorInfo, node *Operand, slice<node *> Args)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_CALL;
	Result->Call.Fn = Operand;
	Result->Call.Args = Args;
	Result->Call.Type = INVALID_TYPE;

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

node *MakeFunction(error_info *ErrorInfo, slice<node *> Args, node *ReturnType, u32 Flags)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_FN;
	Result->Fn.Args = Args;
	Result->Fn.ReturnType = ReturnType;
	Result->Fn.Flags = Flags;
	// Result->Fn.Body; Not needed with dynamic, the memory is cleared and when you push, it does everything it needs
	return Result;
}

node *MakeUnary(const error_info *ErrorInfo, node *Operand, token_type Op)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_UNARY;
	Result->Unary.Operand = Operand;
	Result->Unary.Op = Op;
	return Result;
}

node *MakeBinary(const error_info *ErrorInfo, node *Left, node *Right, token_type Op)
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

node *MakeArrayType(const error_info *ErrorInfo, node *ID, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_ARRAYTYPE;
	Result->ArrayType.Type = ID;
	Result->ArrayType.Expression = Expression;
	return Result;
}

node *MakeBasicType(error_info *ErrorInfo, node *ID)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_BASICTYPE;
	Result->BasicType.ID = ID;
	return Result;
}

node *MakeDecl(error_info *ErrorInfo, const string *ID, node *Expression, node *MaybeType, u32 Flags)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_DECL;
	Result->Decl.ID = ID;
	Result->Decl.Expression = Expression;
	Result->Decl.Type = MaybeType;
	Result->Decl.Flags = Flags;
	return Result;
}

node *MakeConstant(error_info *ErrorInfo, const_value Value)
{
	node *Result = AllocateNode(ErrorInfo);
	Result->Type = AST_CONSTANT;
	Result->Constant.Value = Value;
	return Result;
}

token GetToken(parser *Parser)
{
	token Token = Parser->Tokens[Parser->TokenIndex++];
	if(Token.Type == T_EOF)
	{
		RaiseError(Token.ErrorInfo, "Unexpected End of File");
	}
	Parser->Current = &Parser->Tokens[Parser->TokenIndex];
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
		RaiseError(Token.ErrorInfo, "Unexpected token!\nExpected: %s\nGot: %s", GetTokenName(Type), GetTokenName(Token.Type));
	}
	GetToken(Parser);
	return Token;
}

token EatToken(parser *Parser, char C)
{
	return EatToken(Parser, (token_type)C);
}

parse_result ParseTokens(token *Tokens, string ModuleName)
{
	dynamic<node *>Nodes = {};

	parser Parser = {};
	Parser.Tokens = Tokens;
	Parser.TokenIndex = 0;
	Parser.Current = Tokens;
	Parser.ModuleName = ModuleName;
	Parser.IsInBody = false;
	Parser.CurrentlyPublic = true;

	size_t TokenCount = ArrLen(Tokens);
	// @Note: + 1 because the last token is EOF, we don't want to try and parse it
	while(Parser.TokenIndex + 1 < TokenCount)
	{
		node *Node = ParseTopLevel(&Parser);
		if(Node)
		{
			if(Node != (node *)0x1)
			{
				Nodes.Push(Node);
			}
		}
		else
		{
			break;
		}
	}

	parse_result Result = {};
	Result.Nodes = Nodes;
	Result.Imports = SliceFromArray(Parser.Imported);

	return Result;
}

node *ParseNumber(parser *Parser)
{
	ERROR_INFO;
	token NumberToken = GetToken(Parser);
	const string *NumberString = NumberToken.ID;

	const_value Value = MakeConstValue(NumberString);

	return MakeConstant(ErrorInfo, Value);
}

node *ParseArrayType(parser *Parser)
{
	node *Expression = NULL;
	if(Parser->Current->Type != T_CLOSEBRACKET)
	{
		Expression = ParseExpression(Parser);
	}

	EatToken(Parser, T_CLOSEBRACKET);
	node *ID = ParseType(Parser);
	if(ID == NULL)
	{
		RaiseError(Parser->Current->ErrorInfo, "Expected type after [] for declaring an array");
	}

	return MakeArrayType(ID->ErrorInfo, ID, Expression);
}

string MakeLambdaName(error_info *Info)
{
	string_builder Builder = MakeBuilder();
	PushBuilderFormated(&Builder, "__lambda_%s%d", Info->FileName, Info->Line);
	return MakeString(Builder);
}

// @Todo: type types
node *ParseType(parser *Parser, b32 ShouldError)
{
	token ErrorToken = PeekToken(Parser);
	node *Result = NULL;
	switch(Parser->Current->Type)
	{
		case T_ID:
		{
			ERROR_INFO;
			token IDToken = GetToken(Parser);
			node *ID = MakeID(ErrorInfo, IDToken.ID);
			if(Parser->Current->Type == T_DOT)
			{
				GetToken(Parser);
				token TypeID = EatToken(Parser, T_ID);
				Result = MakeSelector(ErrorInfo, ID, TypeID.ID);
			}
			else
			{
				Result = MakeBasicType(ErrorInfo, ID);
			}
		} break;
		case T_OPENBRACKET:
		{
			GetToken(Parser);
			Result = ParseArrayType(Parser);
		} break;
		case T_PTR:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Pointed = ParseType(Parser, false);
			Result = MakePointerType(ErrorInfo, Pointed);
		} break;
		case T_FN:
		{
			Result = ParseFunctionType(Parser);
		} break;
		default:
		{
		} break;
	}
	if(Result == NULL && ShouldError)
	{
		RaiseError(ErrorToken.ErrorInfo, "Expected a type. Found %s", GetTokenName(ErrorToken.Type));
	}
	return Result;
}

node *ParseFunctionArgument(parser *Parser)
{
	ERROR_INFO;
	token ID = EatToken(Parser, T_ID);
	EatToken(Parser, ':');
	node *Type   = ParseType(Parser);
	return MakeDecl(ErrorInfo, ID.ID, NULL, Type, SymbolFlag_Const);
}

slice<node *> Delimited(parser *Parser, token_type Deliminator, node *(*Fn)(parser *))
{
	dynamic<node *>Nodes{};
	while(true)
	{
		node *Node = Fn(Parser);
		if(Node)
			Nodes.Push(Node);
		if(PeekToken(Parser).Type != Deliminator)
			break;
		GetToken(Parser);
	}
	return SliceFromArray(Nodes);
}

slice<node *> Delimited(parser *Parser, char Deliminator, node *(*Fn)(parser *))
{
	return Delimited(Parser, (token_type)Deliminator, Fn);
}

node *ParseFunctionType(parser *Parser)
{
	ERROR_INFO;
	u32 Flags = 0;
	EatToken(Parser, T_FN);
	if(Parser->Current->Type == T_FOREIGN)
	{
		GetToken(Parser);
		Flags |= SymbolFlag_Foreign;
	}
	EatToken(Parser, '(');
	slice<node *> Args{};
	if(PeekToken(Parser).Type != T_CLOSEPAREN)
		Args = Delimited(Parser, ',', ParseFunctionArgument);
	EatToken(Parser, ')');

	node *ReturnType = NULL;
	if(PeekToken(Parser).Type == T_ARR)
	{
		GetToken(Parser);
		ReturnType = ParseType(Parser);
	}

	return MakeFunction(ErrorInfo, Args, ReturnType, Flags);
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

node *ParseFunctionCall(parser *Parser, node *Operand)
{
	ERROR_INFO;
	if(Operand == NULL)
	{
		RaiseError(*ErrorInfo, "Trying to call an invalid expression as a function");
	}

	dynamic<node *> Args = {};
	EatToken(Parser, T_OPENPAREN);
	while(PeekToken(Parser).Type != T_CLOSEPAREN)
	{
		Args.Push(ParseExpression(Parser));
		token Next = PeekToken(Parser);
		if(Next.Type == ',')
		{
			GetToken(Parser);
		}
		else if(Next.Type != ')')
		{
			ERROR_INFO;
			RaiseError(*ErrorInfo, "Improper argument formatting\nProper arguments example: call(arg1, arg2)");
		}
	}
	EatToken(Parser, T_CLOSEPAREN);
	return MakeCall(ErrorInfo, Operand, SliceFromArray(Args));
}

node *ParseIndex(parser *Parser, node *Operand)
{
	ERROR_INFO;
	if(Operand == NULL)
	{
		RaiseError(*ErrorInfo, "Trying to index an invalid expression");
	}

	EatToken(Parser, T_OPENBRACKET);
	node *IndexExpression = ParseExpression(Parser);
	EatToken(Parser, T_CLOSEBRACKET);

	return MakeIndex(ErrorInfo, Operand, IndexExpression);
}

node *ParseSelectors(parser *Parser, node *Operand)
{
	ERROR_INFO;
	EatToken(Parser, T_DOT);
	while(true)
	{
		token ID = EatToken(Parser, T_ID);
		Operand = MakeSelector(ErrorInfo, Operand, ID.ID);
		if(Parser->Current->Type != T_DOT)
			break;
		ErrorInfo = &Parser->Tokens[Parser->TokenIndex].ErrorInfo;
		EatToken(Parser, T_DOT);
	}

	return Operand;
}

node *ParseAtom(parser *Parser, node *Operand)
{
	bool Loop = true;
	while(Loop)
	{
		token FirstToken = PeekToken(Parser);
		switch(FirstToken.Type)
		{
			case T_OPENPAREN:
			{
				Operand = ParseFunctionCall(Parser, Operand);
			} break;
			case T_OPENBRACKET:
			{
				Operand = ParseIndex(Parser, Operand);
			} break;
			case T_DOT:
			{
				Operand = ParseSelectors(Parser, Operand);
			} break;
			default:
			{
				Loop = false;
			} break;
		}
	}
	return Operand;
}

node *ParseStructList(parser *Parser)
{
	ERROR_INFO;
	node *StructType = ParseType(Parser, true);
	EatToken(Parser, T_STARTSCOPE);
	dynamic<const string *> FieldNames = {};
	dynamic<node *> FieldExpressions = {};
	while(Parser->Current->Type != T_ENDSCOPE)
	{
		token FieldNameT = GetToken(Parser);
		if(FieldNameT.Type != T_ID)
		{
			RaiseError(FieldNameT.ErrorInfo, "Expected name of field, got: %s. To initialize a struct the syntax is:\n"
					"\tStructName { field_name1 = value, field_name2 = value };",
					GetTokenName(FieldNameT.Type));
		}
		EatToken(Parser, T_EQ);
		node *Expression = ParseExpression(Parser);
		if(Parser->Current->Type == T_COMMA)
		{
			GetToken(Parser);
		}
		else
		{
			if(Parser->Current->Type != T_ENDSCOPE)
			{
				RaiseError(Parser->Current->ErrorInfo,
						"Expected ',' to continue struct list or '}' to close it, got: %s",
						GetTokenName(FieldNameT.Type));
			}
		}

		FieldNames.Push(FieldNameT.ID);
		FieldExpressions.Push(Expression);
	}
	EatToken(Parser, T_ENDSCOPE);

	return MakeStructList(ErrorInfo, SliceFromArray(FieldNames), SliceFromArray(FieldExpressions), StructType);
}

node *ParseOperand(parser *Parser)
{
	token Token = PeekToken(Parser);
	node *Result = NULL;
	switch((int)Token.Type)
	{
		case T_AUTOCAST:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = ParseExpression(Parser);
			Result = MakeCast(ErrorInfo, Expr, NULL, 0, 0);
		} break;
		case T_CAST:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Type = ParseType(Parser);
			node *Expr = ParseExpression(Parser);
			Result = MakeCast(ErrorInfo, Expr, Type, 0, 0);
		} break;
		case T_SIZE:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = ParseOperand(Parser);
			Result = MakeSize(ErrorInfo, Expr);
		} break;
		case T_FN:
		{
			ERROR_INFO;
			Result = ParseFunctionType(Parser);
			string Name = MakeLambdaName(ErrorInfo);
			Result->Fn.Name = DupeType(Name, string);
			if((int)PeekToken(Parser).Type == T_STARTSCOPE)
			{
				ParseBody(Parser, Result->Fn.Body);
			}
		} break;
		case T_ID:
		{
			ERROR_INFO;
			if(PeekToken(Parser, 1).Type == T_DOT && PeekToken(Parser, 3).Type == T_STARTSCOPE)
				Result = ParseStructList(Parser);
			else if(PeekToken(Parser, 1).Type == T_STARTSCOPE)
				Result = ParseStructList(Parser);
			else
				Result = MakeID(ErrorInfo, GetToken(Parser).ID);
		} break;
		case T_STARTSCOPE:
		{
			ERROR_INFO;
			GetToken(Parser);
			if(Parser->Current->Type == T_ENDSCOPE)
			{
				Result = MakeArrayList(ErrorInfo, ZeroSlice<node *>());
			}
			else
			{
				slice<node *> Items = Delimited(Parser, ',', ParseExpression);
				Result = MakeArrayList(ErrorInfo, Items);
			}
			EatToken(Parser, T_ENDSCOPE);
		} break;
		case T_VAL:
		{
			Result = ParseNumber(Parser);
		} break;
		case T_CSTR:
		{
			ERROR_INFO;
			GetToken(Parser);
			const_value Value = MakeConstString(Token.ID);
			Value.String.Flags = ConstString_CSTR;
			Result = MakeConstant(ErrorInfo, Value);
		} break;
		case T_STR:
		{
			ERROR_INFO;
			GetToken(Parser);
			const_value Value = MakeConstString(Token.ID);
			Result = MakeConstant(ErrorInfo, Value);
		} break;
		case T_OPENPAREN:
		{
			GetToken(Parser);
			Result = ParseExpression(Parser);
			EatToken(Parser, T_CLOSEPAREN);
		} break;
	}
	return Result;
}

node *ParseUnary(parser *Parser)
{
	token Token = PeekToken(Parser);

	node *Result = NULL;
	ERROR_INFO;
	switch(Token.Type)
	{
		case T_ADDROF:
		case T_PTR:
		{
			GetToken(Parser);
			Result = MakeUnary(ErrorInfo, ParseUnary(Parser), Token.Type);
			return Result;
		} break;
		default: break;
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
	b32 HasExpression = true;
	if(!IsConst)
	{
		if(MaybeType.Type != T_EQ)
		{
			MaybeTypeNode = ParseType(Parser);
		}
		if(Parser->Current->Type != T_EQ)
			HasExpression = false;
		else
			GetToken(Parser);
	}
	node *Expression = NULL;
	if(HasExpression)
		Expression = ParseExpression(Parser);
	u32 Flags = IsConst ? SymbolFlag_Const : 0 | IsShadow ? SymbolFlag_Shadow : 0;
	return MakeDecl(ErrorInfo, ID.ID, Expression, MaybeTypeNode, Flags);
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
			node *Expr = NULL;
			if(Parser->Current->Type != ';')
				Expr = ParseExpression(Parser);
			Result = MakeReturn(ErrorInfo, Expr);
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

			if(PeekToken(Parser).Type != T_STARTSCOPE)
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

string *StructToModuleNamePtr(string &StructName, string &ModuleName)
{
	string Result = StructToModuleName(StructName, ModuleName);
	string *StructNamePtr = NewType(string);
	*StructNamePtr = Result;
	return StructNamePtr;
}

string StructToModuleName(string &StructName, string &ModuleName)
{
	string_builder Builder = MakeBuilder();
	Builder += ModuleName;
	Builder += '!';
	Builder += StructName;
	return MakeString(Builder);
}

node *ParseTopLevel(parser *Parser)
{
	node *Result = NULL;
	token StartToken = PeekToken(Parser);
	switch(StartToken.Type)
	{
		case T_PUBLIC:
		{
			GetToken(Parser);
			Parser->CurrentlyPublic = true;
			Result = (node *)0x1;
		} break;
		case T_PRIVATE:
		{
			GetToken(Parser);
			Parser->CurrentlyPublic = false;
			Result = (node *)0x1;
		} break;
		case T_ID:
		{
			node *Decl = ParseDeclaration(Parser, false);
			if(Decl->Decl.Expression->Type == AST_FN)
			{
				node *Fn = Decl->Decl.Expression;
				if(Fn == NULL)
				{
					RaiseError(*Decl->ErrorInfo, "Invalid function declaration");
				}
				if((Decl->Decl.Flags & SymbolFlag_Const) == 0)
				{
					RaiseError(*Decl->ErrorInfo, "Global function declaration needs to be constant");
				}
				Fn->Fn.Name = Decl->Decl.ID;
				if(Parser->CurrentlyPublic)
					Fn->Fn.Flags |= SymbolFlag_Public;
				if(!Fn->Fn.Body.IsValid())
					EatToken(Parser, ';');
				Result = Fn;
			}
			else
			{
				if(Parser->CurrentlyPublic)
					Decl->Decl.Flags |= SymbolFlag_Public;
				Result = Decl;
				EatToken(Parser, ';');
			}
		} break;
		case T_IMPORT:
		{
			GetToken(Parser);
			token T = EatToken(Parser, T_ID);
			string *As = NULL;
			if(Parser->Current->Type == T_AS)
			{
				GetToken(Parser);
				As = EatToken(Parser, T_ID).ID;
			}
			import Imported = {.Name = *T.ID, .As = As ? *As : STR_LIT("")};
			Parser->Imported.Push(Imported);
			Result = (node *)0x1;
		} break;
		case T_STRUCT:
		{
			ERROR_INFO;
			GetToken(Parser);
			token NameT = GetToken(Parser);
			if(NameT.Type != T_ID)
			{
				RaiseError(*ErrorInfo, "Expected struct name after `struct` keyword");
			}
			EatToken(Parser, T_STARTSCOPE);

			auto ParseFn = [](parser *P) -> node* {
				if(P->Current->Type == T_ENDSCOPE)
					return NULL;
				return ParseDeclaration(P, false);
			};
			auto Name = StructToModuleNamePtr(*NameT.ID, Parser->ModuleName);
			Result = MakeStructDecl(ErrorInfo, Name, Delimited(Parser, ',', ParseFn));
			EatToken(Parser, T_ENDSCOPE);
			if(Parser->Current->Type == T_SEMICOL)
			{
				RaiseError(Parser->Current->ErrorInfo,
						"In this language you do not put semicolons after struct declarations");
			}
		} break;
		case T_EOF:
		{
			return NULL;
		} break;
		default:
		{
#if defined(DEBUG)
			LERROR("%d", StartToken.Type);
#endif
			RaiseError(StartToken.ErrorInfo, "Unexpected Top Level declaration");
		} break;
	}

	return Result;
}

