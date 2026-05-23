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
		return {};
	u8 *Data = (u8 *)IPCRecvMessage(&Size);
	slice<u8> s = {.Data = Data, .Count = Size};
	return ipc_packet{(ipc_cmd)*Cmd, s};
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

 __attribute__((noreturn))
void IPCListenAndServe()
{
	if(IPCProgramModules.Data == nullptr)
	{
		exit(0);
	}
	IPCSendMessage("0", 1);
	DontExit = true;
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
				u32 line = ReadUInt32(&r) + 1;
				u32 chr = ReadUInt32(&r) + 1;
				file *File = nullptr;
				ForN(IPCProgramModules, Mod)
				{
					For((*Mod)->Files)
					{
						if((*it)->Name == FilePath)
						{
							File = *it;
							break;
						}
					}
					if(File)
						break;
				}
				if(!File)
				{
					binary_blob Blob = StartOutput();
					DumpU32(&Blob, 0);
					break;
				}
			} break;
			case ipc_cmd::DO_COMPLETION:
			{
			} break;
			case ipc_cmd::QUIT:
			{
				Running = false;
			} break;
		}
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
	};
};

void IPCSearchNode(node *N, find_result *Find)
{
	if(!N) return;

	switch (N->Type)
	{
		case AST_INVALID: 
			unreachable; 
			break;

		case AST_RUN:
		{
			for(node *Node : N->Run.Body)
			{
				IPCSearchNode(Node, Find);
			}
		} break;

		case AST_YIELD:
		{
			IPCSearchNode(N->TypedExpr.Expr, Find);
		} break;

		case AST_USING:
		{
			IPCSearchNode(N->TypedExpr.Expr, Find);
		} break;

		case AST_ASSERT:
		{
			IPCSearchNode(N->Assert.Expr, Find);
		} break;

		case AST_VAR:
		{
			IPCSearchNode(N->Var.TypeNode, Find);
			IPCSearchNode(N->Var.Default, Find);
		} break;

		case AST_LIST:
		{
			for(node *Node : N->List.Nodes)
			{
				IPCSearchNode(Node, Find);
			}
		} break;

		case AST_EMBED:
		{
		} break;

		case AST_CHARLIT:
		{
		} break;

		case AST_CONSTANT:
		{
		} break;

		case AST_BINARY:
		{
			IPCSearchNode(N->Binary.Left, Find);
			IPCSearchNode(N->Binary.Right, Find);
		} break;

		case AST_UNARY:
		{
			IPCSearchNode(N->Unary.Operand, Find);
		} break;

		case AST_IFX:
		{
			IPCSearchNode(N->IfX.Expr, Find);
			IPCSearchNode(N->IfX.True, Find);
			IPCSearchNode(N->IfX.False, Find);
		} break;

		case AST_IF:
		{
			IPCSearchNode(N->If.Expression, Find);
			for(node *Node : N->If.Body)
			{
				IPCSearchNode(Node, Find);
			}
			for(node *Node : N->If.Else)
			{
				IPCSearchNode(Node, Find);
			}
		} break;

		case AST_FOR:
		{
			IPCSearchNode(N->For.Expr1, Find);
			IPCSearchNode(N->For.Expr2, Find);
			IPCSearchNode(N->For.Expr3, Find);
			for(node *Node : N->For.Body)
			{
				IPCSearchNode(Node, Find);
			}
		} break;

		case AST_ID:
		{
			if(!PositionInRange(N->ErrorInfo->Range, Find->Line, Find->Char))
				break;
			bool Found = false;
			for(scope_dump &Scope : Find->Scopes)
			{
				for(symbol &Symbol : Scope.Symbols)
				{
					if(Symbol.Name && *Symbol.Name == *N->ID.Name)
					{
						Find->FoundType = 0;
						Find->Symbol = &Symbol;
						Found = true;
						goto found_id;
					}
				}
			}
			found_id:;
			if(!Found)
			{
				string ID = *N->ID.Name;
				for(import &Import : Find->File->Imported)
				{
					if(ID == Import.As || (Import.As == "" && ID == Import.M->Name))
					{
						Find->FoundType = 1;
						Find->Module = Import.M;
						Found = true;
						goto found_mod;
					}
					else if(Import.As == "")
					{
						symbol **S = Import.M->Globals.GetUnstablePtr(ID);
						if(S)
						{
							Find->FoundType = 0;
							Find->Symbol = *S;
							goto found_mod;
						}
						// FindType
					}
				}
			}
			found_mod:;
		} break;

		case AST_DECL:
		{
			IPCSearchNode(N->Decl.LHS, Find);
			IPCSearchNode(N->Decl.Expression, Find);
			IPCSearchNode(N->Decl.Type, Find);
		} break;

		case AST_CALL:
		{
			IPCSearchNode(N->Call.Fn, Find);
			for(node *Arg : N->Call.Args)
			{
				IPCSearchNode(Arg, Find);
			}
		} break;

		case AST_RETURN:
		{
			IPCSearchNode(N->Return.Expression, Find);
		} break;

		case AST_PTRTYPE:
		{
			IPCSearchNode(N->PointerType.Pointed, Find);
		} break;

		case AST_ARRAYTYPE:
		{
			IPCSearchNode(N->ArrayType.Type, Find);
			IPCSearchNode(N->ArrayType.Expression, Find);
		} break;

		case AST_FN:
		{
			for(node *Arg : N->Fn.Args)
				IPCSearchNode(Arg, Find);
			for(node *Ret : N->Fn.ReturnTypes)
				IPCSearchNode(Ret, Find);
			for(node *Node : N->Fn.Body)
				IPCSearchNode(Node, Find);
			IPCSearchNode(N->Fn.ProfileCallback, Find);
		} break;

		case AST_CAST:
		{
			IPCSearchNode(N->Cast.Expression, Find);
			IPCSearchNode(N->Cast.TypeNode, Find);
		} break;

		case AST_TYPELIST:
		{
			IPCSearchNode(N->TypeList.TypeNode, Find);
			for(node *Item : N->TypeList.Items)
				IPCSearchNode(Item, Find);
		} break;

		case AST_INDEX:
		{
			IPCSearchNode(N->Index.Operand, Find);
			IPCSearchNode(N->Index.Expression, Find);
		} break;

		case AST_STRUCTDECL:
		{
			for(node *Node : N->StructDecl.Members)
				IPCSearchNode(Node, Find);
		} break;

		case AST_ENUM:
		{
			for(node *Node : N->Enum.Items)
				IPCSearchNode(Node, Find);
			IPCSearchNode(N->Enum.Type, Find);
		} break;

		case AST_SELECTOR:
		{
			// HERE
			// R->Selector.Operand = CopyASTNode(N->Selector.Operand);
			if(PositionInRange(N->ErrorInfo->Range, Find->Line, Find->Char))
			{
				
			}
			else
			{
				IPCSearchNode(N->Selector.Operand, Find);
			}
		} break;

		case AST_SIZE:
		{
			IPCSearchNode(N->Size.Expression, Find);
		} break;

		case AST_TYPEOF:
		{
			IPCSearchNode(N->TypeOf.Expression, Find);
		} break;

		case AST_GENERIC:
		{
		} break;

		case AST_RESERVED:
		{
		} break;

		case AST_NOP:
		case AST_BREAK:
		case AST_CONTINUE:
			// No additional data to copy
			break;

		case AST_GENSTRUCTTYPE:
		{
			for(node *Node : N->GenericStructType.Args)
				IPCSearchNode(Node, Find);
			IPCSearchNode(N->GenericStructType.ID, Find);
		} break;

		case AST_LISTITEM:
		{
			IPCSearchNode(N->Item.Expression, Find);
		} break;

		case AST_SWITCH:
		{
			IPCSearchNode(N->Switch.Expression, Find);
			for(node *Node : N->Switch.Cases)
				IPCSearchNode(Node, Find);
		} break;

		case AST_CASE:
		{
			IPCSearchNode(N->Case.Value, Find);
			for(node *Node : N->Case.Body)
				IPCSearchNode(Node, Find);
		} break;

		case AST_POSTOP:
		{
			IPCSearchNode(N->PostOp.Operand, Find);
		} break;

		case AST_DEFER:
		{
			for(node *Node : N->Defer.Body)
				IPCSearchNode(Node, Find);
		} break;

		case AST_SCOPE:
		{
		} break;

		case AST_TYPEINFO:
		{
			IPCSearchNode(N->TypeInfoLookup.Expression, Find);
		} break;

		case AST_PTRDIFF:
		{
			IPCSearchNode(N->PtrDiff.Left, Find);
			IPCSearchNode(N->PtrDiff.Right, Find);
		} break;

		case AST_FILE_LOCATION:
		{
		} break;
	}
}

void IPCFindSymbol(file *File, u32 Line, u32 Char)
{
	node *Root = nullptr;
	For(File->Nodes)
	{
		const range *r = &(*it)->ErrorInfo->Range;
		if(r->StartLine < Line && r->EndLine > Line)
		{
			Root = *it;
			break;
		}
		if(r->StartLine > Line)
		{
			if(it == File->Nodes.Data)
			{
				return;
			}
			Root = *(it-1);
			break;
		}
	}
}

