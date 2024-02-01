#include "Lexer.h"

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
	String->Data++;
	String->Size--;
	Error->Character++;
	return Result;
}

char PeekC(string *String)
{
	return *String->Data;
}

void NewLine(error_info *ErrorInfo)
{
	ErrorInfo->Line++;
	ErrorInfo->Character = 0;
}

token *StringToTokens(string String, error_info ErrorInfo)
{
	token *Result = ArrCreate(token);

	while(String.Size > 0)
	{
		token Token = GetNextToken(&String, &ErrorInfo);
		ArrPush(Result, Token);
	}

	return Result;
}

token TokinizeIdentifier(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;

	while(IsIDCharacter(PeekC(String)) || isdigit(PeekC(String)))
		AdvanceC(String, ErrorInfo);

	const char *End = String->Data;
	string ID = MakeString(Start, End - Start);
	token_type TokenType = GetKeyword(ID);

	token Token = {};
	Token.Type = TokenType;
	Token.ErrorInfo = *ErrorInfo;
	if(TokenType == T_ID)
		Token.ID = MakeStringPointer(ID);
	return Token;
}

token TokinizeNumber(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;

	b32 FoundDot = false;
	while(IsNumCharacter(PeekC(String), &FoundDot))
		AdvanceC(String, ErrorInfo);

	const char *End  = String->Data;

	string Number = MakeString(Start, End - Start);
	return MakeToken(T_NUM, *ErrorInfo, MakeStringPointer(Number));
}

token TokinizeSpecialCharacter(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;
	char C = AdvanceC(String, ErrorInfo);
	if(String->Size == 0)
	{
		return MakeToken((token_type)C, *ErrorInfo, NULL);
	}
	string PotentialKeywordString = MakeString(Start, 2);
	token_type PotentialKeyword = GetKeyword(PotentialKeywordString);
	if(PotentialKeyword == T_ID)
	{
		return MakeToken((token_type)C, *ErrorInfo, NULL);
	}
	AdvanceC(String, ErrorInfo);

	return MakeToken(PotentialKeyword, *ErrorInfo, NULL);
}

token TokinizeString(string *String, error_info *ErrorInfo)
{
	AdvanceC(String, ErrorInfo);
	const char *Start = String->Data;
	while(PeekC(String) != '"')
	{
		char Next = AdvanceC(String, ErrorInfo);
		if(Next == '\\')
		{
			// @TODO: Escaped characters, for now we just skip it
			AdvanceC(String, ErrorInfo);
		}
	}
	const char *End = String->Data;
	AdvanceC(String, ErrorInfo);
	string Tokinized = MakeString(Start, End - Start);
	return MakeToken(T_STR, *ErrorInfo, MakeStringPointer(Tokinized));
}

token GetNextToken(string *String, error_info *ErrorInfo)
{
	while(isspace(PeekC(String)))
	{
		if(PeekC(String) == '\n')
			NewLine(ErrorInfo);
		AdvanceC(String, ErrorInfo);
		if(String->Size == 0)
		{
			return MakeToken(T_EOF, *ErrorInfo, NULL);
		}
	}

	char FirstChar = PeekC(String);

	if(IsIDCharacter(FirstChar))
	{
		return TokinizeIdentifier(String, ErrorInfo);
	}
	if(isdigit(FirstChar) || FirstChar == '-')
	{
		return TokinizeNumber(String, ErrorInfo);
	}
	if(FirstChar == '"')
	{
		return TokinizeString(String, ErrorInfo);
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
	AddKeyword("if",  T_IF);
	AddKeyword("for", T_FOR);
	AddKeyword(">=",  T_GEQ);
	AddKeyword("<=",  T_LEQ);
	AddKeyword("!=",  T_NEQ);
	AddKeyword("==",  T_EQEQ);
	AddKeyword("->",  T_ARR);
	AddKeyword("++",  T_PPLUS);
	AddKeyword("--",  T_MMIN);
}

