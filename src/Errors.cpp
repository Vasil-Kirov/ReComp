#include "Errors.h"
#include "Basic.h"
#include "Log.h"

string GetErrorSegment(error_info ErrorInfo)
{

	const u8 *scanner = (const u8 *)ErrorInfo.Data->Data;
	i32 row = 1;
	while(true)
	{
		if(*scanner == 0)
			return MakeString(" ");

		scanner++;
		if(*scanner == '\n')
		{
			row++;
		}
		if(row == ErrorInfo.Line - 1)
			break;
	}
	scanner++;
	const u8 *start = scanner;
	i32 counter = 0;
	i32 last_line_columns = 0;

	string_builder Builder = MakeBuilder();

	char *white_space = ArrCreate(char);
	char tab = '\t';
	char space = ' ';
	while(true)
	{
		PushBuilder(&Builder, *scanner);
		if (*scanner == 0)
		{
			scanner--;
			if(counter != 0)
				break;
			return MakeString(" ");
		}
		
		if(counter == 1) {
			if(*scanner == '\t')
				ArrPush(white_space, tab);
			else
				ArrPush(white_space, space);
		}

		if(counter == 2) {
			last_line_columns++;
		}
		if(*scanner == '\n')
		{
			if(counter == 0)
			{
				int i = Builder.Size;
				char space = ' ';
				const u8 *to_put = scanner + 1;
				if(*to_put == '\t')
				{
					for(int i = 0; i < 4; ++i)
						PushBuilder(&Builder, space);
				}
				else  PushBuilder(&Builder, space);
				Builder.Data[i] = '>';
			}
			counter++;
		}

		if(counter == 3)
			break;
		scanner++;
	}
	const u8 *end = scanner;
	size_t len = end - start + 1;

	PushBuilder(&Builder, '\n');
	
	i32 tick_counter = 0;
	i32 white_space_it = 0;
	while(tick_counter != ErrorInfo.Character + 1)
	{
		if(white_space_it == ArrLen(white_space))
			white_space_it = 0;
		PushBuilder(&Builder, white_space[white_space_it++]);
		tick_counter++;
	}
	Builder.Data[Builder.Size - 2] = '^';
	Builder.Data[Builder.Size - 1] = '^';
	Builder.Data[Builder.Size - 0] = '^';

	return MakeString(Builder);
}
#if 0

void
raise_interpret_error(const char *error_msg, struct _Token_Iden token)
{
	if(token.file)
	{
		u8 *error_location = get_error_segment(token);
		LG_ERROR("%s (%d, %d):\n\tInterpretting error: %s.\n\n%s",
				token.file, token.line, token.column, error_msg, error_location);
	}
}

void raise_semantic_error(File_Contents *f, const char *error_msg, struct _Token_Iden token)
{
	u8 *error_location = get_error_segment(token);
	LG_FATAL("%s (%d, %d):\n\tSemantic error: %s.\n\n%s",
			 token.file, token.line, token.column, error_msg, error_location);
}

void raise_token_syntax_error(File_Contents *f, const char *error_msg, char *file, u64 line,
							  u64 column)
{
	Token_Iden new_token = {};
	new_token.file = file;
	new_token.column = column;
	new_token.line = line;
	new_token.f_start = f->file_data;
	u8 *error_location = get_error_segment(new_token);
	LG_FATAL("%s (%d, %d):\n\tAn error occured while tokenizing: %s.\n\n%s", file, line, column, error_msg, error_location);
}
#endif

void
RaiseError(error_info ErrorInfo, const char *_ErrorMessage, ...)
{
	string ErrorMessage = MakeString(_ErrorMessage);
	char FinalFormat[4096] = {0};

	va_list Args;
	va_start(Args, _ErrorMessage);
	
	vsnprintf_s(FinalFormat, ErrorMessage.Size, ErrorMessage.Data, Args);
	
	va_end(Args);
	
	string ErrorSegment = GetErrorSegment(ErrorInfo);
	LFATAL("%s (%d, %d):\n%s\n\n%s",
			ErrorInfo.FileName, ErrorInfo.Line, ErrorInfo.Character, ErrorMessage.Data, ErrorSegment.Data);
}

