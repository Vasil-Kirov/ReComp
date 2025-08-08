#include "Lexer.h"
#include "Errors.h"
#include "Memory.h"
#include "VString.h"
#include "Module.h"
#include <ctype.h>

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
	return isdigit(C);
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
		RaiseError(true, *Error, "Unexpected end of file"); 
	}
	char Result = *String->Data;
	if(Result == '\n')
	{
		Error->Range.EndLine++;
		Error->Range.EndChar = 1;
	}
	else
	{
		Error->Range.EndChar++;
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

file *StringToTokens(string String, error_info ErrorInfo)
{
	file Result = {};
#if 0
	token ModuleName = GetNextToken(&String, &ErrorInfo);
	if(ModuleName.Type != T_ID)
	{
		RaiseError(true, ModuleName.ErrorInfo, "Expected module name at the start of file"); 
	}
	*OutModuleName = *ModuleName.ID;
#endif
	token *Tokens = ArrCreate(token);
	while(String.Size > 0)
	{
		token Token = GetNextToken(&String, &ErrorInfo);
		ArrPush(Tokens, Token);
	}

	Result.Tokens = Tokens;
	return DupeType(Result, file);
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
		RaiseError(true, *ErrorInfo, "Incorrect compiler directive \"%s\"", ID.Data);

	token Token = {};
	Token.Type = TokenType;
	Token.ErrorInfo = StartErrorInfo;
	return Token;
}

token TokinizeIdentifier(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;

	//error_info StartErrorInfo = *ErrorInfo;

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
	//error_info StartErrorInfo = *ErrorInfo;

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
				RaiseError(true, *ErrorInfo, "Binary number contains a character that's neither 0 nor 1: %c", c);
			}
			Builder += c;
			AdvanceC(String, ErrorInfo);
		}

		if(Builder.Size == 2)
		{
			RaiseError(true, *ErrorInfo, "Invalid binary number");
		}

		string Number = MakeString(Builder);
		return MakeToken(T_VAL, *ErrorInfo, MakeStringPointer(Number));
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
				RaiseError(true, *ErrorInfo, "Invalid character in hex number: %c", c);
			}
			Builder += c;
			AdvanceC(String, ErrorInfo);
		}

		if(Builder.Size == 2)
		{
			RaiseError(true, *ErrorInfo, "Invalid hex number");
		}

		string Number = MakeString(Builder);
		return MakeToken(T_VAL, *ErrorInfo, MakeStringPointer(Number));
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
		return MakeToken(T_VAL, *ErrorInfo, MakeStringPointer(Number));
	}
}

token TokinizeRawStringAndText(string *String, error_info *ErrorInfo)
{
	error_info Start = *ErrorInfo;

	const char *StringStart = String->Data;
	while(true)
	{
		SkipWhiteSpace(String, ErrorInfo);
		if(String->Data[0] == '`')
		{
			if(String->Size >= 3)
			{
				if(memcmp(String->Data, "```", 3) == 0)
				{
					AdvanceC(String, ErrorInfo);
					AdvanceC(String, ErrorInfo);
					AdvanceC(String, ErrorInfo);
					break;
				}
			}
		}

		AdvanceC(String, ErrorInfo);
	}
	const char *StringEnd = String->Data-3;
	string S = {};
	S.Data = StringStart;
	S.Size = StringEnd - StringStart;

	auto b = MakeBuilder();
	for(int i = 0; i < S.Size; ++i)
	{
		if(S.Data[i] != '\r') PushBuilder(&b, S.Data[i]);
	}

	S = MakeString(b);


	return MakeToken(T_STR, Start, MakeStringPointer(S));
}

token TokinizeSpecialCharacter(string *String, error_info *ErrorInfo)
{
	const char *Start = String->Data;
	//error_info StartErrorInfo = *ErrorInfo;

	char C = AdvanceC(String, ErrorInfo);
	if(String->Size == 0)
	{
		return MakeToken((token_type)C, *ErrorInfo, NULL);
	}
	if(String->Size > 2)
	{
		string PotentialKeywordString = MakeString(Start, 3);
		token_type PotentialKeyword = GetKeyword(PotentialKeywordString);
		if(PotentialKeyword != T_ID)
		{
			AdvanceC(String, ErrorInfo);
			AdvanceC(String, ErrorInfo);
			if(PotentialKeyword == T_RAWSTRING)
			{
				return TokinizeRawStringAndText(String, ErrorInfo);
			}
			return MakeToken(PotentialKeyword, *ErrorInfo, NULL);
		}
	}
	if(String->Size > 1)
	{
		string PotentialKeywordString = MakeStringSlice(Start, 2);
		token_type PotentialKeyword = GetKeyword(PotentialKeywordString);
		if(PotentialKeyword != T_ID)
		{
			AdvanceC(String, ErrorInfo);
			return MakeToken(PotentialKeyword, *ErrorInfo, NULL);
		}
	}

	for(int i = 0; i < String->Size && !isspace(String->Data[i]) &&
			IsIDCharacter(String->Data[i]); ++i)
	{
		string PotentialKeywordString = MakeStringSlice(Start, i+2);
		token_type PotentialKeyword = GetKeyword(PotentialKeywordString);
		if(PotentialKeyword != T_ID)
		{
			for(int j = i; j >= 0; j--)
			{
				AdvanceC(String, ErrorInfo);
			}
			if(PotentialKeyword == T_RAWSTRING)
			{
				return TokinizeRawStringAndText(String, ErrorInfo);
			}
			return MakeToken(PotentialKeyword, *ErrorInfo, NULL);
		}
	}
	return MakeToken((token_type)C, *ErrorInfo, NULL);

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
		case '0':
		return '\0';
		case 'r':
		return '\r';
		case 'v':
		return '\v';
		case 'f':
		return '\f';
	}
	return ToEscape;
}

token TokinizeString(string *String, error_info *ErrorInfo, b32 CString)
{
	//error_info StartErrorInfo = *ErrorInfo;
	AdvanceC(String, ErrorInfo);

	string_builder Builder = MakeBuilder();
	while(PeekC(String) != '"')
	{
		char Next = AdvanceC(String, ErrorInfo);
		if(Next == '\\')
		{
			char ToEscape = AdvanceC(String, ErrorInfo);
			char Escaped = GetEscapedChar(ToEscape);
			PushBuilder(&Builder, Escaped);
		}
		else
		{
			PushBuilder(&Builder, Next);
		}
	}
	AdvanceC(String, ErrorInfo);
	string Tokinized = STR_LIT("");
	if(Builder.Size > 0)
		Tokinized = MakeString(Builder);

	if(CString)
		return MakeToken(T_CSTR, *ErrorInfo, MakeStringPointer(Tokinized));
	else
		return MakeToken(T_STR, *ErrorInfo,  MakeStringPointer(Tokinized));
}

u32 ExtractCodepoint(const char *P, uint Size)
{
	u32 res = 0;
	switch(Size)
	{
		case 1:
		{
			res = *P;
			if(res == '\\')
				res = GetEscapedChar(*(P+1));
		}
		break;
		case 2:
		{
			u8 First = *P & 0b0001'1111;
			u8 Second = *(P+1) & 0b0011'1111;
			res = (u32)First << 6 | (u32)Second;
		} break;
		case 3:
		{
			u8 First = *P & 0b0000'1111;
			u8 Second = *(P+1) & 0b0011'1111;
			u8 Third = *(P+2) & 0b0011'1111;
			res = (u32)First << 12 | (u32)Second << 6 | (u32)Third;
		} break;
		case 4:
		{
			u8 First = *P & 0b0000'0111;
			u8 Second = *(P+1) & 0b0011'1111;
			u8 Third = *(P+2) & 0b0011'1111;
			u8 Forth = *(P+3) & 0b0011'1111;
			res = (u32)First << 18 | (u32)Second << 12 | (u32)Third << 6 | (u32)Forth;
		} break;
	}
	return res;
}

token TokinizeCharLiteral(string *String, error_info *ErrorInfo)
{
	//error_info StartErrorInfo = *ErrorInfo;
	char c = AdvanceC(String, ErrorInfo);
	Assert(c == '\'');
	u64 result = 0;
	uint i = 0;
	const char *Start = String->Data;
	while(String->Data[0] != '\'')
	{
		if (i >= 4)
		{
			RaiseError(true, *ErrorInfo, "Char literal is too large. It can be a maximum of 4 bytes");
		}
		char c = AdvanceC(String, ErrorInfo);
		if(c == '\\')
		{
			char escaped = AdvanceC(String, ErrorInfo);
			c = GetEscapedChar(escaped);
		}

		i++;
	}
	if (i > 4)
	{
		RaiseError(true, *ErrorInfo, "Char literal is too large. It can be a maximum of 4 bytes");
	}

	result = ExtractCodepoint(Start, i);

	char end = AdvanceC(String, ErrorInfo);
	Assert(end == '\'')
	return MakeToken(T_CHAR, *ErrorInfo, (string *)(u64)result);
}

token EatCommentAndNext(string *String, error_info *ErrorInfo)
{
	if(PeekCAhead(String, 1) == '/')
	{
		while(AdvanceC(String, ErrorInfo) != '\n');
	}
	else
	{
		Assert(PeekCAhead(String, 1) == '*');
		AdvanceC(String, ErrorInfo);
		AdvanceC(String, ErrorInfo);

		int Depth = 1;
		while(Depth > 0)
		{
			char c = AdvanceC(String, ErrorInfo);
			if(c == '/')
			{
				c = PeekC(String);
				if(c == '*')
				{
					AdvanceC(String, ErrorInfo);
					Depth++;
				}
			}
			else if(c == '*')
			{
				c = PeekC(String);
				if(c == '/')
				{
					AdvanceC(String, ErrorInfo);
					Depth--;
				}
			}
		}

	}
	return GetNextToken(String, ErrorInfo);
}

token GetNextToken(string *String, error_info *ErrorInfo)
{
	SkipWhiteSpace(String, ErrorInfo);
	if(String->Size == 0)
	{
		return MakeToken(T_EOF, *ErrorInfo, NULL);
	}
	ErrorInfo->Range.StartChar = ErrorInfo->Range.EndChar;
	ErrorInfo->Range.StartLine = ErrorInfo->Range.EndLine;
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
	if(isdigit(FirstChar) || (FirstChar == '.' && isdigit(PeekCAhead(String, 1))) )
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
		if(SecondChar == '/' || SecondChar == '*')
			return EatCommentAndNext(String, ErrorInfo);
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
	AddKeyword("cast", T_NEWCAST);
	AddKeyword("bit_cast", T_BITCAST);
	AddKeyword("continue", T_CONTINUE);
	AddKeyword("break", T_BREAK);
	AddKeyword("if",  T_IF);
	AddKeyword("else",T_ELSE);
	AddKeyword("then", T_THEN);
	AddKeyword("for", T_FOR);
	AddKeyword("fn",  T_FN);
	AddKeyword("as",  T_AS);
	AddKeyword("in",  T_IN);
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
	AddKeyword("```",  T_RAWSTRING);
	AddKeyword("...",  T_VARARG);
	AddKeyword("#shadow", T_SHADOW);
	AddKeyword("#import", T_IMPORT);
	AddKeyword("#foreign",  T_FOREIGN);
	AddKeyword("#link", T_LINK);
	AddKeyword("#intrinsic",  T_INTR);
	AddKeyword("#public",  T_PUBLIC);
	AddKeyword("#private", T_PRIVATE);
	AddKeyword("#embed_bin", T_EMBED_BIN);
	AddKeyword("#embed_str", T_EMBED_STR);
	AddKeyword("#inline",  T_INLINE);
	AddKeyword("#if",      T_PWDIF);
	AddKeyword("#elif",    T_PWDELIF);
	AddKeyword("#else",    T_PWDELSE);
	AddKeyword("type_info", T_INFO);
	AddKeyword("#assert",  T_ASSERT);
	AddKeyword("#load_dl",  T_LOAD_DL);
	AddKeyword("#load_system_dl",  T_LOAD_SYSTEM_DL);
	AddKeyword("#file_location",  T_FILE_LOCATION);
	AddKeyword("#run",   T_RUN);
	AddKeyword("return", T_RETURN);
	AddKeyword("struct", T_STRUCT);
	AddKeyword("enum",   T_ENUM);
	AddKeyword("union",  T_UNION);
	AddKeyword("defer",  T_DEFER);
	AddKeyword("match",  T_MATCH);
	AddKeyword("size_of", T_SIZEOF);
	AddKeyword("type_of", T_TYPEOF);
	AddKeyword("void", T_VOID);
	AddKeyword("@profile", T_PROFILE);
	AddKeyword("using",  T_USING);
	AddKeyword("yield",  T_YIELD);
	AddKeyword("module",  T_MODULE);
}

