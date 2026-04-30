#pragma once
#include "Dynamic.h"
#include "Module.h"
#include "VString.h"

struct binary_blob
{
	dynamic<u8> Buf;
};

extern bool DumpingInfo;
extern string DumpFileName;
binary_blob StartOutput();

void DumpModule(binary_blob *Blob, module* M);
void DumpTypeTable(binary_blob *Blob);
void WriteBlobToFile(binary_blob *Blob);
void DumpString(binary_blob *Blob, string S);

