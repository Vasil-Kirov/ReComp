#pragma once
#include "Dynamic.h"
#include "Module.h"
#include "VString.h"
#include "Errors.h"
#include <unordered_map>

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

struct scope_dump
{
	const error_info *From;
	const error_info *To;
	slice<symbol> Symbols;
};

enum selector_type
{
	SELECT_MOD_GLOBAL,
	SELECT_TYPE,
};

struct selector_info
{
	selector_type Kind;
	union
	{
		symbol *ModGlobal;
		u32 ModType;
	};
};

extern std::unordered_map<node*, selector_info> SelectorInfo;
extern std::unordered_map<u32, node*> TypeNodeRecord;
extern bool DumpingInfo;
extern string DumpFileName;
extern binary_blob *GlobalBlob;
extern dynamic<error_dump> ErrorsToDump;
extern dynamic<scope_dump> ScopesToDump;
binary_blob StartOutput();

void DumpU32(binary_blob *Blob, u32 Num);
void DumpFile(binary_blob *Blob, file *File);
void DumpModule(binary_blob *Blob, module* M);
void DumpTypeTable(binary_blob *Blob);
void DumpString(binary_blob *Blob, string S);
void DumpError(binary_blob *Blob, error_dump Error);
void DumpScope(binary_blob *Blob, scope_dump Symbol);
void AddErrorToDump(error_dump Error);
void AddScopeToDump(scope_dump Symbol);
void PipeInfoBlob(binary_blob *Blob);
void DumpFileTokens(binary_blob *Blob, file *File);
void DumpLocationErrI(binary_blob *Blob, const error_info *ErrI);

