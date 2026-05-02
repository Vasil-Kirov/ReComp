#include "Module.h"
#include "Platform.h"
#include "Semantics.h"
#include "Type.h"
#include "VString.h"
#include "DumpInfo.h"

string DumpFileName = {};

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

void DumpLocation(binary_blob *Blob, const error_info *ErrI)
{
	string FileName = string {ErrI->FileName, strlen(ErrI->FileName)};
	DumpString(Blob, FileName);
	DumpU32(Blob, ErrI->Range.StartLine);
	DumpU32(Blob, ErrI->Range.StartChar);
	DumpU32(Blob, ErrI->Range.EndLine);
	DumpU32(Blob, ErrI->Range.EndChar);
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
		DumpLocation(Blob, s->Node->ErrorInfo);
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
		DumpString(Blob, GetTypeNameAsString(T));
		switch(T->Kind)
		{
			case TypeKind_Struct:
			{
				DumpU32(Blob, T->Struct.Members.Count);
				For(T->Struct.Members)
				{
					DumpU32(Blob, it->Type);
					DumpString(Blob, it->ID);
				}
			} break;
			default: {} break;
		}
	}
}

void WriteBlobToFile(binary_blob *Blob)
{
	PlatformDeleteFile(DumpFileName.Data);
	PlatformWriteFile(DumpFileName.Data, Blob->Buf.Data, Blob->Buf.Count);
}

void WriteStringError(const char *FileName, int LineNumber, const char *ErrorMsg)
{
	string FileString = string {FileName, strlen(FileName)};
	string ErrorString = string {ErrorMsg, strlen(ErrorMsg)};
	binary_blob Blob = StartOutput();
	DumpString(&Blob, STR_LIT(":ERRS\n"));
	DumpString(&Blob, FileString);
	DumpU32(&Blob, LineNumber);
	DumpString(&Blob, ErrorString);
	WriteBlobToFile(&Blob);
}


