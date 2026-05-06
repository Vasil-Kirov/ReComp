#pragma once
#include "Dynamic.h"
#include "Module.h"
#include "VString.h"
#include "Errors.h"

struct binary_blob
{
	dynamic<u8> Buf;
};

struct error_dump
{
	error_info ErrI;
	string Code;
	const char *Message;
};

extern bool DumpingInfo;
extern string DumpFileName;
extern binary_blob *GlobalBlob;
binary_blob StartOutput();

void DumpU32(binary_blob *Blob, u32 Num);
void DumpFile(binary_blob *Blob, file *File);
void DumpModule(binary_blob *Blob, module* M);
void DumpTypeTable(binary_blob *Blob);
void DumpString(binary_blob *Blob, string S);
void DumpError(binary_blob *Blob, error_dump Error);
void AddErrorToDump(error_dump Error);
void WriteBlobToFile(binary_blob *Blob);

