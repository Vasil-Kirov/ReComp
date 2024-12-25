#include "Module.h"
#include "Platform.h"
#include "Semantics.h"
#include "Type.h"
#include "VString.h"

struct binary_blob
{
	dynamic<u8> Buf;
};

binary_blob StartOutput()
{
	return binary_blob {};
};

void DumpU32(binary_blob *Blob, u32 Num)
{
	Blob->Buf.Push((u8)(Num >> 24));
	Blob->Buf.Push((u8)(Num >> 16));
	Blob->Buf.Push((u8)(Num >> 8));
	Blob->Buf.Push((u8)(Num));
}

void DumpString(binary_blob *Blob, string S)
{
	DumpU32(Blob, S.Size);
	for(int i = 0; i < S.Size; ++i)
	{
		Blob->Buf.Push(S.Data[i]);
	}
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
	PlatformWriteFile("rcp.dump", Blob->Buf.Data, Blob->Buf.Count);
}

void WriteStringError(const char *FileName, int LineNumber, const char *ErrorMsg)
{
	auto b = MakeBuilder();
	b += "....";
	b += FileName;
	b += "..";
	b.Data.Push((u8)(LineNumber >> 24));
	b.Data.Push((u8)(LineNumber >> 16));
	b.Data.Push((u8)(LineNumber >> 8));
	b.Data.Push((u8)(LineNumber));
	b.Size += 4;
	b += ErrorMsg;
	b += "..";
	PlatformWriteFile("rcp.dump", (u8 *)b.Data.Data, b.Size);
}


