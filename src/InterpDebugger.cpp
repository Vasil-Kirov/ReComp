#include "InterpDebugger.h"
#include "Errors.h"
#include "IR.h"
#include "Interpreter.h"
#include "Log.h"
#include "Platform.h"
#include "Type.h"
#include "VString.h"
#include <cstdlib>
#include <cstring>

const char *Help = R"raw(
continue -- continue until next signal or breakpoint
print -- print a register's value
step -- step a single ir instruction
next -- step a single statement
list -- show the current location
list_instructions -- show the current location in IR
quit -- quit the program
clear -- clears the screen

)raw";

#if _WIN32
#include <iostream>
#include <string>

int getline(char **OutLine, size_t *OutSize, FILE *s)
{
	Assert(s == stdin);
	std::string out;
    std::getline(std::cin, out);

	char *Data = (char *)malloc(out.size()+2);
	if(Data == NULL)
		return -1;

	Data[out.size()]   = '\n';
	Data[out.size()+1] = '\0';

	memcpy(Data, out.c_str(), out.size());

	*OutLine = Data;
	*OutSize = out.size()+1;

	return 0;
}
#endif

void PrintRegisterValue(interpreter *VM, long long Register)
{
	value *V = VM->Registers.GetValue(Register);
	const type *T = GetType(V->Type);
	auto b = MakeBuilder();
	b.printf("%%%d = %s ", Register, GetTypeName(T));
	switch(T->Kind)
	{
		case TypeKind_Basic:
		{
			switch(T->Basic.Kind)
			{
				case Basic_bool:
				case Basic_u8:
				{
					b.printf("%u", V->u8);
				} break;
				case Basic_u16:
				{
					b.printf("%u", V->u16);
				} break;
				case Basic_u32:
				{
					b.printf("%u", V->u32);
				} break;
				case Basic_uint:
				case Basic_u64:
				{
					b.printf("%llu", V->u64);
				} break;
				case Basic_i8:
				{
					b.printf("%d", V->i8);
				} break;
				case Basic_i16:
				{
					b.printf("%d", V->i16);
				} break;
				case Basic_i32:
				{
					b.printf("%d", V->i32);
				} break;
				case Basic_type:
				case Basic_int:
				case Basic_i64:
				{
					b.printf("%lld", V->i64);
				} break;
				case Basic_f32:
				{
					b.printf("%f", V->f32);
				} break;
				case Basic_f64:
				{
					b.printf("%f", V->f64);
				} break;
				case Basic_string:
				{
					size_t Size = *(size_t *)V->ptr;
					u8 *Data = *((u8 **)V->ptr + 1);
					b.printf("count: %d, data: %.*s", Size, Size, Data);
				} break;
				default:
				{
					b.printf("<not printable>");
				} break;
			}
		} break;
		case TypeKind_Struct:
		case TypeKind_Pointer:
		{
			b.printf("%p", V->ptr);
		} break;
		case TypeKind_Slice:
		{
			size_t Size = *(size_t *)V->ptr;
			u8 *Data = *((u8 **)V->ptr + 1);
			b.printf("count: %d, data: %p", Size, Data);
		} break;
		default:
		{
			b.printf("<not printable>");
		} break;
	}
	b += "\n";
	string ToPrint = MakeString(b);
	printf("%.*s", (int)ToPrint.Size, ToPrint.Data);
}

DebugAction DebugPrompt(interpreter *VM, instruction I, b32 ShowLine)
{
	auto b = MakeBuilder();
	if(ShowLine)
	{
		DissasembleInstruction(&b, I);
		b += '\n';
	}
	b += "> ";
	string S = MakeString(b);
	printf("%.*s", (int)S.Size, S.Data);

	char *line = NULL;
	size_t size;
	if(getline(&line, &size, stdin) == -1) {
		// idk why it would happen
		exit(1);
	}

	if(size == 0 || size == 1)
	{
		free(line);
		return DebugAction_prompt_again;
	}

	const char *Whitespace = " \n\t";
	char *word = strtok(line, Whitespace);
	if(word == NULL)
	{
		free(line);
		return DebugAction_prompt_again;
	}

	DebugAction r = DebugAction_prompt_again;

	string Action = { .Data = word, .Size = strlen(word) };
	if (Action == "h" || Action == "help")
	{
		printf("%s", Help);
		r = DebugAction_prompt_again;
	}
	else if(Action == "c" || Action == "cont" || Action == "continue")
	{
		r = DebugAction_continue;
	}
	else if(Action == "s" || Action == "step")
	{
		r = DebugAction_step_instruction;
	}
	else if(Action == "n" || Action == "next")
	{
		r = DebugAction_next_stmt;
	}
	else if(Action == "q" || Action == "quit")
	{
		r = DebugAction_quit;
	}
	else if(Action == "li" || Action == "list_instructions")
	{
		r = DebugAction_list_instructions;
	}
	else if(Action == "cls" || Action == "clear")
	{
		r = DebugAction_prompt_again;
#if _WIN32
		system("cls");
#else
		system("clear");
#endif
	}
	else if(Action == "l" || Action == "list")
	{
		if(!VM->ErrorInfo.IsEmpty())
		{
			auto err_i = VM->ErrorInfo.Peek();
			string One;
			string Two;
			string Three;
			GetErrorSegments(*err_i, &One, &Two, &Three);

			PlatformOutputString(One, LOG_CLEAN);
			PlatformOutputString(Two, LOG_ERROR);
			PlatformOutputString(Three, LOG_CLEAN);
		}
		r = DebugAction_prompt_again;
	}
	else if(Action == "p" || Action == "print")
	{
		r = DebugAction_prompt_again;
		word = strtok(NULL, Whitespace);
		if(word == NULL || strlen(word) == 0)
		{
			puts("Expected argument to print");
		}
		else
		{
			string ToPrint = { .Data = word, .Size = strlen(word) };
			if(ToPrint.Data[0] == 'r')
			{
				if(ToPrint.Size <= 1)
				{
					puts("Expected number after r");
				}
				else
				{
					auto Number = atoll(&ToPrint.Data[1]);
					if(Number < 0 || Number > VM->Registers.LastRegister)
					{
						printf("Register number %lld is out of range, 0 < x < %d\n", Number, VM->Registers.LastRegister+1);
					}
					else
					{
						PrintRegisterValue(VM, Number);
					}
				}
			}
			else
			{
				puts("print argument doesn't start with r, currently non register arguments are not supported");
			}
		}
	}
	else if(Action == "ps" || Action == "print_stack")
	{
		r = DebugAction_prompt_again;
		for(int i = 0; i < VM->FunctionStack.Data.Count; ++i)
		{
			string FnName = VM->FunctionStack.PeekNth(i);
			printf("%.*s\n", (int)FnName.Size, FnName.Data);
		}
	}
	else
	{
		printf("%s", Help);
	}

	free(line);
	return r;
}

