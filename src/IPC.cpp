#include "IPC.h"
#include "Module.h"
#include "Platform.h"
#include "Semantics.h"
#include "vbytestream.h"
#include "DumpInfo.h"
#include "Parser.h"

u64 IPCWritePipe;
u64 IPCReadPipe;
slice<module*> IPCProgramModules = {};
uint IPCAtStage = 0;

void IPCSetModules(slice<module*> Modules)
{
	IPCProgramModules = Modules;
}

void IPCSendMessage(const void *Msg, u32 Size)
{
	PlatformWritePipe(IPCWritePipe, &Size, sizeof(u32));
	PlatformWritePipe(IPCWritePipe, Msg, Size);
}

void *IPCRecvMessage(u32 *OutSize)
{
	u32 Size = 0;
	PlatformReadPipe(IPCReadPipe, &Size, sizeof(u32));
	void *Data = VAlloc(Size);
	PlatformReadPipe(IPCReadPipe, Data, Size);
	if(OutSize)
		*OutSize = Size;
	return Data;
}

void IPCExecCMD(ipc_cmd Cmd, slice<u8> Data)
{
	IPCSendMessage(&Cmd, 1);
	IPCSendMessage(Data.Data, Data.Count);
}

ipc_packet IPCGetCMD()
{
	u32 Size = 0;
	u8 *Cmd = (u8 *)IPCRecvMessage(&Size);
	if(Size != 1)
	{
		VFree(Cmd);
		return {};
	}
	u8 *Data = (u8 *)IPCRecvMessage(&Size);
	slice<u8> s = {.Data = Data, .Count = Size};

	ipc_cmd ECmd = (ipc_cmd)*Cmd;
	VFree(Cmd);
	return ipc_packet{ECmd, s};
}

void IPCFreePacket(ipc_packet Packet)
{
	VFree(Packet.Data.Data);
}

string IPCReadSubstituteFile(string OriginalFile)
{
	IPCSendMessage((void *)OriginalFile.Data, (u32)OriginalFile.Size);
	u32 Size = 0;
	void *Data = IPCRecvMessage(&Size);
	return string {(const char *)Data, Size};
}

string ReadRCPString(binary_reader *r)
{
	u32 Size = ReadUInt32(r);
	return ReadString(r, Size);
}

struct type_find_result
{
	string Name;
	u32 T;
	const error_info *Location;
};

struct find_result
{
	u32 Line;
	u32 Char;
	slice<scope_dump> Scopes;
	file *File;
	int FoundType;
	union {
		symbol *Symbol;
		module *Module;
		type_find_result TypeLocation;
	};
};

enum class completion_item_kind : u32 {
	Text = 1,
	Method = 2,
	Function = 3,
	Constructor = 4,
	Field = 5,
	Variable = 6,
	Class = 7,
	Interface = 8,
	Module = 9,
	Property = 10,
	Unit = 11,
	Value = 12,
	Enum = 13,
	Keyword = 14,
	Snippet = 15,
	Color = 16,
	File = 17,
	Reference = 18,
	Folder = 19,
	EnumMember = 20,
	Constant = 21,
	Struct = 22,
	Event = 23,
	Operator = 24,
	TypeParameter = 25,
};

struct completion_result
{
	completion_item_kind Kind;
	string Label;
	string LabelDetail;
	string InsertText;
	int InsertTextFormat;
};

find_result IPCFindSymbol(file *File, u32 Line, u32 Char, scratch_arena *Arena);

dynamic<completion_result> DoCompletion(file *File, string Line, int LineNo, int Char, scratch_arena *Arena);

void IPCSendFail()
{
	binary_blob Blob = StartOutput();
	DumpU32(&Blob, 0);
	IPCSendMessage(Blob.Buf.Data, Blob.Buf.Count);
	Blob.Buf.Free();
}

file *IPCFindFile(string FilePath)
{
	ForN(IPCProgramModules, Mod)
	{
		For((*Mod)->Files)
		{
			if((*it)->Name == FilePath)
			{
				return *it;
			}
		}
	}
	return nullptr;
}

 __attribute__((noreturn))
void IPCListenAndServe()
{
	u8 c = (u8)IPCAtStage;
	IPCSendMessage(&c, 1);
	if((IPCAtStage & (1 << 4)) == 0)
	{
		binary_blob Blob = StartOutput();
		DumpU32(&Blob, ErrorsToDump.Count);
		for(const error_dump& e : ErrorsToDump)
		{
			DumpError(&Blob, e);
		}
		exit(0);
	}
	DontExit = true;
	scratch_arena Arena = {};
	bool Running = true;
	while(Running)
	{
		ipc_packet Packet = IPCGetCMD();
		switch(Packet.CMD)
		{
			case ipc_cmd::NOP:
			{
			} break;
			case ipc_cmd::FIND_SYMBOL:
			{
				binary_reader r = MakeReader(Packet.Data.Data, Packet.Data.Count);
				string FilePath = ReadRCPString(&r);
				u32 Line = ReadUInt32(&r);
				u32 Char = ReadUInt32(&r);
				file *File = IPCFindFile(FilePath);
				if(!File)
				{
					IPCSendFail();
					break;
				}
				find_result Result = IPCFindSymbol(File, Line, Char, &Arena);
				switch(Result.FoundType)
				{
					case 0:
					{
						binary_blob Blob = StartOutput();
						DumpU32(&Blob, 1);
						DumpString(&Blob, *Result.Symbol->Name);
						DumpU32(&Blob, Result.Symbol->Type);
						DumpLocationErrI(&Blob, Result.Symbol->Node->ErrorInfo);
						IPCSendMessage(Blob.Buf.Data, Blob.Buf.Count);
						Blob.Buf.Free();
					} break;
					case 2:
					{
						binary_blob Blob = StartOutput();
						DumpU32(&Blob, 1);
						DumpString(&Blob, Result.TypeLocation.Name);
						DumpU32(&Blob, Result.TypeLocation.T);
						DumpLocationErrI(&Blob, Result.TypeLocation.Location);
						IPCSendMessage(Blob.Buf.Data, Blob.Buf.Count);
						Blob.Buf.Free();
					} break;
					case 1:
					[[fallthrough]];
					default:
					{
						IPCSendFail();
					} break;
				}
			} break;
			case ipc_cmd::DO_COMPLETION:
			{
				binary_reader r = MakeReader(Packet.Data.Data, Packet.Data.Count);
				string FilePath = ReadRCPString(&r);
				u32 LineNo = ReadUInt32(&r);
				u32 Char = ReadUInt32(&r);
				string Line = ReadRCPString(&r);
				file *File = IPCFindFile(FilePath);
				if(!File)
				{
					IPCSendFail();
					break;
				}
				dynamic<completion_result> Results = DoCompletion(File, Line, LineNo, Char, &Arena);
				binary_blob Blob = StartOutput();
				DumpU32(&Blob, 1);
				DumpU32(&Blob, Results.Count);
				for(completion_result& r : Results)
				{
					DumpString(&Blob, r.Label);
					DumpString(&Blob, r.LabelDetail);
					DumpString(&Blob, r.InsertText);
					DumpU32(&Blob, r.InsertTextFormat);
					DumpU32(&Blob, (u32)r.Kind);
				}
				IPCSendMessage(Blob.Buf.Data, Blob.Buf.Count);
				Blob.Buf.Free();
				Results.Free();
			} break;
			case ipc_cmd::QUIT:
			{
				Running = false;
				u8 b = 1;
				IPCSendMessage(&b, 1);
			} break;
		}

		IPCFreePacket(Packet);
		ResetArena(&Arena.Arena);
	}
	exit(0);
}

bool PositionInRange(range Range, u32 Line, u32 Char)
{
    if(Line < Range.StartLine || Line > Range.EndLine)
        return false;
    if(Line == Range.StartLine && Char < Range.StartChar)
        return false;
    if(Line == Range.EndLine && Char >= Range.EndChar)
        return false;
    return true;
}

bool IPCSearchNode(node *N, void *Arg)
{
	if(!N) return false;

	find_result *Find = (find_result *)Arg;
	if(N->Type == AST_ID)
	{
		if(!PositionInRange(N->ErrorInfo->Range, Find->Line, Find->Char))
			return true;
		for(scope_dump &Scope : Find->Scopes)
		{
			for(symbol &Symbol : Scope.Symbols)
			{
				if(Symbol.Name && *Symbol.Name == *N->ID.Name)
				{
					Find->FoundType = 0;
					Find->Symbol = &Symbol;
					return false;
				}
			}
			symbol **s = Find->File->Module->Globals.GetUnstablePtr(*N->ID.Name);
			if(s)
			{
				Find->FoundType = 0;
				Find->Symbol = *s;
				return false;
			}
		}
		string ID = *N->ID.Name;
		for(import &Import : Find->File->Imported)
		{
			if(ID == Import.As || (Import.As == "" && ID == Import.M->Name))
			{
				Find->FoundType = 1;
				Find->Module = Import.M;
				return false;
			}
			else if(Import.As == "")
			{
				symbol **S = Import.M->Globals.GetUnstablePtr(ID);
				if(S)
				{
					Find->FoundType = 0;
					Find->Symbol = *S;
					return false;
				}
				// FindType
			}
		}
	}
	else if(N->Type == AST_SELECTOR)
	{
		// HERE
		// R->Selector.Operand = CopyASTNode(N->Selector.Operand);
		if(PositionInRange(N->ErrorInfo->Range, Find->Line, Find->Char))
		{
			u32 Ti = N->Selector.Type;
			if(Ti == Basic_error)
				return true;

			if(auto Info = SelectorInfo.find(N); Info != SelectorInfo.end())
			{
				switch(Info->second.Kind)
				{
					case SELECT_MOD_GLOBAL:
					{
						Find->FoundType = 0;
						Find->Symbol = Info->second.ModGlobal;
						return false;
					} break;
					case SELECT_TYPE:
					{
						if(auto TypeNode = TypeNodeRecord.find(Info->second.ModType); TypeNode != TypeNodeRecord.end())
						{
							const type *T = GetType(Info->second.ModType);
							switch(T->Kind)
							{
								case TypeKind_Struct:
								{
									Assert(TypeNode->second->Type == AST_STRUCTDECL);
									for(node *Mem : TypeNode->second->StructDecl.Members)
									{
										Assert(Mem->Type == AST_VAR);
										if(Mem->Var.Name && *Mem->Var.Name == *N->Selector.Member)
										{
											Find->FoundType = 2;
											Find->TypeLocation = {*Mem->Var.Name, Info->second.ModType, Mem->ErrorInfo};
											return false;
										}
									}
								} break;
								case TypeKind_Enum:
								{
									Assert(TypeNode->second->Type == AST_ENUM);
									for(node *Mem : TypeNode->second->Enum.Items)
									{
										Assert(Mem->Type == AST_LISTITEM);
										if(Mem->Item.Name && *Mem->Item.Name == *N->Selector.Member)
										{
											Find->FoundType = 2;
											Find->TypeLocation = {*Mem->Item.Name, Info->second.ModType, Mem->ErrorInfo};
											return false;
										}
									}
								} break;
								default:
								{
								} break;
							}
						}
					} break;
				}
			}
		}
	}
	return true;
}

slice<scope_dump> SortClosestScopes(scratch_arena *Arena, u32 Line, u32 Char)
{
	uint Count = 0;
	for(scope_dump &Scope : ScopesToDump)
	{
		range Range = {
			.StartLine = Scope.From->Range.StartLine,
			.EndLine = Scope.To->Range.EndLine,
			.StartChar = Scope.From->Range.StartChar,
			.EndChar = Scope.To->Range.EndChar,
		};
		if(PositionInRange(Range, Line, Char))
		{
			Count++;
		}
	}
	array<scope_dump> Scopes(Arena->Allocate(sizeof(scope_dump) * Count), Count); 
	uint At = 0;
	for(scope_dump &Scope : ScopesToDump)
	{
		range Range = {
			.StartLine = Scope.From->Range.StartLine,
			.EndLine = Scope.To->Range.EndLine,
			.StartChar = Scope.From->Range.StartChar,
			.EndChar = Scope.To->Range.EndChar,
		};
		if(PositionInRange(Range, Line, Char))
		{
			Scopes[At++] = Scope;
		}
	}
	Scopes.Count = At;
	range R = { 
		.StartLine = (i32)Line,
		.StartChar = (i16)Char,
	};
	Scopes.Sort([](void *Arg, const void *ap, const void *bp) -> int {
			auto a = *(const scope_dump **)ap;
			auto b = *(const scope_dump **)bp;
			auto l = (range *)Arg;
			if (a->From->Range.StartLine == b->From->Range.StartLine)
			{
				return l->StartChar - a->From->Range.StartChar < l->StartChar - b->From->Range.StartChar;
			}
			return l->StartLine - a->From->Range.StartLine < l->StartLine - b->From->Range.StartLine;
	}, &R);
	return SliceFromArray(Scopes);
}

find_result IPCFindSymbol(file *File, u32 Line, u32 Char, scratch_arena *Arena)
{
	slice<scope_dump> Scopes = SortClosestScopes(Arena, Line, Char);
	find_result Find = {};
	For(File->Nodes)
	{
		Find = {.Line = Line, .Char = Char, .Scopes = Scopes, .File = File, .FoundType = -1};
		WalkASTNode(*it, IPCSearchNode, &Find);
		if(Find.FoundType != -1)
			break;
	}
	return Find;
}

enum recurse_completion_found_type
{
	RCFT_Nothing,
	RCFT_Selector,
	RCFT_Symbol,
	RCFT_Module,
	RCFT_Final,
};

struct recurse_completion_found
{
	recurse_completion_found_type Type;
	union
	{
		u32 SelectorType;
		symbol *s;
		module *Module;
		dynamic<completion_result> Results;
	};
};

// @LEAK:
completion_result CompleteMakeFnResult(symbol *s)
{
	const type *T = GetType(s->Type);
	if(T->Kind == TypeKind_Pointer)
		T = GetType(T->Pointer.Pointed);
	if(T->Kind != TypeKind_Function)
		return {};
	auto b = MakeBuilder();
	b += *s->Name;
	b += '(';
	for(int i = 0; i < T->Function.ArgCount; ++i)
	{
		string Arg = *s->Node->Fn.Args[i]->Var.Name;
		if(i == 0)
		{
			b.printf("${1:%.*s: %s}", (int)Arg.Size, Arg.Data, GetTypeName(T->Function.Args[i]));
		}
		else
		{
			b.printf(", %.*s: %s", (int)Arg.Size, Arg.Data, GetTypeName(T->Function.Args[i]));
		}
	}
	b += ')';
	string Insert = MakeString(b);
	b = MakeBuilder();
	//b += *s->Name;
	b += "fn(";
	for(int i = 0; i < T->Function.ArgCount; ++i)
	{
		string Arg = *s->Node->Fn.Args[i]->Var.Name;
		if(i != 0)
			b += ", ";
		b.printf("%.*s: %s", (int)Arg.Size, Arg.Data, GetTypeName(T->Function.Args[i]));
	}
	b += ')';
	string TypeSpelling = MakeString(b);

	return completion_result{completion_item_kind::Function, *s->Name, TypeSpelling, Insert, 2 /* Snippet */};
}

recurse_completion_found CompleteBehind(file *File, slice<scope_dump> Scopes, recurse_completion_found Behind, bool IsFirst, string ID, recurse_completion_found &r)
{
	using cik = completion_item_kind;
	u32 PrevType = Basic_error;
	switch(Behind.Type)
	{
		case RCFT_Nothing:
		{
			for(scope_dump &Scope : Scopes)
			{
				for(symbol &s : Scope.Symbols)
				{
					if(!s.Name || s.Type == Basic_error)
						continue;
					if(IsFirst && StringStartsWith(*s.Name, ID))
					{
						cik Kind = cik::Variable;
						if(s.Node && s.Node->Type == AST_FN)
						{
							completion_result Result = CompleteMakeFnResult(&s);
							if((u32)Result.Kind != 0)
							{
								r.Results.Push(Result);
							}
						}
						else
						{
							r.Results.Push(completion_result{Kind, *s.Name, GetTypeNameAsString(s.Type), *s.Name, 1});
						}
					}
					else if(*s.Name == ID)
						return {RCFT_Symbol, {.s=&s}};
				}
			}
			if(IsFirst)
			{
				for(key &Key : File->Module->Globals.Keys)
				{
					if(StringStartsWith(Key.N, ID))
					{
						auto s = File->Module->Globals[Key.N];
						cik Kind = cik::Variable;
						if(s->Node && s->Node->Type == AST_FN)
						{
							completion_result Result = CompleteMakeFnResult(s);
							if((u32)Result.Kind != 0)
							{
								r.Results.Push(Result);
							}
						}
						else
						{
							r.Results.Push(completion_result{Kind, *s->Name, GetTypeNameAsString(s->Type), *s->Name, 1});
						}
					}
				}
			}
			else
			{
				if(auto s = File->Module->Globals.GetUnstablePtr(ID); s != nullptr)
				{
					return {RCFT_Symbol, {.s=*s}};
				}
			}
			for(import& Import : File->Imported)
			{
				if(Import.As == "*")
				{
					if(IsFirst)
					{
						for(key &Key : Import.M->Globals.Keys)
						{
							if(StringStartsWith(Key.N, ID))
							{
								auto s = Import.M->Globals[Key.N];
								cik Kind = cik::Variable;
								if(s->Node && s->Node->Type == AST_FN)
								{
									completion_result Result = CompleteMakeFnResult(s);
									if((u32)Result.Kind != 0)
									{
										r.Results.Push(Result);
									}
								}
								else
								{
									r.Results.Push(completion_result{Kind, *s->Name, GetTypeNameAsString(s->Type), *s->Name, 1});
								}
							}
						}
					}
					else
					{
						symbol **s = Import.M->Globals.GetUnstablePtr(ID);
						if(s)
							return {RCFT_Symbol, {.s=*s}};
					}
				}
				else
				{
					if(IsFirst)
					{
						if(StringStartsWith(Import.M->Name, ID))
						{
							r.Results.Push(completion_result {cik::Module, Import.M->Name, STR_LIT("module"), Import.M->Name, 1});
						}
						if(Import.As != "" && StringStartsWith(Import.As, ID))
						{
							r.Results.Push(completion_result {cik::Module, Import.As, STR_LIT("module"), Import.M->Name, 1});
						}
					}
					else
					{
						if(Import.M->Name == ID)
						{
							return {RCFT_Module, {.Module=Import.M}};
						}
						if(Import.As != "" && Import.As == ID)
						{
							return {RCFT_Module, {.Module=Import.M}};
						}
					}
				}
			}
		} break;
		case RCFT_Module:
		{
			if(IsFirst)
			{
				for(key &Key : Behind.Module->Globals.Keys)
				{
					if(StringStartsWith(Key.N, ID))
					{
						auto s = Behind.Module->Globals[Key.N];
						cik Kind = cik::Variable;
						if(s->Node && s->Node->Type == AST_FN)
						{
							completion_result Result = CompleteMakeFnResult(s);
							if((u32)Result.Kind != 0)
							{
								r.Results.Push(Result);
							}
						}
						else
						{
							r.Results.Push(completion_result{Kind, *s->Name, GetTypeNameAsString(s->Type), *s->Name, 1});
						}
					}
				}
			}
			else
			{
				symbol **s = Behind.Module->Globals.GetUnstablePtr(ID);
				if(s)
					return {RCFT_Symbol, {.s=*s}};
				u32 T = FindType(File->Checker, &ID, &Behind.Module->Name);
				if(T != Basic_error)
				{
					return {RCFT_Selector, {.SelectorType=T}};
				}
			}
		} break;
		case RCFT_Symbol:
		PrevType = Behind.s->Type;
		[[fallthrough]];
		case RCFT_Selector:
		{
			if(Behind.Type == RCFT_Selector)
				PrevType = Behind.SelectorType;
			const type *T = GetType(PrevType);
			switch(T->Kind)
			{
				case TypeKind_Struct:
				{
					for(struct_member &Member : T->Struct.Members)
					{
						if(IsFirst)
						{
							if(StringStartsWith(Member.ID, ID))
								r.Results.Push({cik::Field, Member.ID, GetTypeNameAsString(Member.Type)});
						}
						else
						{
							if(Member.ID == ID)
								return {RCFT_Selector, {.SelectorType=Member.Type}};
						}
					}
				} break;
				case TypeKind_Enum:
				{
					for(enum_member &Member : T->Enum.Members)
					{
						if(IsFirst)
						{
							if(StringStartsWith(Member.Name, ID))
								r.Results.Push({cik::EnumMember, Member.Name, GetTypeNameAsString(T->Enum.Type)});
						}
					}
				} break;
				default: {} break;
			}
		} break;
		case RCFT_Final: {} break;
	}
	return r;
}

recurse_completion_found RecurseCompletion(file *File, slice<scope_dump> Scopes, token *Tokens, ssize_t i, bool ExpectingDot, bool IsFirst)
{
	if(i < 0)
		return {};

	recurse_completion_found Behind = RecurseCompletion(File, Scopes, Tokens, i-1, !ExpectingDot, false);
	recurse_completion_found r = {};
	if(IsFirst)
		r.Type = RCFT_Final;

	switch(Tokens[i].Type)
	{
		case T_ID:
		{
			if(ExpectingDot)
				return {};

			string ID = *Tokens[i].ID;
			r = CompleteBehind(File, Scopes, Behind, IsFirst, ID, r);
		} break;
		case T_DOT:
		{
			if(IsFirst)
				r = CompleteBehind(File, Scopes, Behind, IsFirst, STR_LIT(""), r);
			else
				r = Behind;
		} break;
		default:
		{
		} break;
	}
	return r;
}

dynamic<completion_result> DoCompletion(file *File, string Line, int LineNo, int Char, scratch_arena *Arena)
{
	if(Char == 0)
		return {};

	slice<scope_dump> Scopes = SortClosestScopes(Arena, LineNo, Char);
	error_info ErrorInfo = File->Tokens[0].ErrorInfo;
	ErrorInfo.Range.StartLine = LineNo;
	ErrorInfo.Range.StartChar = 0;
	ErrorInfo.Range.EndLine = LineNo;
	ErrorInfo.Range.EndChar = 0;
	file *Parsed = StringToTokens(Line, ErrorInfo);
	ssize_t TokenCount = ArrLen(Parsed->Tokens);

	ssize_t CompletionTokenI = 0;
	for(; CompletionTokenI < TokenCount; ++CompletionTokenI)
	{
		if(PositionInRange(Parsed->Tokens[CompletionTokenI].ErrorInfo.Range, LineNo, Char-1))
		{
			break;
		}
	}
	if(CompletionTokenI == TokenCount)
		return {};

	recurse_completion_found Result = RecurseCompletion(File, Scopes, Parsed->Tokens, CompletionTokenI, Parsed->Tokens[CompletionTokenI].Type == T_DOT, true);

	ArrFree(Parsed->Tokens);
	// @LEAK: Parsed
	if(Result.Type == recurse_completion_found_type::RCFT_Final)
		return Result.Results;
	return {};
}

