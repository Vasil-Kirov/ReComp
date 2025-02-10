#pragma once
#include "Basic.h"
#include "VString.h"
#include "Log.h"


#if _WIN32
typedef void *t_handle;
typedef void *t_semaphore;
#else
#include <semaphore.h>
typedef unsigned long t_handle;
typedef sem_t *t_semaphore;
#endif

typedef unsigned long (*t_proc)(void *);
typedef void (*sig_proc)(void *Context);

void PlatformWriteFile(const char *Path, u8 *Data, u32 Size); 
string ReadEntireFile(string Path);
void PlatformFreeMemory(void *Memory, size_t Size);
void *PlatformReserveMemory(size_t Size);
void PlatformAllocateReserved(void *Memory, size_t Size);
void PlatformOutputString(string String, log_level Level);
b32 PlatformDeleteFile(const char *Path);
t_handle PlatformCreateThread(t_proc Proc, void *PassValue);
t_semaphore PlatformCreateSemaphore(uint MaxCount);
void PlatformSleepOnSemaphore(t_semaphore);
void PlatformSignalSemaphore(t_semaphore);
void PlatformSetSignalHandler(sig_proc Proc, void *Data);
void PlatformClearSignalHandler();


