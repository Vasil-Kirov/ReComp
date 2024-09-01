#pragma once
#include "Basic.h"
#include "VString.h"
#include "Log.h"



void PlatformWriteFile(const char *Path, u8 *Data, u32 Size); 
string ReadEntireFile(string Path);
void PlatformFreeMemory(void *Memory);
void *PlatformReserveMemory(size_t Size);
void PlatformAllocateReserved(void *Memory, size_t Size);
void PlatformOutputString(string String, log_level Level);
b32 PlatformDeleteFile(const char *Path);

