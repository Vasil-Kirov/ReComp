#include "IPC.h"
#include "Module.h"
#include "Platform.h"
#include "Semantics.h"
#include "Type.h"
#include "VString.h"
#include "DumpInfo.h"

// @THREADING: PUT MUTEXES EVERYWHERE HERE

binary_blob *GlobalBlob = NULL;
dynamic<error_dump> ErrorsToDump = {};
dynamic<scope_dump> ScopesToDump = {};
bool AlreadyPipedInfo = false;
std::unordered_map<node*, selector_info> SelectorInfo = {};
std::unordered_map<u32, node*> TypeNodeRecord = {};

binary_blob StartOutput()
{
	/*
#if CM_LINUX
	DumpFileName = STR_LIT("/tmp/rcp.dump");
#elif _WIN32
	char buf[4096] = {};
	int r = GetTempPathA(4096, buf);
	Assert(r);
	auto b = MakeBuilder();
	b.printf("%s\\rcp.dump", buf);
	DumpFileName = MakeString(b);
#else
#error Unimplemented default dump file name
#endif
*/
	return binary_blob {};
};

void DumpU64(binary_blob *Blob, u64 Num)
{
	Blob->Buf.Push((u8)(0xFF & (Num)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 8)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 16)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 24)));

	Blob->Buf.Push((u8)(0xFF & (Num >> 32)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 40)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 48)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 56)));
}

void DumpU32(binary_blob *Blob, u32 Num)
{
	Blob->Buf.Push((u8)(0xFF & (Num)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 8)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 16)));
	Blob->Buf.Push((u8)(0xFF & (Num >> 24)));
}

void DumpString(binary_blob *Blob, string S)
{
	DumpU32(Blob, S.Size);
	for(int i = 0; i < S.Size; ++i)
	{
		Blob->Buf.Push(S.Data[i]);
	}
}

void DumpLocation(binary_blob *Blob, string FileName, range Range)
{
	DumpString(Blob, FileName);
	DumpU32(Blob, Range.StartLine);
	DumpU32(Blob, Range.StartChar);
	DumpU32(Blob, Range.EndLine);
	DumpU32(Blob, Range.EndChar);
}

void DumpLocationErrI(binary_blob *Blob, const error_info *ErrI)
{
	string FileName = string {ErrI->FileName, strlen(ErrI->FileName)};
	DumpLocation(Blob, FileName, ErrI->Range);
}

void DumpModule(binary_blob *Blob, module* M)
{
	DumpString(Blob, M->Name);
	DumpU32(Blob, M->Globals.Data.Count);
	For(M->Globals.Data)
	{
		symbol *s = *it;
		DumpString(Blob, *s->Name);
		DumpU32(Blob, s->Type);
		DumpLocationErrI(Blob, s->Node->ErrorInfo);
	}
}

void DumpFile(binary_blob *Blob, file *File)
{
	DumpString(Blob, File->Name);
	DumpString(Blob, File->Module->Name);
	DumpU32(Blob, File->Imported.Count);
	For(File->Imported)
	{
		DumpString(Blob, it->M->Name);
		DumpString(Blob, it->As);
	}
}

void DumpTypeTable(binary_blob *Blob)
{
	uint TypeCount = GetTypeCount();
	DumpU32(Blob, TypeCount);
	for(int i = 0; i < TypeCount; ++i)
	{
		const type *T = GetType(i);
		DumpU32(Blob, T->Kind);
		//DumpString(Blob, GetTypeNameAsString(T));
		switch(T->Kind)
		{
			case TypeKind_Struct:
			{
				DumpU32(Blob, T->Struct.Members.Count);
				For(T->Struct.Members)
				{
					DumpString(Blob, it->ID);
					DumpU32(Blob, it->Type);
				}
				DumpString(Blob, T->Struct.Name);
				DumpU32(Blob, T->Struct.Flags);
			} break;
			case TypeKind_Function:
			{
				DumpU32(Blob, T->Function.Returns.Count);
				For(T->Function.Returns)
					DumpU32(Blob, *it);
				DumpU32(Blob, T->Function.ArgCount);
				for(int i = 0; i < T->Function.ArgCount; ++i)
					DumpU32(Blob, T->Function.Args[i]);
			} break;
			case TypeKind_Array:
			{
				DumpU32(Blob, T->Array.Type);
				DumpU32(Blob, T->Array.MemberCount);
			} break;
			case TypeKind_Basic:
			{
				DumpU32(Blob, T->Basic.Kind);
				DumpU32(Blob, T->Basic.Flags);
				DumpU32(Blob, T->Basic.Size);
				DumpString(Blob, T->Basic.Name);
			} break;
			case TypeKind_Pointer:
			{
				DumpU32(Blob, T->Pointer.Pointed);
				DumpU32(Blob, T->Pointer.Flags);
			} break;
			case TypeKind_Slice:
			{
				DumpU32(Blob, T->Slice.Type);
			} break;
			case TypeKind_Vector:
			{
				DumpU32(Blob, T->Vector.Kind);
				DumpU32(Blob, T->Vector.ElementCount);
			} break;
			case TypeKind_Enum:
			{
				DumpString(Blob, T->Enum.Name);
				DumpU32(Blob, T->Enum.Members.Count);
				For(T->Enum.Members)
				{
					DumpString(Blob, it->Name);
					DumpU64(Blob, it->Value.Int.IsSigned ? it->Value.Int.Signed : it->Value.Int.Unsigned);
				}
				DumpU32(Blob, T->Enum.Type);
			} break;
			case TypeKind_Generic:
			{
				DumpString(Blob, T->Generic.Name);
			} break;
			default: {} break;
		}
	}
}

void PipeInfoBlob(binary_blob *Blob)
{
	if(AlreadyPipedInfo)
		return;

	if(Blob == NULL)
	{
		if(GlobalBlob)
			Blob = GlobalBlob;
		else
			return;
	}
	DumpString(Blob, STR_LIT(":ERRS\n"));
	DumpU32(Blob, ErrorsToDump.Count);
	For(ErrorsToDump)
	{
		DumpError(Blob, *it);
	}
	if(ScopesToDump.Count != 0)
	{
		DumpString(Blob, STR_LIT(":SCOP\n"));
		DumpU32(Blob, ScopesToDump.Count);
		For(ScopesToDump)
		{
			DumpScope(Blob, *it);
		}
	}

	IPCSendMessage(Blob->Buf.Data, Blob->Buf.Count);
	AlreadyPipedInfo = true;
	
	//PlatformDeleteFile(DumpFileName.Data);
	//PlatformWriteFile(DumpFileName.Data, Blob->Buf.Data, Blob->Buf.Count);
}

void DumpError(binary_blob *Blob, error_dump Error)
{
	string ErrorString = string {Error.Message, strlen(Error.Message)};
	DumpString(Blob, ErrorString);
	DumpString(Blob, Error.Code);
	DumpLocationErrI(Blob, &Error.ErrI);
}

void DumpScope(binary_blob *Blob, scope_dump Scope)
{
	range Range = {
		.StartLine = Scope.From->Range.StartLine,
		.EndLine = Scope.To->Range.EndLine,
		.StartChar = Scope.From->Range.StartChar,
		.EndChar = Scope.To->Range.EndChar,
	};
	string FileName = string {Scope.From->FileName, strlen(Scope.From->FileName)};
	DumpLocation(Blob, FileName, Range);
	size_t CountValidSymbols = Scope.Symbols.Count;
	For(Scope.Symbols)
	{
		if(!it->Node)
			CountValidSymbols--;
	}
	

	DumpU32(Blob, CountValidSymbols);
	For(Scope.Symbols)
	{
		if(!it->Node)
			continue;
		DumpString(Blob, *it->Name);
		DumpU32(Blob, it->Type);
		DumpLocationErrI(Blob, it->Node->ErrorInfo);
	}
}

void DumpFileTokens(binary_blob *Blob, file *File)
{
	size_t TokenCount = ArrLen(File->Tokens);
	DumpString(Blob, File->Name);
	DumpU64(Blob, TokenCount);
	for(size_t i = 0; i < TokenCount; ++i)
	{
		token T = File->Tokens[i];
		DumpU32(Blob, T.ErrorInfo.Range.StartLine);
		DumpU32(Blob, T.ErrorInfo.Range.StartChar);
		DumpU32(Blob, T.ErrorInfo.Range.EndLine);
		DumpU32(Blob, T.ErrorInfo.Range.EndChar);
		DumpU32(Blob, T.Type);
		if(T.Type == T_ID)
		{
			DumpString(Blob, *T.ID);
		}
	}
}

void AddErrorToDump(error_dump Error)
{
	ErrorsToDump.Push(Error);
}

void AddScopeToDump(scope_dump Symbol)
{
	ScopesToDump.Push(Symbol);
}

