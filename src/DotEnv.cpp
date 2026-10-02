#include "Lexer.h"
#include "Module.h"
#include "Platform.h"


void ReadEnvironment()
{
	string EnvText = ReadEntireFile(STR_LIT("./.env"));
	if (EnvText.Data == nullptr)
		return;

	error_info ErrorInfo = {};
	ErrorInfo.Data = DupeType(EnvText, string);
	ErrorInfo.FileName = ".env";
	ErrorInfo.Range.StartLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndLine = 1;
	ErrorInfo.Range.EndChar = 1;
	file *f = StringToTokens(EnvText, ErrorInfo);
	if (!f->Tokens)
		return;

	token *At = f->Tokens;
	while(At->Type != T_EOF)
	{
		if (At->Type != T_ID)
		{
			LogCompilerError("Invalid token in .env file, expected a enviornment variable name, got %s", GetTokenName(At->Type));
			return;
		}
		string Name = *At->ID;
		At++;

		if (At->Type != T_EQ)
		{
			LogCompilerError("Invalid token in .env file, expected `=`, got %s", GetTokenName(At->Type));
			if ((At+1)->Type == T_EQ) At += 2;
		}
		else
		{
			At++;
		}
		switch (At->Type)
		{
			case T_ID:
			case T_VAL:
			case T_STR:
			{
				PlatformSetEnv(Name.Data, At->ID->Data);
			} break;
			default:
			{
				LogCompilerError("Invalid token in .env file, expected a value, got %s", GetTokenName(At->Type));
				return;
			} break;
		}
		At++;
	}

}

