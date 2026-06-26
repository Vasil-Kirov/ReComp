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
print_stack -- prints the function stack

)raw";

#if _WIN32
#include <iostream>
#include <string>

value Load(interpreter *VM, value *Value, u32 TypeIdx, b32 *NoResult, u32 ResultReg);

value FindVariableRegister(interpreter *VM, string Name, slice<basic_block> Blocks, bool *Failed)
{
	*Failed = false;
	if(Blocks.Count == 0)
	{
		*Failed = true;
		return {};
	}
	u32 ClosestLine = -1;
	u32 Result = -1;
	for(auto& b : Blocks)
	{
		for(auto i : b.Code)
		{
			if(i.Op == OP_DEBUGINFO)
			{
				ir_debug_info *info = (ir_debug_info *)i.Ptr;
				if(info->type == IR_DBG_VAR && info->var.Name == Name)
				{
					if(VM->ErrorInfo.IsEmpty()) return *VM->Registers.GetValue(info->var.Register);
					auto end = VM->ErrorInfo.Peek()->Range.EndLine;
					if(end < info->var.LineNo)
						continue;

					if(end - info->var.LineNo < ClosestLine)
					{
						ClosestLine = info->var.LineNo;
						Result = info->var.Register;
					}
				}
			}
		}
	}
	if(Result == -1)
	{
		*Failed = true;
		return {};
	}
	value *v = VM->Registers.GetValue(Result);
	const type *T = GetType(v->Type);
	if(T->Kind == TypeKind_Pointer && T->Pointer.Pointed != INVALID_TYPE && v->ptr != nullptr && IsLoadableType(T))
	{
		b32 _;
		value V = Load(VM, v, T->Pointer.Pointed, &_, -1);
		return V;
	}
	return *v;
}

void PrintCodeAround(interpreter *VM)
{
	if(VM->ErrorInfo.IsEmpty() || VM->ErrorInfo.Peek() == NULL)
		return;

	auto b = MakeBuilder();
	auto err_i = VM->ErrorInfo.Peek();
	int start = err_i->Range.StartLine-5;
	if(start < 1) start = 1;
	int atline = 1;
	int at = 0;
	while(atline < start)
	{
		b += err_i->Data->Data[at];
		if(err_i->Data->Data[at] == '\n')
		{
			atline++;
			if(atline == err_i->Range.EndLine)
				b += ">>>";
		}
		if(atline < start)
			at++;
	}
	int end = err_i->Range.EndLine+5;
	while(atline < end && at < err_i->Data->Size)
	{
		b += err_i->Data->Data[at];
		if(err_i->Data->Data[at] == '\n')
		{
			atline++;
			if(atline == err_i->Range.EndLine)
				b += ">>>";
		}
		if(atline < end)
			at++;
	}

	scratch_arena sarena{};
	auto s = MakeString(b, sarena.Allocate(b.Data.Count));
	printf("%.*s\n", (int)s.Size, s.Data);
}

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

void PrintRegisterValue(value *V)
{
	const type *T = GetType(V->Type);
	auto b = MakeBuilder();
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
					b.printf("count: %zu, data: %.*s", Size, (int)Size, Data);
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
			b.printf("count: %zu, data: %p", Size, Data);
		} break;
		default:
		{
			b.printf("<not printable>");
		} break;
	}
	b.printf(" |%s|", GetTypeName(T));
	b += "\n";
	string ToPrint = MakeString(b);
	printf("%.*s", (int)ToPrint.Size, ToPrint.Data);
}

DebugAction DebugPrompt(interpreter *VM, instruction I, b32 ShowLine, slice<basic_block> Blocks)
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
		PrintCodeAround(VM);
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
			if(ToPrint.Data[0] == '#')
			{
				u32 Register = -1;
				if(ToPrint.Size <= 1)
				{
					puts("Expected number after #");
				}
				else
				{
					Register = atoll(&ToPrint.Data[1]);
					if(Register < 0 || Register > VM->Registers.LastRegister)
					{
						printf("Register number %u is out of range, 0 < x < %d\n", Register, VM->Registers.LastRegister+1);
						Register = -1;
					}
				}
				if(Register != -1)
					PrintRegisterValue(VM->Registers.GetValue(Register));
			}
			else
			{
				bool Failed = false;
				value V = FindVariableRegister(VM, ToPrint, Blocks, &Failed);
				if(Failed)
				{
					printf("Couldn't find variable %.*s\n", (int)ToPrint.Size, ToPrint.Data);
				}
				else
				{
					PrintRegisterValue(&V);
				}
			}
		}
	}
	else if(Action == "ps" || Action == "print_stack")
	{
		r = DebugAction_prompt_again;
		for(int i = 0; i < VM->FunctionStack.Data.Count; ++i)
		{
			string FnName = *VM->FunctionStack.PeekNth(i)->Name;
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

