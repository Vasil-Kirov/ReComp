#include "Lexer.h"
#include "Memory.h"
#include "VString.h"

keyword *KeywordTable = NULL;

token_type GetKeyword(string String)
{
	size_t Count = ArrLen(KeywordTable);
	for(int I = 0; I < Count; ++I)
	{
		if(String == KeywordTable[I].ID)
		{
			return KeywordTable[I].Type;
		}
	}
	return T_ID;
}

inline token MakeToken(token_type Type, error_info ErrorInfo, string *ID)
{
	token Token;
	Token.ErrorInfo = ErrorInfo;
	Token.Type = Type;
	Token.ID = ID;
	return Token;
}

b32 IsIDCharacter(char C)
{
	return C == '_' || isalpha(C);
}

b32 IsNumCharacter(char C, b32 *FoundDot)
{
	if(C == '.')
	{
		if(*FoundDot)
			return false;
		*FoundDot = true;
		return true;
	}
	return C == '-' || isdigit(C);
}

inline string *MakeStringPointer(string String)
{
	string *ID_Ptr = (string *)AllocatePermanent(sizeof(string));
	*ID_Ptr = String;
	return ID_Ptr;
}

char AdvanceC(string *String, error_info *Error)
{
	if(String->Size == 0)
	{
		RaiseError(*Error, "Unexpected end of file"); 
	}
	char Result = *String->Data;
	if(Result == '\n')
	{
		Error->Line++;
		Error->Character = 1;
	}
	else
	{
		Error->Character++;
	}
	String->Data++;
	String->Size--;
	return Result;
}

char PeekCAhead(string *String, int Depth)
{
	if(Depth >= String->Size)
		return 0;
	else
		return String->Data[Depth];
}

char PeekC(string *String)
{
	return *String->Data;
}

file StringToTokens(string String, error_info ErrorInfo, string *OutModuleName)
{
	file Result = {};
	token ModuleName = GetNextToken(&String, &ErrorInfo);
	if(ModuleName.Type != T_ID)
	{
		RaiseError(ModuleName.ErrorInfo, "Expected module name at the start of file"); 
	}
	*OutModuleName = *ModuleName.ID;
	token *Tokens = ArrCreate(token);
	while(String.Size > 0)
	{
		token Token = GetNextToken(&String, &ErrorInfo);
		ArrPush(Tokens, Token);
	}

	Result.Tokens = Tokens;
	return Result;
}

token TokinizeCompilerDirective(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;
	error_info StartErrorInfo = *ErrorInfo;

	AdvanceC(String, ErrorInfo);
	while(IsIDCharacter(PeekC(String)) || isdigit(PeekC(String)))
		AdvanceC(String, ErrorInfo);

	const char *End = String->Data;
	string ID = MakeString(Start, End - Start);
	token_type TokenType = GetKeyword(ID);
	if(TokenType == T_ID)
		RaiseError(StartErrorInfo, "Incorrect compiler directive \"%s\"", ID.Data);

	token Token = {};
	Token.Type = TokenType;
	Token.ErrorInfo = StartErrorInfo;
	return Token;
}

token TokinizeIdentifier(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;

	error_info StartErrorInfo = *ErrorInfo;

	while(IsIDCharacter(PeekC(String)) || isdigit(PeekC(String)))
		AdvanceC(String, ErrorInfo);

	const char *End = String->Data;
	string ID = MakeString(Start, End - Start);
	token_type TokenType = GetKeyword(ID);

	token Token = {};
	Token.Type = TokenType;
	Token.ErrorInfo = StartErrorInfo;
	if(TokenType == T_ID)
		Token.ID = MakeStringPointer(ID);
	return Token;
}

token TokinizeNumber(string *String, error_info *ErrorInfo)
{
	error_info StartErrorInfo = *ErrorInfo;

	if(PeekC(String) == '0' && PeekCAhead(String, 1) == 'b')
	{
		string_builder Builder = MakeBuilder();
		Builder += AdvanceC(String, ErrorInfo);
		Builder += AdvanceC(String, ErrorInfo);
		while(true)
		{
			char c = PeekC(String);
			if(c == '_')
			{
				AdvanceC(String, ErrorInfo);
				continue;
			}

			if(!isdigit(c))
				break;
			if(c != '0' && c != '1')
			{
				RaiseError(StartErrorInfo, "Binary number contains a character that's neither 0 nor 1: %c", c);
			}
			Builder += c;
			AdvanceC(String, ErrorInfo);
		}

		if(Builder.Size == 2)
		{
			RaiseError(StartErrorInfo, "Invalid binary number");
		}

		string Number = MakeString(Builder);
		return MakeToken(T_VAL, StartErrorInfo, MakeStringPointer(Number));
	}
	else if(PeekC(String) == '0' && PeekCAhead(String, 1) == 'x')
	{
		string_builder Builder = MakeBuilder();
		Builder += AdvanceC(String, ErrorInfo);
		Builder += AdvanceC(String, ErrorInfo);
		while(true)
		{
			char c = PeekC(String);
			if(c == '_')
			{
				AdvanceC(String, ErrorInfo);
				continue;
			}

			if(!isalnum(c))
				break;

			if((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
			{}
			else
			{
				RaiseError(StartErrorInfo, "Invalid character in hex number: %c", c);
			}
			Builder += c;
			AdvanceC(String, ErrorInfo);
		}

		if(Builder.Size == 2)
		{
			RaiseError(StartErrorInfo, "Invalid hex number");
		}

		string Number = MakeString(Builder);
		return MakeToken(T_VAL, StartErrorInfo, MakeStringPointer(Number));
	}
	else
	{
		b32 FoundDot = false;
		string_builder Builder = MakeBuilder();
		while(IsNumCharacter(PeekC(String), &FoundDot) || PeekC(String) == '_')
		{
			char c = AdvanceC(String, ErrorInfo);
			if(c != '_')
				Builder += c;
		}

		string Number = MakeString(Builder);
		return MakeToken(T_VAL, StartErrorInfo, MakeStringPointer(Number));
	}
}

token TokinizeSpecialCharacter(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;
	error_info StartErrorInfo = *ErrorInfo;

	char C = AdvanceC(String, ErrorInfo);
	if(String->Size == 0)
	{
		return MakeToken((token_type)C, StartErrorInfo, NULL);
	}
	string PotentialKeywordString = MakeString(Start, 2);
	token_type PotentialKeyword = GetKeyword(PotentialKeywordString);
	if(PotentialKeyword == T_ID)
	{
		if(String->Size > 1)
		{
			PotentialKeywordString = MakeString(Start, 3);
			PotentialKeyword = GetKeyword(PotentialKeywordString);
			if(PotentialKeyword != T_ID)
			{
				AdvanceC(String, ErrorInfo);
				AdvanceC(String, ErrorInfo);
				return MakeToken(PotentialKeyword, StartErrorInfo, NULL);
			}
		}
		return MakeToken((token_type)C, StartErrorInfo, NULL);
	}
	AdvanceC(String, ErrorInfo);

	return MakeToken(PotentialKeyword, StartErrorInfo, NULL);
}

char GetEscapedChar(char ToEscape)
{
	switch (ToEscape)
	{
		case 'n':
		return '\n';
		case '\\':
		return '\\';
		case 't':
		return '\t';
		case '\'':
		return '\'';
	}
	return ToEscape;
}

token TokinizeString(string *String, error_info *ErrorInfo, b32 CString)
{
	error_info StartErrorInfo = *ErrorInfo;
	AdvanceC(String, ErrorInfo);

	string_builder Builder = MakeBuilder();
	while(PeekC(String) != '"')
	{
		char Next = AdvanceC(String, ErrorInfo);
		if(Next == '\\')
		{
			char ToEscape = AdvanceC(String, ErrorInfo);
			char Escaped = GetEscapedChar(ToEscape);
			if(Escaped == ToEscape)
			{
				PushBuilder(&Builder, Next);
				PushBuilder(&Builder, ToEscape);
			}
			else
			{
				PushBuilder(&Builder, Escaped);
			}
		}
		else
		{
			PushBuilder(&Builder, Next);
		}
	}
	AdvanceC(String, ErrorInfo);
	string Tokinized = MakeString(Builder);
	if(CString)
		return MakeToken(T_CSTR, StartErrorInfo, MakeStringPointer(Tokinized));
	else
		return MakeToken(T_STR, StartErrorInfo,  MakeStringPointer(Tokinized));
}

token TokinizeCharLiteral(string *String, error_info *ErrorInfo)
{
	error_info StartErrorInfo = *ErrorInfo;
	char c = AdvanceC(String, ErrorInfo);
	Assert(c == '\'');
	char inside = AdvanceC(String, ErrorInfo);
	if(inside == '\\')
	{
		char escaped = AdvanceC(String, ErrorInfo);
		inside = GetEscapedChar(escaped);
		if(inside == escaped)
		{
			RaiseError(StartErrorInfo, "Unkown escape sequence \\%c", escaped);
		}
	}
	char end = AdvanceC(String, ErrorInfo);
	if(end != '\'')
	{
		RaiseError(StartErrorInfo, "Multi character literals are not supported");
	}
	return MakeToken(T_CHAR, StartErrorInfo, (string *)(u64)inside);
}

void SkipWhiteSpace(string *String, error_info *ErrorInfo)
{
	while(isspace(PeekC(String)))
	{
		AdvanceC(String, ErrorInfo);
		if(String->Size == 0)
		{
			return;
		}
	}

}

token GetNextToken(string *String, error_info *ErrorInfo)
{
	SkipWhiteSpace(String, ErrorInfo);
	if(String->Size == 0)
	{
		return MakeToken(T_EOF, *ErrorInfo, NULL);
	}
	char FirstChar = PeekC(String);

	if(FirstChar == 'c' && String->Size > 1)
	{
		if(String->Data[1] == '"')
		{
			AdvanceC(String, ErrorInfo);
			return TokinizeString(String, ErrorInfo, true);
		}
	}
	if(IsIDCharacter(FirstChar))
	{
		return TokinizeIdentifier(String, ErrorInfo);
	}
	if(isdigit(FirstChar) || (FirstChar == '-' && isdigit(PeekCAhead(String, 1))))
	{
		return TokinizeNumber(String, ErrorInfo);
	}
	if(FirstChar == '\'')
	{
		return TokinizeCharLiteral(String, ErrorInfo);
	}
	if(FirstChar == '"')
	{
		return TokinizeString(String, ErrorInfo, false);
	}
	if(FirstChar == '#')
	{
		return TokinizeCompilerDirective(String, ErrorInfo);
	}
	if(FirstChar == '/')
	{
		char SecondChar = PeekCAhead(String, 1);
		if(SecondChar == '/')
		{
			while(AdvanceC(String, ErrorInfo) != '\n');
			return GetNextToken(String, ErrorInfo);
		}
	}
	return TokinizeSpecialCharacter(String, ErrorInfo);
}

void AddKeyword(const char *TokenName, token_type Type)
{
	keyword Keyword;
	Keyword.ID   = MakeString(TokenName);
	Keyword.Type = Type;
	ArrPush(KeywordTable, Keyword);
}

void InitializeLexer()
{
	KeywordTable = ArrCreate(keyword);
	AddKeyword("break", T_BREAK);
	AddKeyword("if",  T_IF);
	AddKeyword("else",T_ELSE);
	AddKeyword("for", T_FOR);
	AddKeyword("fn",  T_FN);
	AddKeyword("as",  T_AS);
	AddKeyword("in",  T_IN);
	AddKeyword("@@",  T_AUTOCAST);
	AddKeyword("::",  T_CONST);
	AddKeyword(">=",  T_GEQ);
	AddKeyword("<=",  T_LEQ);
	AddKeyword("!=",  T_NEQ);
	AddKeyword("==",  T_EQEQ);
	AddKeyword("||",  T_LOR);
	AddKeyword("&&",  T_LAND);
	AddKeyword("->",  T_ARR);
	AddKeyword("++",  T_PPLUS);
	AddKeyword("--",  T_MMIN);
	AddKeyword("<<",  T_SLEFT);
	AddKeyword(">>",  T_SRIGHT);
	AddKeyword("+=",  T_PEQ);
	AddKeyword("-=",  T_MEQ);
	AddKeyword("*=",  T_TEQ);
	AddKeyword("/=",  T_DEQ);
	AddKeyword("%=",  T_MODEQ);
	AddKeyword("&=",  T_ANDEQ);
	AddKeyword("^=",  T_XOREQ);
	AddKeyword("|=",  T_OREQ);
	AddKeyword("<<=",  T_SLEQ);
	AddKeyword(">>=",  T_SREQ);
	AddKeyword("...",  T_VARARG);
	AddKeyword("#shadow", T_SHADOW);
	AddKeyword("#import", T_IMPORT);
	AddKeyword("#foreign",  T_FOREIGN);
	AddKeyword("#link", T_LINK);
	AddKeyword("#intr",  T_INTR);
	AddKeyword("#public",  T_PUBLIC);
	AddKeyword("#private", T_PRIVATE);
	AddKeyword("#if",      T_PWDIF);
	AddKeyword("#info",    T_INFO);
	AddKeyword("return", T_RETURN);
	AddKeyword("struct", T_STRUCT);
	AddKeyword("enum",   T_ENUM);
	AddKeyword("union",  T_UNION);
	AddKeyword("defer",  T_DEFER);
	AddKeyword("match",  T_MATCH);
	AddKeyword("size_of", T_SIZEOF);
	AddKeyword("type_of", T_TYPEOF);
}

