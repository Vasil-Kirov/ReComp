#pragma once
#include "Basic.h"
#include "String.h"



string ReadEntireFile(string Path);
void *PlatformReserveMemory(size_t Size);
void PlatformAllocateReserved(void *Memory, size_t Size);
void PlatformOutputString(string String, log_level Level);

