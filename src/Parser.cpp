#include "Parser.h"
#include "Dynamic.h"
#include "DynamicLib.h"
#include "Errors.h"
#include "Lexer.h"
#include "Log.h"
#include "Memory.h"
#include "Pipeline.h"
#include "Semantics.h"
#include "Type.h"
#include "VString.h"

string ErrorID = STR_LIT("Error");

node *ParseOperand(parser *Parser);

void CopyRangeEndToErrorInfo(error_info *Out, const error_info *End)
{
	Out->Range.EndChar = End->Range.EndChar;
	Out->Range.EndLine = End->Range.EndLine;
}

node *AllocateNode(const error_info *ErrorInfo, node_type Type)
{
	node *Result = (node *)AllocatePermanent(sizeof(node));
	Result->ErrorInfo = ErrorInfo;
	Result->Type = Type;
	return Result;
}

node *MakeFileLocation(const error_info *ErrorInfo)
{
	node *Result = AllocateNode(ErrorInfo, AST_FILE_LOCATION);

	return Result;
}

node *MakeRun(const error_info *ErrorInfo, slice<node *> Body)
{
	node *Result = AllocateNode(ErrorInfo, AST_RUN);
	Result->Run.Body = Body;
	Result->Run.TypeIdx = INVALID_TYPE;
	Result->Run.IsExprRun = false;

	return Result;
}

node *MakeTypedExpr(const error_info *ErrorInfo, node_type T, node *Expr)
{
	node *Result = AllocateNode(ErrorInfo, T);
	Result->TypedExpr.Expr = Expr;
	Result->TypedExpr.TypeIdx = INVALID_TYPE;

	return Result;
}

node *MakeAssert(const error_info *ErrorInfo, node *Expr)
{
	node *Result = AllocateNode(ErrorInfo, AST_ASSERT);
	Result->Assert.Expr = Expr;

	return Result;
}

node *MakeVar(const error_info *ErrorInfo, const string *Name, node *Type, node *DefaultExpr)
{
	node *Result = AllocateNode(ErrorInfo, AST_VAR);
	Result->Var.Name = Name;
	Result->Var.TypeNode = Type;
	Result->Var.Default = DefaultExpr;

	return Result;
}

node *MakeList(const error_info *ErrorInfo, slice<node *> Nodes)
{
	node *Result = AllocateNode(ErrorInfo, AST_LIST);
	Result->List.Nodes = Nodes;

	return Result;
}

node *MakeScope(const error_info *ErrorInfo, b32 IsUp)
{
	node *Result = AllocateNode(ErrorInfo, AST_SCOPE);
	Result->ScopeDelimiter.IsUp = IsUp;


	return Result;
}

node *MakeEmbed(const error_info *ErrorInfo, const string *FileName, b32 IsString)
{
	node *Result = AllocateNode(ErrorInfo, AST_EMBED);
	Result->Embed.IsString = IsString;
	Result->Embed.FileName = FileName;

	return Result;
}

node *MakeTypeInfo(const error_info *ErrorInfo, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo, AST_TYPEINFO);
	Result->TypeInfoLookup.Expression = Expression;
	
	return Result;
}

node *MakeDefer(const error_info *ErrorInfo, dynamic<node *> Body)
{
	node *Result = AllocateNode(ErrorInfo, AST_DEFER);
	Result->Defer.Body = SliceFromArray(Body);

	return Result;
}

node *MakeNop(const error_info *ErrorInfo)
{
	node *Result = AllocateNode(ErrorInfo, AST_NOP);

	return Result;
}

node *MakeCase(const error_info *ErrorInfo, node *Value, slice<node *> Body)
{
	node *Result = AllocateNode(ErrorInfo, AST_CASE);
	Result->Case.Value = Value;
	Result->Case.Body = Body;

	return Result;
}

node *MakePointerDiff(const error_info *ErrorInfo, node *Left, node *Right, u32 Type)
{
	node *Result = AllocateNode(ErrorInfo, AST_PTRDIFF);
	Result->PtrDiff.Left = Left;
	Result->PtrDiff.Right = Right;
	Result->PtrDiff.Type = Type;

	return Result;
}

node *MakeSwitch(const error_info *ErrorInfo, node *Expression, slice<node *> Cases)
{
	node *Result = AllocateNode(ErrorInfo, AST_SWITCH);
	Result->Switch.Expression = Expression;
	Result->Switch.Cases = Cases;

	return Result;
}

node *MakeCharLiteral(const error_info *ErrorInfo, u32 C)
{
	node *Result = AllocateNode(ErrorInfo, AST_CHARLIT);
	Result->CharLiteral.C = C;

	return Result;
}

node *MakeReserve(const error_info *ErrorInfo, reserved ID)
{
	node *Result = AllocateNode(ErrorInfo, AST_RESERVED);
	Result->Reserved.ID = ID;

	return Result;
}

node *MakeContinue(const error_info *ErrorInfo)
{
	node *Result = AllocateNode(ErrorInfo, AST_CONTINUE);

	return Result;
}

node *MakeBreak(const error_info *ErrorInfo)
{
	node *Result = AllocateNode(ErrorInfo, AST_BREAK);

	return Result;
}

node *MakeGenericNode(const error_info *ErrorInfo, const string *T)
{
	node *Result = AllocateNode(ErrorInfo, AST_GENERIC);
	Result->Generic.Name = T;

	return Result;
}

node *MakeTypeOf(const error_info *ErrorInfo, node *Expr)
{
	node *Result = AllocateNode(ErrorInfo, AST_TYPEOF);
	Result->TypeOf.Expression = Expr;

	return Result;
}

node *MakeSize(const error_info *ErrorInfo, node *Expr)
{
	node *Result = AllocateNode(ErrorInfo, AST_SIZE);
	Result->Size.Expression = Expr;

	return Result;
}

node *MakeSelector(const error_info *ErrorInfo, node *Operand, const string *Member)
{
	node *Result = AllocateNode(ErrorInfo, AST_SELECTOR);
	Result->Selector.Operand = Operand;
	Result->Selector.Member = Member;

	return Result;
}

node *MakeEnum(const error_info *ErrorInfo, const string *Name, slice<node *> Items, node *T)
{
	node *Result = AllocateNode(ErrorInfo, AST_ENUM);
	Result->Enum.Name = Name;
	Result->Enum.Items = Items;
	Result->Enum.Type = T;

	return Result;
}

node *MakeStructDecl(const error_info *ErrorInfo, const string *Name, slice<node *> Members, slice<string> GenericTypeParams, b32 IsUnion)
{
	node *Result = AllocateNode(ErrorInfo, AST_STRUCTDECL);
	Result->StructDecl.Name = Name;
	Result->StructDecl.Members = Members;
	Result->StructDecl.TypeParams = GenericTypeParams;
	Result->StructDecl.IsUnion = IsUnion;

	return Result;
}

node *MakeIndex(const error_info *ErrorInfo, node *Operand, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo, AST_INDEX);
	Result->Index.Expression = Expression;
	Result->Index.Operand = Operand;
	Result->Index.ForceNotLoad = false;

	return Result;
}

node *MakeListItem(const error_info *ErrorInfo, const string *OptionalName, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo, AST_LISTITEM);
	Result->Item.Name = OptionalName;
	Result->Item.Expression = Expression;


	return Result;
}

node *MakeTypeList(const error_info *ErrorInfo, node *Type, slice<node *> ListItems)
{
	node *Result = AllocateNode(ErrorInfo, AST_TYPELIST);
	Result->TypeList.TypeNode = Type;
	Result->TypeList.Items = ListItems;

	return Result;
}

node *MakeCall(const error_info *ErrorInfo, node *Operand, slice<node *> Args)
{
	node *Result = AllocateNode(ErrorInfo, AST_CALL);
	Result->Call.Fn = Operand;
	Result->Call.Args = Args;
	Result->Call.Type = INVALID_TYPE;

	return Result;
}

node *MakeIfX(const error_info *ErrorInfo, node *IfExpr, node *TrueExpr, node *FalseExpr)
{
	node *Result = AllocateNode(ErrorInfo, AST_IFX);
	Result->IfX.Expr = IfExpr;
	Result->IfX.True = TrueExpr;
	Result->IfX.False = FalseExpr;

	return Result;
}

// @NOTE: The body is not initialized here and it is the job of the caller to do it
node *MakeIf(const error_info *ErrorInfo, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo, AST_IF);
	Result->If.Expression = Expression;
	
	return Result;
}

node *MakeFor(const error_info *ErrorInfo, node *Expr1, node *Expr2, node *Expr3, for_type Kind)
{
	node *Result = AllocateNode(ErrorInfo, AST_FOR);
	Result->For.Expr1 = Expr1;
	Result->For.Expr2 = Expr2;
	Result->For.Expr3 = Expr3;
	Result->For.Kind = Kind;
	Result->For.ItByRef = false;

	return Result;
}

node *MakeCast(const error_info *ErrorInfo, node *Expression, node *TypeNode, u32 FromType, u32 ToType)
{
	node *Result = AllocateNode(ErrorInfo, AST_CAST);
	Result->Cast.Expression = Expression;
	Result->Cast.TypeNode = TypeNode;
	Result->Cast.FromType = FromType;
	Result->Cast.ToType = ToType;
	Result->Cast.IsBitCast = false;

	return Result;
}

node *MakeReturn(const error_info *ErrorInfo, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo, AST_RETURN);
	Result->Return.Expression = Expression;

	return Result;
}

node *MakeFunction(const error_info *ErrorInfo, const string *LinkName, slice<node *> Args, slice<node *> ReturnTypes, u32 Flags)
{
	node *Result = AllocateNode(ErrorInfo, AST_FN);
	Result->Fn.LinkName = LinkName;
	Result->Fn.Args = Args;
	Result->Fn.ReturnTypes = ReturnTypes;
	Result->Fn.Flags = Flags;
	Result->Fn.AlreadyAnalyzed = false;
	// Result->Fn.Body; Not needed with dynamic, the memory is cleared and when you push, it does everything it needs
	return Result;
}

node *MakeUnary(const error_info *ErrorInfo, node *Operand, token_type Op)
{
	node *Result = AllocateNode(ErrorInfo, AST_UNARY);
	Result->Unary.Operand = Operand;
	Result->Unary.Op = Op;
	return Result;
}

node *MakeBinary(const error_info *ErrorInfo, node *Left, node *Right, token_type Op)
{
	node *Result = AllocateNode(ErrorInfo, AST_BINARY);
	Result->Binary.Left = Left;
	Result->Binary.Right = Right;
	Result->Binary.Op = Op;
	return Result;
}

node *MakeID(const error_info *ErrorInfo, const string *ID)
{
	node *Result = AllocateNode(ErrorInfo, AST_ID);
	Result->ID.Name = ID;
	Result->ID.Type = INVALID_TYPE;
	return Result;
}

node *MakePointerType(const error_info *ErrorInfo, node *Pointed)
{
	node *Result = AllocateNode(ErrorInfo, AST_PTRTYPE);
	Result->PointerType.Pointed = Pointed;
	Result->PointerType.Flags   = 0;
	return Result;
}

node *MakeArrayType(const error_info *ErrorInfo, node *ID, node *Expression)
{
	node *Result = AllocateNode(ErrorInfo, AST_ARRAYTYPE);
	Result->ArrayType.Type = ID;
	Result->ArrayType.Expression = Expression;
	return Result;
}

node *MakeGenericStructType(const error_info *ErrorInfo, node *ID, slice<node *> Args)
{
	node *Result = AllocateNode(ErrorInfo, AST_GENSTRUCTTYPE);
	Result->GenericStructType.ID = ID;
	Result->GenericStructType.Args = Args;
	return Result;
}

node *MakeDecl(const error_info *ErrorInfo, node *LHS, node *Expression, node *MaybeType, u32 Flags)
{
	node *Result = AllocateNode(ErrorInfo, AST_DECL);
	Result->Decl.LHS = LHS;
	Result->Decl.Expression = Expression;
	Result->Decl.Type = MaybeType;
	Result->Decl.Flags = Flags;
	return Result;
}

node *MakeConstant(const error_info *ErrorInfo, const_value Value)
{
	node *Result = AllocateNode(ErrorInfo, AST_CONSTANT);
	Result->Constant.Value = Value;
	return Result;
}

token GetToken(parser *Parser)
{
	token Token = Parser->Tokens[Parser->TokenIndex++];
	if(Token.Type == T_EOF)
	{
		RaiseError(true, Token.ErrorInfo, "Unexpected End of File");
	}
	Parser->Current = &Parser->Tokens[Parser->TokenIndex];
	return Token;
}

token PeekToken(parser *Parser, int Depth)
{
	for(int i = 0; i < Depth; ++i)
	{
		token Token = Parser->Tokens[Parser->TokenIndex + i];
		if(Token.Type == T_EOF)
		{
			RaiseError(true, Token.ErrorInfo, "Unexpected End of File");
		}
	}

	token Token = Parser->Tokens[Parser->TokenIndex + Depth];
	return Token;
}

token PeekToken(parser *Parser)
{
	return PeekToken(Parser, 0);
}

token EatToken(parser *Parser, token_type Type, b32 Abort)
{
	token Token = PeekToken(Parser);
	if(Token.Type != Type)
	{
		if(Parser->TokenIndex > 0)
		{
			token LastToken = Parser->Tokens[Parser->TokenIndex-1];
			string Name = {};
			if(LastToken.Type == T_ID)
				Name = *LastToken.ID;
			else
				Name = MakeString(GetTokenName(LastToken.Type));

			RaiseError(Abort, LastToken.ErrorInfo,
					"Expected %s after %.*s", GetTokenName(Type), Name.Size, Name.Data);
		}
		else
		{
			RaiseError(Abort, Token.ErrorInfo, "Unexpected token!\nExpected: %s\nGot: %s", GetTokenName(Type), GetTokenName(Token.Type));
		}
		return token {};
	}
	GetToken(Parser);
	return Token;
}

token EatToken(parser *Parser, char C, b32 Abort)
{
	return EatToken(Parser, (token_type)C, Abort);
}

parse_result ParseTokens(file *F, slice<string> ConfigIDs)
{
	dynamic<node *>Nodes = {};

	parser Parser = {};
	Parser.Tokens = F->Tokens;
	Parser.TokenIndex = 0;
	Parser.Current = F->Tokens;
	Parser.CurrentlyPublic = true;
	Parser.NoItemLists = true;


	ForArray(Idx, ConfigIDs)
	{
		Parser.ConfigIDs.Push(ConfigIDs[Idx]);
	}

	if(Parser.Current->Type != T_MODULE)
	{
		RaiseError(false, Parser.Current->ErrorInfo, "Expected module keyword at the start of file");
		return parse_result {};
	}
	EatToken(&Parser, T_MODULE, true);
	if(Parser.Current->Type != T_ID)
	{
		RaiseError(false, Parser.Current->ErrorInfo, "Expected module name at the start of file");
		return parse_result {};
	}
	Parser.ModuleName = *EatToken(&Parser, T_ID, true).ID;

	size_t TokenCount = ArrLen(F->Tokens);
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

	if(Parser.ScopeLevel > 0)
	{
		RaiseError(false, *Nodes[Nodes.Count-1]->ErrorInfo, "Unexpected EOF, unterminated scope");
	}

	parse_result Result = {};
	Result.Nodes = Nodes;
	Result.Imports = SliceFromArray(Parser.Imported);
	Result.DynamicLibraries = SliceFromArray(Parser.LoadedDynamicLibs);
	Result.ModuleName = Parser.ModuleName;
	Result.File = F;

	return Result;
}

void ParseImport(parser *Parser, const string *NewStyleName)
{
	ERROR_INFO;
	GetToken(Parser);
	bool Failed = false;
	string DirectName = STR_LIT("");
	string FileName = STR_LIT("");
	string RelativePath = STR_LIT("");

	if(Parser->Current->Type == T_ID)
	{
		token T = EatToken(Parser, T_ID, true);
		DirectName = *T.ID;
	}
	else
	{
		token T = EatToken(Parser, T_STR, true);
		FileName = *T.ID;
		RelativePath = MakeString(T.ErrorInfo.FileName);
		if(FileName.Size == 0)
		{
			RaiseError(false, T.ErrorInfo, "Empty string after #import is not valid");
			Failed = true;
		}
		else if(!PipelineDoFile(FileName, RelativePath))
		{
			string Checked = GetLookupPathsPrintable(FileName, RelativePath);
			RaiseError(false, T.ErrorInfo, "Cannot find imported file %.*s\nChecked paths:\n%.*s",
					FileName.Size, FileName.Data, Checked.Size, Checked.Data);
			Failed = true;
		}
	}

	string *As = NULL;
	if(Parser->Current->Type == T_AS)
	{
		token AsKeyword = GetToken(Parser);

		if(NewStyleName)
		{
			RaiseError(false, AsKeyword.ErrorInfo, "Cannot combine `as` import with new style name :: #import");
		}

		if(Parser->Current->Type == T_PTR)
		{
			GetToken(Parser);
			string Any = STR_LIT("*");
			As = DupeType(Any, string);
		}
		else
		{
			As = EatToken(Parser, T_ID, true).ID;
		}
	}

	if(!Failed)
	{
		needs_resolving_import Imported = {
			.FileName = FileName,
			.RelativePath = RelativePath,
			.Name = DirectName,
			.As = NewStyleName ? *NewStyleName : (As ? *As : STR_LIT("")),
			.ErrorInfo = ErrorInfo,
		};

		if(NewStyleName && *NewStyleName == STR_LIT("_"))
		{
			Imported.As = STR_LIT("*");
		}


		Parser->Imported.Push(Imported);
	}
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
	ERROR_INFO;
	node *Expression = NULL;
	if(Parser->Current->Type != T_CLOSEBRACKET)
	{
		Expression = ParseExpression(Parser);
	}

	EatToken(Parser, T_CLOSEBRACKET, true);
	node *ID = ParseType(Parser);
	if(ID == NULL)
	{
		RaiseError(false, Parser->Current->ErrorInfo, "Expected type after [] for declaring an array");
		string tmp = STR_LIT("invalid");
		ID = MakeID(ErrorInfo, DupeType(tmp, string));
	}

	return MakeArrayType(ID->ErrorInfo, ID, Expression);
}

slice<node *> Delimited(parser *Parser, token_type Deliminator, node *(*Fn)(parser *))
{
	b32 SaveILists = Parser->NoItemLists;
	Parser->NoItemLists = true;

	dynamic<node *>Nodes{};
	while(true)
	{
		node *Node = Fn(Parser);
		if(Node)
			Nodes.Push(Node);
		if(Deliminator == 0)
		{
			if(Node == NULL)
				break;
		}
		else
		{
			if(PeekToken(Parser).Type != Deliminator)
				break;
			GetToken(Parser);
		}
	}
	Parser->NoItemLists = SaveILists;
	return SliceFromArray(Nodes);
}

slice<node *> Delimited(parser *Parser, char Deliminator, node *(*Fn)(parser *))
{
	return Delimited(Parser, (token_type)Deliminator, Fn);
}


string MakeLambdaName(const error_info *Info)
{
	string_builder Builder = MakeBuilder();
	PushBuilderFormated(&Builder, "__lambda_%s(%d:%d)", Info->FileName, Info->Range.StartLine, Info->Range.StartChar);
	return MakeString(Builder);
}

node *ParseEnum(parser *Parser)
{
	ERROR_INFO;
	GetToken(Parser);
	token NameT = GetToken(Parser);
	string *EnumName = NameT.ID;
	if(NameT.Type != T_ID)
	{
		RaiseError(false, *ErrorInfo, "Expected enum name after `enum` keyword");
		EnumName = &ErrorID;
	}
	node *TypeNode = NULL;
	if(Parser->Current->Type == T_DECL)
	{
		EatToken(Parser, T_DECL, true);
		TypeNode = ParseType(Parser);
	}

	EatToken(Parser, T_STARTSCOPE, true);
	auto ParseEnumMembers = [](parser *Parser) -> node* {
		if(Parser->Current->Type == T_ENDSCOPE)
			return NULL;

		ERROR_INFO;
		token Name = EatToken(Parser, T_ID, true);
		node *Expression = NULL;
		if(Parser->Current->Type == T_EQ)
		{
			EatToken(Parser, T_EQ, true);
			Expression = ParseExpression(Parser);
		}
		return MakeListItem(ErrorInfo, Name.ID, Expression);
	};

	slice<node *> Items = Delimited(Parser, ',', ParseEnumMembers);
	EatToken(Parser, T_ENDSCOPE, true);

	string *Name = StructToModuleNamePtr(*EnumName, Parser->ModuleName);

	return MakeEnum(ErrorInfo, Name, Items, TypeNode);
}

string *MakeAnonStructName(const error_info *e)
{
	string_builder b = MakeBuilder();
	PushBuilderFormated(&b, "anon<%s|%d|%d>", e->FileName, e->Range.StartLine, e->Range.StartChar);
	string Result = MakeString(b);
	return DupeType(Result, string);
}

node *ParseStruct(parser *Parser, b32 IsUnion, b32 IsAnon)
{
	ERROR_INFO;
	GetToken(Parser);
	string *StructName = NULL;
	if(!IsAnon)
	{
		token NameT = GetToken(Parser);
		StructName = NameT.ID;
		if(NameT.Type != T_ID)
		{
			RaiseError(false, *ErrorInfo, "Expected struct name after `%s` keyword", IsUnion ? "union" : "struct");
			StructName = &ErrorID;
		}
	}
	else
	{
		StructName = MakeAnonStructName(ErrorInfo);
	}

	dynamic<string> TypeParams = {};
	if(Parser->Current->Type == '<')
	{
		GetToken(Parser);
		while(true)
		{
			token T = EatToken(Parser, T_ID, true);
			TypeParams.Push(*T.ID);
			if(Parser->Current->Type != ',')
				break;
			GetToken(Parser);
		}
		EatToken(Parser, '>', true);
	}

	EatToken(Parser, T_STARTSCOPE, true);

	auto ParseFn = [](parser *P) -> node* {
		if(P->Current->Type == T_ENDSCOPE)
			return NULL;
		error_info *ErrorInfo = &P->Tokens[P->TokenIndex].ErrorInfo;

		if(P->Current->Type == T_USING)
		{
			GetToken(P);
			node *Type = ParseType(P);
			return MakeVar(ErrorInfo, NULL, Type, NULL);
		}

		token ID = EatToken(P, T_ID, false);
		string *MemberName = ID.ID;
		if(ID.Type != T_ID)
			MemberName = &ErrorID;
		EatToken(P, T_DECL, false);
		node *Type = ParseType(P);
		node *Default = NULL;
		if(P->Current->Type == T_EQ)
		{
			GetToken(P);
			b32 SaveILists = P->NoItemLists;
			P->NoItemLists = true;
			Default = ParseExpression(P);
			P->NoItemLists = SaveILists;
		}

		return MakeVar(ErrorInfo, MemberName, Type, Default);
	};
	auto Name = StructToModuleNamePtr(*StructName, Parser->ModuleName);

	slice<node *> Members = Delimited(Parser, ',', ParseFn);
	EatToken(Parser, T_ENDSCOPE, true);
	return MakeStructDecl(ErrorInfo, Name, Members, SliceFromArray(TypeParams), IsUnion);
}

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
				token TypeID = EatToken(Parser, T_ID, false);
				string *Name = TypeID.ID;
				if(Name == NULL)
					Name = &ErrorID;
				Result = MakeSelector(ErrorInfo, ID, TypeID.ID);
			}
			else if(Parser->Current->Type == '<')
			{
				GetToken(Parser);
				slice<node*> Args = Delimited(Parser, ',', [](parser *Parser){
						return ParseType(Parser, false);
						});
				if(Parser->Current->Type != '>' && !ShouldError)
				{
					Result = nullptr;
				}
				else
				{
					EatToken(Parser, '>', false);
					Result = MakeGenericStructType(ErrorInfo, ID, Args);
				}
			}
			else
			{
				Result = ID;
			}
		} break;
		case T_TYPEOF:
		{
			Result = ParseOperand(Parser);
		} break;
		case T_OPENBRACKET:
		{
			GetToken(Parser);
			Result = ParseArrayType(Parser);
		} break;
		case T_QMARK:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Pointer = ParseType(Parser, false);
			if(Pointer == NULL)
			{
				RaiseError(false, *ErrorInfo, "Expected a valid type after `?`");
				Pointer = MakePointerType(ErrorInfo, NULL);
			}
			if(Pointer->Type != AST_PTRTYPE)
			{
				RaiseError(false, *ErrorInfo, "Optional need to be a pointer");
				Pointer = MakePointerType(ErrorInfo, NULL);
			}
			Pointer->PointerType.Flags |= PointerFlag_Optional;
			Result = Pointer;
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
		case T_DOLLAR:
		{
			ERROR_INFO;
			GetToken(Parser);
			token ID = EatToken(Parser, T_ID, false);
			string *Name = ID.ID;
			if(Name == NULL)
				Name = &ErrorID;
			Result = MakeGenericNode(ErrorInfo, ID.ID);
		} break;
		case T_UNION:
		{
			Result = ParseStruct(Parser, true, true);
		} break;
		case T_STRUCT:
		{
			Result = ParseStruct(Parser, false, true);
		} break;
		case T_VOID:
		{
			GetToken(Parser);
		} break;
		default:
		{
		} break;
	}
	if(Result == NULL && ShouldError)
	{
		RaiseError(false, ErrorToken.ErrorInfo, "Expected a type. Found %s", GetTokenName(ErrorToken.Type));
		ERROR_INFO;
		string Int = STR_LIT("int");
		Result = MakeID(ErrorInfo, DupeType(Int, string));
	}
	return Result;
}

void SkipPwdIf(parser *Parser, const error_info *ErrorInfo)
{
	int depth = 1;
	while(depth > 0)
	{
		token_type Type = Parser->Current->Type;
		if(Type == T_STARTSCOPE)
			depth++;
		else if(Type == T_ENDSCOPE)
			depth--;
		else if(Type == T_EOF)
		{
			RaiseError(true, *ErrorInfo, "#if is not terminated");
		}
		GetToken(Parser);
	}
}

node *ParsePwdIf(parser *Parser)
{
	node *Result = NULL;
	while(true)
	{
		ERROR_INFO;
		GetToken(Parser);
		token ID = EatToken(Parser, T_ID, true);
		EatToken(Parser, T_STARTSCOPE, true);
		b32 IsTrue = false;
		ForArray(Idx, Parser->ConfigIDs)
		{
			if(*ID.ID == Parser->ConfigIDs[Idx])
			{
				IsTrue = true;
				break;
			}
		}
		if(IsTrue)
		{
			Result = MakeScope(ErrorInfo, true);
			Parser->ScopeLevel++;
			break;
		}
		else
		{
			SkipPwdIf(Parser, ErrorInfo);
			if(Parser->Current->Type == T_PWDELSE)
			{
				GetToken(Parser);
				EatToken(Parser, T_STARTSCOPE, true);
				Result = MakeScope(ErrorInfo, true);
				Parser->ScopeLevel++;
				break;
			}
			if(Parser->Current->Type != T_PWDELIF)
				break;
		}
	}
	return Result;
}

node *ParseFunctionArgument(parser *Parser)
{
	ERROR_INFO;
	token ID = EatToken(Parser, T_ID, true);
	EatToken(Parser, ':', false);
	node *Type = NULL;
	node *Default = NULL;
	if(Parser->Current->Type == T_VARARG)
	{
		Type = (node *)0x1;
		EatToken(Parser, T_VARARG, true);
	}
	else
	{
		Type = NULL;
		if(Parser->Current->Type != T_EQ)
			Type = ParseType(Parser);

		if(Parser->Current->Type == T_EQ)
		{
			GetToken(Parser);
			b32 SaveILists = Parser->NoItemLists;
			Parser->NoItemLists = true;
			Default = ParseExpression(Parser);
			Parser->NoItemLists = SaveILists;
		}
	}
	return MakeVar(ErrorInfo, ID.ID, Type, Default);
}

u32 ParseFunctionFlags(parser *Parser, const string **LinkName)
{
	struct {
		token_type T;
		SymbolFlag F;
	} FlagTokens[] = { {T_FOREIGN, SymbolFlag_Foreign}, {T_INTR, SymbolFlag_Intrinsic}, {T_LINK, SymbolFlag_None}, {T_INLINE, SymbolFlag_Inline}, {T_NORETURN, SymbolFlag_NoReturn} };

	u32 Result = 0;
	size_t Len = ARR_LEN(FlagTokens);
	bool ParsingFlags = true;
	while(ParsingFlags)
	{
		// Quit parsing at the end of the loop if no flag is found
		ParsingFlags = false;
		for(int i = 0; i < Len; ++i)
		{
			auto Check = FlagTokens[i];
			if(Parser->Current->Type == Check.T)
			{
				if(Check.T == T_LINK)
				{
					GetToken(Parser);
					EatToken(Parser, T_EQ, false);
					token Name = EatToken(Parser, T_STR, false);
					if(Name.ID)
						*LinkName = Name.ID;
					else
						*LinkName = &ErrorID;
				}
				else
				{
					Result |= Check.F;
					GetToken(Parser);

					// Continue parsing
					ParsingFlags = true;
				}
			}
		}
	}
	return Result;
}

node *ParseFunctionType(parser *Parser)
{
	ERROR_INFO;
	u32 Flags = 0;
	EatToken(Parser, T_FN, true);

	const string *LinkName = NULL;
	Flags |= ParseFunctionFlags(Parser, &LinkName);

	EatToken(Parser, '(', true);
	slice<node *> Args{};
	if(PeekToken(Parser).Type != T_CLOSEPAREN)
		Args = Delimited(Parser, ',', ParseFunctionArgument);
	EatToken(Parser, ')', true);

	dynamic<node *> ReturnTypes = {};
	if(PeekToken(Parser).Type == T_ARR)
	{
		GetToken(Parser);
		if(Parser->Current->Type == T_OPENPAREN)
		{
			do {
				GetToken(Parser);
				ReturnTypes.Push(ParseType(Parser));
			} while(Parser->Current->Type == T_COMMA);
			EatToken(Parser, T_CLOSEPAREN, true);
		}
		else
		{
			ReturnTypes.Push(ParseType(Parser));
		}
	}

	Flags |= ParseFunctionFlags(Parser, &LinkName);

	return MakeFunction(ErrorInfo, LinkName, Args, SliceFromArray(ReturnTypes), Flags);
}

void ParseBody(parser *Parser, dynamic<node *> &OutBody)
{
	ERROR_INFO;
	uint EnterLevel = Parser->ScopeLevel;

	EatToken(Parser, T_STARTSCOPE, true);
	OutBody.Push(MakeScope(ErrorInfo, true));

	Parser->ScopeLevel++;
	while(true)
	{
		node *Node = ParseNode(Parser);
		if(Node)
			OutBody.Push(Node);
		if(Parser->ScopeLevel == EnterLevel)
		{
			break;
		}
	}
}

void ParseMaybeBody(parser *Parser, dynamic<node *> &OutBody)
{
	if(PeekToken(Parser).Type == T_STARTSCOPE)
	{
		ParseBody(Parser, OutBody);
	}
	else
	{
		node *Node = ParseNode(Parser);
		if(Node)
			OutBody.Push(Node);
	}
}

node *ParseFunctionCall(parser *Parser, node *Operand)
{
	ERROR_INFO;
	if(Operand == NULL)
	{
		RaiseError(false, *ErrorInfo, "Trying to call an invalid expression as a function");
		return MakeCall(ErrorInfo, MakeID(ErrorInfo, &ErrorID), ZeroSlice<node*>());
	}

	b32 SaveILists = Parser->NoItemLists;
	b32 SaveSLists = Parser->NoStructLists;
	Parser->NoStructLists = false;
	Parser->NoItemLists = true;
	dynamic<node *> Args = {};
	EatToken(Parser, T_OPENPAREN, true);
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
			RaiseError(false, *ErrorInfo, "Improper argument formatting\nProper arguments example: call(arg1, arg2)");
			while(PeekToken(Parser).Type != ')')
			{
				GetToken(Parser);
			}
		}
	}
	EatToken(Parser, T_CLOSEPAREN, true);

	Parser->NoStructLists = SaveSLists;
	Parser->NoItemLists = SaveILists;
	return MakeCall(ErrorInfo, Operand, SliceFromArray(Args));
}

node *ParseIndex(parser *Parser, node *Operand)
{
	ERROR_INFO;
	if(Operand == NULL)
	{
		RaiseError(false, *ErrorInfo, "Trying to index an invalid expression");
		Operand = MakeID(ErrorInfo, &ErrorID);
	}

	EatToken(Parser, T_OPENBRACKET, true);
	node *IndexExpression = ParseExpression(Parser);
	EatToken(Parser, T_CLOSEBRACKET, true);

	return MakeIndex(ErrorInfo, Operand, IndexExpression);
}

node *ParseList(parser *Parser, node *Operand)
{
	ERROR_INFO;
	GetToken(Parser);
	node *Result = NULL;
	if(Parser->Current->Type == T_ENDSCOPE)
	{
		Result = MakeTypeList(ErrorInfo, Operand, ZeroSlice<node *>());
	}
	else
	{
		auto ParseListItems = [](parser *Parser) -> node* {
			ERROR_INFO;
			if(Parser->Current->Type == T_ENDSCOPE)
				return NULL;

			const string *Name = NULL;
			if(PeekToken(Parser, 1).Type == T_EQ)
			{
				ErrorInfo = &Parser->Tokens[Parser->TokenIndex+1].ErrorInfo;
				token ID = EatToken(Parser, T_ID, false);
				if(ID.Type == 0)
				{
					GetToken(Parser);
					Name = &ErrorID;
				}
				else
					Name = ID.ID;
				EatToken(Parser, T_EQ, true);
			}
			node *Expression = ParseExpression(Parser);
			return MakeListItem(ErrorInfo, Name, Expression);
		};
		slice<node *> Items = Delimited(Parser, ',', ParseListItems);
		Result = MakeTypeList(ErrorInfo, Operand, Items);
	}
	EatToken(Parser, T_ENDSCOPE, true);
	return Result;
}

node *ParseSelectors(parser *Parser, node *Operand)
{
	ERROR_INFO;
	EatToken(Parser, T_DOT, true);
	if(Parser->Current->Type == T_STARTSCOPE && Operand == NULL)
	{
		return ParseList(Parser, Operand);
	}
	else
	{
		while(true)
		{
			token ID = EatToken(Parser, T_ID, false);
			string *Name = NULL;
			if(ID.Type == 0)
				Name = &ErrorID;
			else
				Name = ID.ID;

			CopyRangeEndToErrorInfo(ErrorInfo, &ID.ErrorInfo);
			Operand = MakeSelector(ErrorInfo, Operand, Name);
			if(Parser->Current->Type != T_DOT)
				break;
			ErrorInfo = &Parser->Tokens[Parser->TokenIndex].ErrorInfo;
			EatToken(Parser, T_DOT, true);
		}

		return Operand;
	}
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
			case T_STARTSCOPE:
			{
				if(!Parser->NoStructLists)
					Operand = ParseList(Parser, Operand);
				else
					Loop = false;
			} break;
			case T_COMMA:
			{
				if(!Parser->NoItemLists)
				{
					Parser->NoItemLists = true;
					ERROR_INFO;
					dynamic<node *> Items = {};
					Items.Push(Operand);
					while(Parser->Current->Type == T_COMMA)
					{
						ERROR_INFO;
						GetToken(Parser);
						node *N = ParseExpression(Parser);
						if(!N)
						{
							RaiseError(false, *ErrorInfo, "Invalid item after comma in list");
							continue;
						}
						Items.Push(N);
					}
					Operand = MakeList(ErrorInfo, SliceFromArray(Items));
					Parser->NoItemLists = false;
				}
				Loop = false;

			} break;
			case T_AS:
			{
				GetToken(Parser);
				ERROR_INFO;
				node *Type = ParseType(Parser, true);
				Operand = MakeCast(ErrorInfo, Operand, Type, INVALID_TYPE, INVALID_TYPE);
			} break;
			default:
			{
				Loop = false;
			} break;
		}
	}
	return Operand;
}

bool LooksLikeGenericTypeInit(parser *Parser)
{
	if(Parser->Current->Type != T_ID)
		return false;
	if(PeekToken(Parser, 1).Type != '<')
		return false;

	u64 RewindTo = Parser->TokenIndex;

	node *Node = ParseType(Parser, false);

	Parser->TokenIndex = RewindTo;
	Parser->Current = &Parser->Tokens[Parser->TokenIndex];

	if(Node == nullptr || Node->Type != AST_GENSTRUCTTYPE)
			return false;
	return true;
}

node *ParseOperand(parser *Parser)
{
	token Token = PeekToken(Parser);
	node *Result = NULL;
	switch((int)Token.Type)
	{
		case T_FILE_LOCATION:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeFileLocation(ErrorInfo);
		} break;
		case T_DOT:
		{
			// Detect selector
			// enum_var = .ENUM_MEMBER
			Result = ParseSelectors(Parser, NULL);
		} break;
		case T_IF:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *IfExpr = ParseExpression(Parser);
			if(Parser->Current->Type == T_THEN)
			{
				GetToken(Parser);
			}
			node *IfTrue = ParseExpression(Parser);
			EatToken(Parser, T_ELSE, true);
			node *IfFalse = ParseExpression(Parser);
			Result = MakeIfX(ErrorInfo, IfExpr, IfTrue, IfFalse);
		} break;
		case T_INFO:
		{
			ERROR_INFO;
			GetToken(Parser);

			node *Expr = ParseOperand(Parser);
			if(Expr == NULL) {
				RaiseError(true, *ErrorInfo, "Expected type operand after type_info");
			}
			Result = MakeTypeInfo(ErrorInfo, Expr);
		} break;
#if 0
		case T_AUTOCAST:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = ParseUnary(Parser);
			Result = MakeCast(ErrorInfo, Expr, NULL, 0, 0);
		} break;
		case T_CAST:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Type = ParseType(Parser);
			node *Expr = ParseUnary(Parser);
			Result = MakeCast(ErrorInfo, Expr, Type, INVALID_TYPE, INVALID_TYPE);
		} break;
#endif
		case T_BITCAST:
		case T_NEWCAST:
		{
			ERROR_INFO;
			GetToken(Parser);
			EatToken(Parser, '(', true);
			auto WasNoItemLists = Parser->NoItemLists;
			Parser->NoItemLists = true;

			node *Type = ParseType(Parser);
			EatToken(Parser, ',', true);
			node *Expr = ParseExpression(Parser);
			EatToken(Parser, ')', true);

			Parser->NoItemLists = WasNoItemLists;
			Result = MakeCast(ErrorInfo, Expr, Type, INVALID_TYPE, INVALID_TYPE);
			Result->Cast.IsBitCast = Token.Type == T_BITCAST;
		} break;
		case T_EMBED_BIN:
		{
			ERROR_INFO;
			GetToken(Parser);
			token T = EatToken(Parser, T_STR, true);
			Result = MakeEmbed(ErrorInfo, T.ID, false);
		} break;
		case T_EMBED_STR:
		{
			ERROR_INFO;
			GetToken(Parser);
			token T = EatToken(Parser, T_STR, true);
			Result = MakeEmbed(ErrorInfo, T.ID, true);
		} break;
		case T_TYPEOF:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = ParseUnary(Parser);
			Result = MakeTypeOf(ErrorInfo, Expr);
		} break;
		case T_SIZEOF:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = ParseUnary(Parser);
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
		case T_SWITCH:
		{
			ERROR_INFO;
			GetToken(Parser);
			b32 NoStructLists = Parser->NoStructLists;
			Parser->NoStructLists = true;
			node *Expr = ParseExpression(Parser);
			Parser->NoStructLists = NoStructLists;
			EatToken(Parser, T_STARTSCOPE, true);

			auto ParseFn = [](parser *Parser) -> node* {
				ERROR_INFO;
				if(Parser->Current->Type != T_CASE)
					return NULL;

				EatToken(Parser, T_CASE, true);

				dynamic<node *> List = {};
				node *Value = NULL;
				do {
					if(Parser->Current->Type == T_COMMA)
					{
						if(Value == NULL)
						{
							RaiseError(false, Parser->Current->ErrorInfo, "Expected case value before comma");
							EatToken(Parser, ',', false);
							continue;
						}
						else
						{
							if(!List.IsValid())
								List.Push(Value);
							EatToken(Parser, ',', false);
						}

					}

					Value = ParseExpression(Parser);
					if(List.IsValid())
						List.Push(Value);
				} while(Parser->Current->Type == T_COMMA);


				if(List.IsValid())
				{
					Value = MakeList(ErrorInfo, SliceFromArray(List));
				}

				EatToken(Parser, ':', false);
				dynamic<node *> Body = {};
				ParseMaybeBody(Parser, Body);
				return MakeCase(ErrorInfo, Value, SliceFromArray(Body));
			};
			
			slice<node *> Cases = Delimited(Parser, 0, ParseFn);

			EatToken(Parser, T_ENDSCOPE, true);
			Result = MakeSwitch(ErrorInfo, Expr, Cases);
		} break;
		case T_OPENBRACKET:
		{
			GetToken(Parser);
			Result = ParseArrayType(Parser);
		} break;
		case T_ID:
		{
			ERROR_INFO;
			const string *Name = PeekToken(Parser).ID;
			static const string ReservedIDs[] = {
				STR_LIT("null"),
				STR_LIT("true"),
				STR_LIT("false"),
				STR_LIT("inf"),
				STR_LIT("nan"),
			};
			int Found = -1;
			for(int i = 0; i < ARR_LEN(ReservedIDs); ++i) {
				if(*Name == ReservedIDs[i]) {
					Found = i;
					break;
				}
			}
			if(Found != -1)
			{
				GetToken(Parser);
				Result = MakeReserve(ErrorInfo, (reserved)Found);
			}
			else
			{
				if(LooksLikeGenericTypeInit(Parser))
				{
					Result = ParseType(Parser);
				}
				else
				{
					GetToken(Parser);
					Result = MakeID(ErrorInfo, Name);
				}
			}
		} break;
		case T_CHAR:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeCharLiteral(ErrorInfo, (u32)(u64)Token.ID);
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
			EatToken(Parser, T_CLOSEPAREN, true);
		} break;
		case T_RUN:
		{
			ERROR_INFO;
			GetToken(Parser);
			dynamic<node *> Body = {};
			Body.Push(ParseExpression(Parser));
			Result = MakeRun(ErrorInfo, SliceFromArray(Body));
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
		case T_MINUS:
		case T_BANG:
		case T_QMARK:
		case T_ADDROF:
		case T_BITNOT:
		case T_PTR:
		{
			GetToken(Parser);
			if(Token.Type == T_PTR)
			{
				if(Parser->Current->Type == T_VOID)
				{
					GetToken(Parser);
					return MakePointerType(ErrorInfo, NULL);
				}
				else if(Parser->Current->Type == T_SEMICOL)
				{
					return MakePointerType(ErrorInfo, NULL);
				}
			}
			Result = MakeUnary(ErrorInfo, ParseUnary(Parser), Token.Type);
			return Result;
		} break;
		default: break;
	}

	b32 SaveILists = Parser->NoItemLists;
	Parser->NoItemLists = true;

	node *Operand = ParseOperand(Parser);

	Parser->NoItemLists = SaveILists;
	if(!Operand)
	{
		RaiseError(true, Token.ErrorInfo, "Expected operand in expression, got %s", GetTokenName(Parser->Current->Type));
	}
	node *Atom = ParseAtom(Parser, Operand);
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

		case '&':
			return MakePrecedence(50, 51);

		case '^':
			return MakePrecedence(40, 41);

		case '|':
			return MakePrecedence(30, 31);


		case '<':
		case '>':
		case T_GEQ:
		case T_LEQ:
			return MakePrecedence(20, 21);

		case T_EQEQ:
		case T_NEQ:
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
		case T_SLEQ:
		case T_SREQ:
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

node *ParseDeclaration(parser *Parser, b32 IsShadow, node *LHS, b32 IsStatic=false)
{
	b32 IsConst = false;

	ERROR_INFO;

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
			RaiseError(false, *ErrorInfo, "Expected declaration!");
			IsConst = false;
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

		if(Parser->Current->Type != T_EQ && Parser->Current->Type != T_DECL)
		{
			HasExpression = false;
		}
		else
		{
			token T = GetToken(Parser);
			if(T.Type == T_DECL)
			{
				IsConst = true;
			}
		}
	}
	node *Expression = NULL;
	if(HasExpression)
	{
		if(Parser->Current->Type == T_IMPORT)
		{
			if(!IsConst)
			{
				RaiseError(false, Decl.ErrorInfo, "Import declaration must be constant!");
			}
			if(MaybeTypeNode)
			{
				RaiseError(false, MaybeType.ErrorInfo, "Import declaration cannot have an explicit type!");
			}
			if(LHS->Type != AST_ID)
			{
				RaiseError(true, *LHS->ErrorInfo, "Left-hand side of import declaration must be a single identifier.");
			}
			else
			{
				ParseImport(Parser, LHS->ID.Name);
				return (node *)0x1;
			}
		}
		Expression = ParseExpression(Parser);
	}
	u32 Flags = IsConst ? SymbolFlag_Const : 0 | IsShadow ? SymbolFlag_Shadow : 0;
	if(IsStatic)
	{
		Flags |= SymbolFlag_LocalStatic;
	}
	return MakeDecl(ErrorInfo, LHS, Expression, MaybeTypeNode, Flags);
}

node *ParseNode(parser *Parser, b32 ExpectSemicolon)
{
	token Token = PeekToken(Parser);
	node *Result = NULL;
	b32 IsParsingStaticVariable = false;
	switch(Token.Type)
	{
		case T_IMPORT:
		{
			ParseImport(Parser, NULL);
			ExpectSemicolon = false;
		} break;
		case T_RUN:
		{
			ERROR_INFO;
			GetToken(Parser);
			dynamic<node *> Body = {};
			ParseMaybeBody(Parser, Body);
			Result = MakeRun(ErrorInfo, SliceFromArray(Body));
			ExpectSemicolon = false;
		} break;
		case T_DEFER:
		{
			ERROR_INFO;
			GetToken(Parser);
			dynamic<node *> Body = {};
			ParseMaybeBody(Parser, Body);
			Result = MakeDefer(ErrorInfo, Body);
			ExpectSemicolon = false;
		} break;
		case T_ASSERT:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = ParseExpression(Parser);
			Result = MakeAssert(ErrorInfo, Expr);
			ExpectSemicolon = false;
		} break;
		case T_USING:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = ParseUnary(Parser);
			Result = MakeTypedExpr(ErrorInfo, AST_USING, Expr);
		} break;
#if 0
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
#endif
		case T_STATIC:
		{
			GetToken(Parser);
			if(Parser->Current->Type != T_ID)
			{
				RaiseError(true, Parser->Current->ErrorInfo, "Expected declaration after #static");
			}
			IsParsingStaticVariable = true;
		}
		case T_ID:
		{
			b32 SaveILists = Parser->NoItemLists;
			Parser->NoItemLists = false;

			node *LHS = ParseExpression(Parser);
			if(Parser->Current->Type == T_DECL || Parser->Current->Type == T_CONST)
				Result = ParseDeclaration(Parser, false, LHS, IsParsingStaticVariable);
			else
				Result = LHS;

			if(Result == (node *)0x1)
				Result = NULL;

			Parser->NoItemLists = SaveILists;
		} break;
		case T_BREAK:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeBreak(ErrorInfo);
		} break;
		case T_CONTINUE:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeContinue(ErrorInfo);
		} break;
		case T_RETURN:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = NULL;
			if(Parser->Current->Type != ';')
			{
				b32 Save = Parser->NoItemLists;
				Parser->NoItemLists = false;
				Expr = ParseExpression(Parser);
				Parser->NoItemLists = Save;
			}
			Result = MakeReturn(ErrorInfo, Expr);
		} break;
		case T_YIELD:
		{
			ERROR_INFO;
			GetToken(Parser);
			node *Expr = NULL;
			if(Parser->Current->Type != ';')
			{
				b32 Save = Parser->NoItemLists;
				Parser->NoItemLists = false;
				Expr = ParseExpression(Parser);
				Parser->NoItemLists = Save;
			}
			Result = MakeTypedExpr(ErrorInfo, AST_YIELD, Expr);
		} break;
		case T_IF:
		{
			ERROR_INFO;
			GetToken(Parser);
			b32 NoStructLists = Parser->NoStructLists;
			Parser->NoStructLists = true;
			node *IfExpression = ParseExpression(Parser);
			Parser->NoStructLists = NoStructLists;
			Result = MakeIf(ErrorInfo, IfExpression);
			ParseMaybeBody(Parser, Result->If.Body);
			
			if(PeekToken(Parser).Type == T_ELSE)
			{
				GetToken(Parser);
				ParseMaybeBody(Parser, Result->If.Else);
			}

			ExpectSemicolon = false;
		} break;
		case T_FOR:
		{
			ERROR_INFO;
			GetToken(Parser);
			using ft = for_type;

			ft Kind = ft::C;
			node *FirstNode = NULL;
			if(PeekToken(Parser).Type == T_STARTSCOPE)
			{
				Kind = ft::Infinite;
			}
			else
			{
				b32 SaveLists = Parser->NoStructLists;
				Parser->NoStructLists = true;

				FirstNode = ParseNode(Parser, false);

				Parser->NoStructLists = SaveLists;

				token t = PeekToken(Parser);
				if(t.Type == T_IN)
				{
					if(FirstNode->Type != AST_ID && FirstNode->Type != AST_LIST)
					{
						if(FirstNode->Type == AST_UNARY && FirstNode->Unary.Op == '&')
						{}
						else
						{
							RaiseError(true, *FirstNode->ErrorInfo, "Expected names of iterators before `in` keyword");
						}
					}
					Kind = ft::It;
				}
				else if(t.Type == T_SEMICOL || FirstNode == NULL)
				{
					Kind = ft::C;
				}
				else
				{
					Kind = ft::While;
				}

			}

			switch(Kind)
			{
				case ft::C:
				{
					node *ForInit = FirstNode;
					node *ForExpr = NULL;
					node *ForIncr = NULL;

					if(ForInit != NULL) {
						EatToken(Parser, ';', false);
					}

					b32 nsl = Parser->NoStructLists;
					Parser->NoStructLists = true;
					if(PeekToken(Parser).Type != ';')
						ForExpr = ParseExpression(Parser);
					EatToken(Parser, ';', false);

					if(PeekToken(Parser).Type != T_STARTSCOPE)
						ForIncr = ParseExpression(Parser);

					Parser->NoStructLists = nsl;
					Result = MakeFor(ErrorInfo, ForInit, ForExpr, ForIncr, ft::C);
				} break;
				case ft::It:
				{
					ERROR_INFO;

					node *It = FirstNode;
					EatToken(Parser, T_IN, true);
					b32 nsl = Parser->NoStructLists;
					Parser->NoStructLists = true;
					node *Array = ParseExpression(Parser);
					Parser->NoStructLists = nsl;

					Result = MakeFor(ErrorInfo, It, Array, NULL, ft::It);
				} break;
				case ft::While:
				{
					node *WhileExpr = FirstNode;
					Result = MakeFor(ErrorInfo, WhileExpr, NULL, NULL, ft::While);
				} break;
				case ft::Infinite:
				{
					Result = MakeFor(ErrorInfo, NULL, NULL, NULL, ft::Infinite);
				} break;
			}

			ParseMaybeBody(Parser, Result->For.Body);
			ExpectSemicolon = false;
		} break;
		case T_STARTSCOPE:
		{
			ERROR_INFO;
			GetToken(Parser);
			Result = MakeScope(ErrorInfo, true);
			Parser->ScopeLevel++;
			ExpectSemicolon = false;
		} break;
		case T_ENDSCOPE:
		{
			ERROR_INFO;
			GetToken(Parser);
			if(Parser->ScopeLevel-- == 0)
			{
				RaiseError(false, Token.ErrorInfo, "No matching opening '{' for '}'");
			}
			ExpectSemicolon = false;
			Result = MakeScope(ErrorInfo, false);
		} break;
		case T_PWDIF:
		{
			Result = ParsePwdIf(Parser);
			ExpectSemicolon = false;
		} break;
		case T_PWDELSE:
		case T_PWDELIF:
		{
			ERROR_INFO;
			// skip it
			GetToken(Parser);

			if(Token.Type == T_PWDELIF)
				EatToken(Parser, T_ID, false);

			EatToken(Parser, T_STARTSCOPE, true);
			SkipPwdIf(Parser, ErrorInfo);
			ExpectSemicolon = false;
		} break;
		case T_SEMICOL:
		{
		} break;
		default:
		{
			Result = ParseExpression(Parser);
		} break;
	}
	if(ExpectSemicolon)
		EatToken(Parser, ';', false);
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
	//Builder += "";
	Builder += ModuleName;
	Builder += ".";
	Builder += StructName;
	return MakeString(Builder);
}

node *ParseTopLevel(parser *Parser)
{
	node *Result = NULL;
	b32 IsStructUnion = false;
	node *ProfileCallback = NULL;
	switch(Parser->Current->Type)
	{
		case T_LOAD_SYSTEM_DL:
		{
			ERROR_INFO;
			GetToken(Parser);
			token T = EatToken(Parser, T_STR, false);
			if(T.ID == NULL)
				return (node *)0x1;
			DLIB Lib = OpenLibrary(T.ID->Data);
			if(Lib == NULL)
			{
				RaiseError(false, *ErrorInfo, "Failed to load system dynamic library:\n%s", DLGetLastError());
			}
			else
			{
				Parser->LoadedDynamicLibs.Push(Lib);
			}

			return (node *)0x1;
		} break;
		case T_LOAD_DL:
		{
			ERROR_INFO;
			GetToken(Parser);
			token T = EatToken(Parser, T_STR, false);
			if(T.ID == NULL)
				return (node *)0x1;

			int FileNameSize = strlen(ErrorInfo->FileName);
			auto b = MakeBuilder();
			int End = FileNameSize-1;
			for(; End >= 0
					&& ErrorInfo->FileName[End] != '/'
					&& ErrorInfo->FileName[End] != '\\'
					; --End);
			string Tmp = {ErrorInfo->FileName, (size_t)End+1};

			b += Tmp;
			b += *T.ID;
			string Path = MakeString(b);
			LDEBUG("LIB: %s", Path.Data);
			DLIB Lib = OpenLibrary(Path.Data);
			if(Lib == NULL)
			{
				RaiseError(false, *ErrorInfo, "Failed to load local dynamic library:\n%s", DLGetLastError());
			}
			else
			{
				Parser->LoadedDynamicLibs.Push(Lib);
			}
			return (node *)0x1;
		} break;
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
		case T_PROFILE:
		{
			GetToken(Parser);
			EatToken(Parser, T_EQ, false);
			ProfileCallback = ParseExpression(Parser);
		}
		// fallthrough
		case T_ID:
		{
			b32 SaveILists = Parser->NoItemLists;
			Parser->NoItemLists = false;
			node *LHS = ParseExpression(Parser);
			node *Decl = ParseDeclaration(Parser, false, LHS);
			Parser->NoItemLists = SaveILists;
			if(Decl == (node *)0x1)
			{
				Result = Decl;
				EatToken(Parser, ';', false);
			}
			else if(Decl->Decl.Expression && Decl->Decl.Expression->Type == AST_FN)
			{
				const string *LHSName = NULL;
				if(LHS->Type != AST_ID)
				{
					RaiseError(false, *LHS->ErrorInfo, "Expected a name on the left of function ");
					LHSName = &ErrorID;
				}
				else
				{
					LHSName = LHS->ID.Name;
				}

				node *Fn = Decl->Decl.Expression;
				Assert(Fn);
				if((Decl->Decl.Flags & SymbolFlag_Const) == 0)
				{
					RaiseError(false, *Decl->ErrorInfo, "Global function declaration needs to be constant");
				}
				Fn->Fn.ProfileCallback = ProfileCallback;
				Fn->Fn.Name = LHSName;
				if(Parser->CurrentlyPublic)
					Fn->Fn.Flags |= SymbolFlag_Public;
				if(!Fn->Fn.Body.IsValid())
					EatToken(Parser, ';', false);
				Result = Fn;
			}
			else if(Decl->Decl.Type && Decl->Decl.Type->Type == AST_FN)
			{
				const string *LHSName = NULL;
				if(LHS->Type != AST_ID)
				{
					RaiseError(false, *LHS->ErrorInfo, "Expected a name on the left of function ");
					LHSName = &ErrorID;
				}
				else
				{
					LHSName = LHS->ID.Name;
				}
				node *Fn = Decl->Decl.Type;
				Fn->Fn.ProfileCallback = ProfileCallback;
				Fn->Fn.Name = LHSName;
				if(Parser->CurrentlyPublic)
					Fn->Fn.Flags |= SymbolFlag_Public;
				if(!Fn->Fn.Body.IsValid())
					EatToken(Parser, ';', false);
				Result = Fn;
			}
			else
			{
				if(Parser->CurrentlyPublic)
					Decl->Decl.Flags |= SymbolFlag_Public;
				Result = Decl;

				if(Parser->Current->Type == T_LINK)
				{
					ERROR_INFO;
					GetToken(Parser);
					EatToken(Parser, T_EQ, false);
					token S = EatToken(Parser, T_STR, true);
					if(LHS->Type != AST_ID)
					{
						RaiseError(true, *ErrorInfo, "External link global must be defined as a single variable!");
					}
					if(Decl->Decl.Expression)
					{
						RaiseError(true, *ErrorInfo, "External link global cannot have an initilizer");
					}
					Decl->Decl.LinkName = S.ID;
					Decl->Decl.Flags |= SymbolFlag_Extern;
				}

				EatToken(Parser, ';', false);
			}
		} break;
		case T_IMPORT:
		{
			ParseImport(Parser, NULL);
			Result = (node *)0x1;
		} break;
		case T_ENUM:
		{
			Result = ParseEnum(Parser);
		} break;
		case T_UNION:
		IsStructUnion = true;
		case T_STRUCT:
		{
			Result = ParseStruct(Parser, IsStructUnion, false);
		} break;
		case T_ENDSCOPE:
		{
			ERROR_INFO;
			GetToken(Parser);

			if(Parser->ScopeLevel-- == 0)
			{
				RaiseError(false, *ErrorInfo, "Unexpected `}`");
			}
			Result = (node *)0x1;
		} break;
		case T_PWDIF:
		{
			ParsePwdIf(Parser);
			Result = (node *)0x1;
		} break;
		case T_PWDELSE:
		case T_PWDELIF:
		{
			ERROR_INFO;
			// skip it
			token Token = GetToken(Parser);

			if(Token.Type == T_PWDELIF)
				EatToken(Parser, T_ID, false);

			EatToken(Parser, T_STARTSCOPE, true);
			SkipPwdIf(Parser, ErrorInfo);
			Result = (node *)0x1;
		} break;
		case T_SEMICOL:
		{
			GetToken(Parser);
			Result = (node *)0x1;
		} break;
		case T_RUN:
		{
			ERROR_INFO;
			GetToken(Parser);
			dynamic<node *> Body = {};
			ParseMaybeBody(Parser, Body);
			Result = MakeRun(ErrorInfo, SliceFromArray(Body));
		} break;
		case T_EOF:
		{
			return NULL;
		} break;
		default:
		{
#if defined(DEBUG)
			LERROR("%d", Parser->Current->Type);
#endif
			RaiseError(false, Parser->Current->ErrorInfo, "Unexpected top level expression: %s", GetTokenName(Parser->Current->Type));
			GetToken(Parser);
		} break;
	}

	return Result;
}

dynamic<node *> CopyNodeDynamic(dynamic<node *> Body)
{
	dynamic<node *> Result = {};
	ForArray(Idx, Body)
	{
		Result.Push(CopyASTNode(Body[Idx]));
	}

	return Result;
}

slice<node *> CopyNodeSlice(slice<node *> Body)
{
	array<node *> Result(Body.Count);
	ForArray(Idx, Body)
	{
		Result[Idx] = CopyASTNode(Body[Idx]);
	}

	return SliceFromArray(Result);
}

slice<u32> CopyTypeSlice(slice<u32> Body)
{
	array<u32> Result(Body.Count);
	ForArray(Idx, Body)
	{
		Result[Idx] = Body[Idx];
	}

	return SliceFromArray(Result);
}


node *CopyASTNode(node *N)
{
	if(!N) return NULL;

	node *R = AllocateNode(N->ErrorInfo, N->Type);
	
	switch (N->Type)
	{
		case AST_INVALID: 
			unreachable; 
			break;

		case AST_RUN:
		{
			R->Run.Body = CopyNodeSlice(N->Run.Body);
			R->Run.TypeIdx = N->Run.TypeIdx;
			R->Run.IsExprRun = N->Run.IsExprRun;
		} break;

		case AST_YIELD:
		{
			R->TypedExpr.Expr = CopyASTNode(N->TypedExpr.Expr);
			R->TypedExpr.TypeIdx = N->TypedExpr.TypeIdx;
		} break;

		case AST_USING:
		{
			R->TypedExpr.Expr = CopyASTNode(N->TypedExpr.Expr);
			R->TypedExpr.TypeIdx = N->TypedExpr.TypeIdx;
		} break;

		case AST_ASSERT:
		{
			R->Assert.Expr = CopyASTNode(N->Assert.Expr);
		} break;

		case AST_VAR:
		{
			R->Var.Name = N->Var.Name;
			R->Var.Type = N->Var.Type;
			R->Var.IsAutoDefineGeneric = N->Var.IsAutoDefineGeneric;
			R->Var.TypeNode = CopyASTNode(N->Var.TypeNode);
			R->Var.Default = CopyASTNode(N->Var.Default);
		} break;

		case AST_LIST:
		{
			R->List.Nodes = CopyNodeSlice(N->List.Nodes);
			R->List.Types = CopyTypeSlice(N->List.Types);
			R->List.WholeType = N->List.WholeType;
		} break;

		case AST_EMBED:
		{
			R->Embed.IsString = N->Embed.IsString;
			R->Embed.Content  = N->Embed.Content;
			R->Embed.FileName = N->Embed.FileName;
		} break;

		case AST_CHARLIT:
		{
			R->CharLiteral.C = N->CharLiteral.C;
		} break;

		case AST_CONSTANT:
		{
			R->Constant.Value = N->Constant.Value;
			R->Constant.Type = N->Constant.Type;
		} break;

		case AST_BINARY:
		{
			R->Binary.Left = CopyASTNode(N->Binary.Left);
			R->Binary.Right = CopyASTNode(N->Binary.Right);
			R->Binary.Op = N->Binary.Op;
			R->Binary.ExpressionType = N->Binary.ExpressionType;
		} break;

		case AST_UNARY:
		{
			R->Unary.Operand = CopyASTNode(N->Unary.Operand);
			R->Unary.Op = N->Unary.Op;
			R->Unary.Type = N->Unary.Type;
		} break;

		case AST_IFX:
		{
			R->IfX.Expr = CopyASTNode(N->IfX.Expr);
			R->IfX.True = CopyASTNode(N->IfX.True);
			R->IfX.False = CopyASTNode(N->IfX.False);
			R->IfX.TypeIdx = N->IfX.TypeIdx;
		} break;

		case AST_IF:
		{
			R->If.Expression = CopyASTNode(N->If.Expression);
			R->If.Body = CopyNodeDynamic(N->If.Body);
			R->If.Else = CopyNodeDynamic(N->If.Else);
		} break;

		case AST_FOR:
		{
			R->For.Expr1 = CopyASTNode(N->For.Expr1);
			R->For.Expr2 = CopyASTNode(N->For.Expr2);
			R->For.Expr3 = CopyASTNode(N->For.Expr3);
			R->For.Body = CopyNodeDynamic(N->For.Body);
			R->For.Kind = N->For.Kind;
			R->For.ArrayType = N->For.ArrayType;
			R->For.ItType = N->For.ItType;
			R->For.ItByRef = N->For.ItByRef;
		} break;

		case AST_ID:
		{
			R->ID.Name = N->ID.Name;
			R->ID.Type = N->ID.Type;
		} break;

		case AST_DECL:
		{
			R->Decl.LHS = CopyASTNode(N->Decl.LHS);
			R->Decl.Expression = CopyASTNode(N->Decl.Expression);
			R->Decl.Type = CopyASTNode(N->Decl.Type);
			R->Decl.TypeIndex = N->Decl.TypeIndex;
			R->Decl.Flags = N->Decl.Flags;
			R->Decl.LinkName = N->Decl.LinkName;
		} break;

		case AST_CALL:
		{
			R->Call.Fn = CopyASTNode(N->Call.Fn);
			R->Call.Args = CopyNodeSlice(N->Call.Args);
			if(N->Call.SymName.Data)
				R->Call.SymName = MakeString(N->Call.SymName.Data, N->Call.SymName.Size);
			R->Call.Type = N->Call.Type;
			R->Call.ArgTypes = N->Call.ArgTypes; // Shallow-copied
		} break;

		case AST_RETURN:
		{
			R->Return.Expression = CopyASTNode(N->Return.Expression);
			R->Return.TypeIdx = N->Return.TypeIdx;
		} break;

		case AST_PTRTYPE:
		{
			R->PointerType.Pointed = CopyASTNode(N->PointerType.Pointed);
			R->PointerType.Flags = N->PointerType.Flags;
		} break;

		case AST_ARRAYTYPE:
		{
			R->ArrayType.Type = CopyASTNode(N->ArrayType.Type);
			R->ArrayType.Expression = CopyASTNode(N->ArrayType.Expression);
		} break;

		case AST_FN:
		{
			R->Fn.Name = N->Fn.Name;
			R->Fn.Name = N->Fn.LinkName;
			R->Fn.Args = CopyNodeSlice(N->Fn.Args);
			R->Fn.ReturnTypes = CopyNodeSlice(N->Fn.ReturnTypes);
			R->Fn.Body = CopyNodeDynamic(N->Fn.Body);
			R->Fn.TypeIdx = N->Fn.TypeIdx;
			R->Fn.Flags = N->Fn.Flags;
			R->Fn.FnModule = N->Fn.FnModule;
			R->Fn.ProfileCallback = CopyASTNode(N->Fn.ProfileCallback);
			R->Fn.CallbackType = N->Fn.CallbackType;
		} break;

		case AST_CAST:
		{
			R->Cast.Expression = CopyASTNode(N->Cast.Expression);
			R->Cast.TypeNode = CopyASTNode(N->Cast.TypeNode);
			R->Cast.FromType = N->Cast.FromType;
			R->Cast.ToType = N->Cast.ToType;
		} break;

		case AST_TYPELIST:
		{
			R->TypeList.TypeNode = CopyASTNode(N->TypeList.TypeNode);
			R->TypeList.Items = CopyNodeSlice(N->TypeList.Items);
			R->TypeList.Type = N->TypeList.Type;
		} break;

		case AST_INDEX:
		{
			R->Index.Operand = CopyASTNode(N->Index.Operand);
			R->Index.Expression = CopyASTNode(N->Index.Expression);
			R->Index.OperandType = N->Index.OperandType;
			R->Index.IndexedType = N->Index.IndexedType;
			R->Index.ForceNotLoad = N->Index.ForceNotLoad;
		} break;

		case AST_STRUCTDECL:
		{
			R->StructDecl.Name = N->StructDecl.Name;
			R->StructDecl.Members = CopyNodeSlice(N->StructDecl.Members);
			R->StructDecl.IsUnion = N->StructDecl.IsUnion;
			R->StructDecl.TypeParams = N->StructDecl.TypeParams;
		} break;

		case AST_ENUM:
		{
			R->Enum.Name = N->Enum.Name;
			R->Enum.Items = CopyNodeSlice(N->Enum.Items);
			R->Enum.Type = CopyASTNode(N->Enum.Type);
		} break;

		case AST_SELECTOR:
		{
			R->Selector.Operand = CopyASTNode(N->Selector.Operand);
			R->Selector.Member = N->Selector.Member;
			R->Selector.Index = N->Selector.Index;
			R->Selector.Type = N->Selector.Type;
		} break;

		case AST_SIZE:
		{
			R->Size.Expression = CopyASTNode(N->Size.Expression);
			R->Size.Type = N->Size.Type;
		} break;

		case AST_TYPEOF:
		{
			R->TypeOf.Expression = CopyASTNode(N->TypeOf.Expression);
			R->TypeOf.Type = N->TypeOf.Type;
		} break;

		case AST_GENERIC:
		{
			R->Generic.Name = N->Generic.Name;
		} break;

		case AST_RESERVED:
		{
			R->Reserved.ID = N->Reserved.ID;
			R->Reserved.Type = N->Reserved.Type;
		} break;

		case AST_NOP:
		case AST_BREAK:
		case AST_CONTINUE:
			// No additional data to copy
			break;

		case AST_GENSTRUCTTYPE:
		{
			R->GenericStructType.Args = CopyNodeSlice(N->GenericStructType.Args);
			R->GenericStructType.ID = CopyASTNode(N->GenericStructType.ID);
			R->GenericStructType.Analyzed = N->GenericStructType.Analyzed;
		} break;

		case AST_LISTITEM:
		{
			R->Item.Name = N->Item.Name;
			R->Item.Expression = CopyASTNode(N->Item.Expression);
		} break;

		case AST_SWITCH:
		{
			R->Switch.Expression = CopyASTNode(N->Switch.Expression);
			R->Switch.Cases = CopyNodeSlice(N->Switch.Cases);
			R->Switch.SwitchType = N->Switch.SwitchType;
			R->Switch.ReturnType = N->Switch.ReturnType;
		} break;

		case AST_CASE:
		{
			R->Case.Value = CopyASTNode(N->Case.Value);
			R->Case.Body = CopyNodeSlice(N->Case.Body);
		} break;

		case AST_DEFER:
		{
			R->Defer.Body = CopyNodeSlice(N->Defer.Body);
		} break;

		case AST_SCOPE:
		{
			R->ScopeDelimiter.IsUp = N->ScopeDelimiter.IsUp;
		} break;

		case AST_TYPEINFO:
		{
			R->TypeInfoLookup.Type = N->TypeInfoLookup.Type;
			R->TypeInfoLookup.Expression = CopyASTNode(N->TypeInfoLookup.Expression);
		} break;

		case AST_PTRDIFF:
		{
			R->PtrDiff.Left = CopyASTNode(N->PtrDiff.Left);
			R->PtrDiff.Right = CopyASTNode(N->PtrDiff.Right);
			R->PtrDiff.Type = N->PtrDiff.Type;
		} break;

		case AST_FILE_LOCATION:
		{
		} break;
	}


	return R;
}

bool IsOpAssignment(token_type Op)
{
	switch(Op)
	{
			case T_SLEQ:
			case T_SREQ:
			case T_PEQ:
			case T_MEQ:
			case T_TEQ:
			case T_DEQ:
			case T_MODEQ:
			case T_ANDEQ:
			case T_XOREQ:
			case T_OREQ:
			case T_EQ:
			return true;
			default:
			return false;
	}

}

