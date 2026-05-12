#include "Module.h"
#include "Platform.h"
#include "Semantics.h"
#include "Type.h"
#include "VString.h"
#include "DumpInfo.h"

// @THREADING: PUT MUTEXES EVERYWHERE HERE

string DumpFileName = {};
binary_blob *GlobalBlob = NULL;
dynamic<error_dump> ErrorsToDump = {};
dynamic<scope_dump> ScopesToDump = {};

binary_blob StartOutput()
{
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
	return binary_blob {};
};

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
				auto b = MakeBuilder();
				b += '(';
				for(int i = 0; i < T->Function.ArgCount; ++i)
				{
					if(i != 0)
						b += ", ";
					PushBuilderFormated(&b, "%s", GetTypeName(T->Function.Args[i]));
				}
				b += ')';

				DumpString(Blob, MakeString(b));
			} break;
			default: {} break;
		}
	}
}

void WriteBlobToFile(binary_blob *Blob)
{
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
	PlatformDeleteFile(DumpFileName.Data);
	PlatformWriteFile(DumpFileName.Data, Blob->Buf.Data, Blob->Buf.Count);
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

void AddErrorToDump(error_dump Error)
{
	ErrorsToDump.Push(Error);
}

void AddScopeToDump(scope_dump Symbol)
{
	ScopesToDump.Push(Symbol);
}

