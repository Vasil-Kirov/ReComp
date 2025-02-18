#include <signal.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Platform.h"
#include "shlwapi.h"

#pragma comment(lib, "shlwapi.lib")

struct windows_signal_handler
{
	sig_proc UserProc;
	void *UserData;
};

thread_local windows_signal_handler SignalHandlerGlobal = {};

bool PlatformIsPathValid(const char *Path)
{
	return PathFileExistsA(Path);
}

void SignalHandler(int Sig)
{
	if(Sig == SIGSEGV && SignalHandlerGlobal.UserProc)
		SignalHandlerGlobal.UserProc(SignalHandlerGlobal.UserData);
}

void PlatformSetSignalHandler(sig_proc Proc, void *Data)
{
	SignalHandlerGlobal = windows_signal_handler { .UserProc = Proc, .UserData = Data };
	signal(SIGSEGV, SignalHandler);
}

void PlatformClearSignalHandler()
{
	signal(SIGSEGV, SIG_DFL);
}

b32 PlatformDeleteFile(const char *Path)
{
	return DeleteFileA(Path);
}

t_handle PlatformCreateThread(t_proc Proc, void *PassValue)
{
	return CreateThread(NULL, 0, Proc, PassValue, 0, NULL);
}

t_semaphore PlatformCreateSemaphore(uint MaxCount)
{
	return CreateSemaphoreA(NULL, 0, MaxCount, NULL);
}

void PlatformSleepOnSemaphore(t_semaphore Semaphore)
{
	WaitForSingleObject(Semaphore, INFINITE);
}

void PlatformSignalSemaphore(t_semaphore Semaphore)
{
	ReleaseSemaphore(Semaphore, 1, NULL);
}

void PlatformWriteFile(const char *Path, u8 *Data, u32 Size)
{
    HANDLE File = CreateFile(Path, FILE_GENERIC_WRITE , 0, 0,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
			0);
	// NOTE(Vasko): FILE_GENERIC_WRITE should do that by itself
	// but it doesn't
	SetFilePointer(File, 0, 0, FILE_END);
    
    DWORD BytesWritten;
    WriteFile(File, Data, Size, &BytesWritten, 0);
	CloseHandle(File);
}

string ReadEntireFile(string Path)
{
	HANDLE File = CreateFileA(Path.Data, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(File == NULL || File == INVALID_HANDLE_VALUE) 
	{
		string Result = {};
		return Result;
	}

	LARGE_INTEGER FileSize;
	GetFileSizeEx(File, &FileSize);
	size_t Size = FileSize.QuadPart;
	
	char *Data = (char *)malloc(FileSize.QuadPart + 1);
	memset(Data, 0, FileSize.QuadPart+1);
	u64 HaveRead = 0;
	while(FileSize.QuadPart > 0)
	{
		DWORD ReadThisLoop;
		DWORD ToRead = (DWORD)FileSize.QuadPart;
		if(FileSize.QuadPart > 0xffff'ffff)
		{
			ToRead = 0xffff'ffff;
		}
		ReadFile(File, (char *)Data+HaveRead, ToRead, &ReadThisLoop, NULL);
		if(ReadThisLoop == 0)
		{
			CloseHandle(File);
			string Result = {};
			return Result;
		}
		FileSize.QuadPart -= ReadThisLoop;
		HaveRead += ReadThisLoop;
	}

	string Result = MakeString(Data, Size);
	
	free(Data);
	CloseHandle(File);
	return Result;
}

void *PlatformReserveMemory(size_t Size)
{
	return VirtualAlloc(NULL, Size, MEM_RESERVE, PAGE_NOACCESS);
}

void PlatformAllocateReserved(void *Memory, size_t Size)
{
	VirtualAlloc(Memory, Size, MEM_COMMIT, PAGE_READWRITE);
}

void PlatformOutputString(string String, log_level Level)
{
	if(Level == LOG_CLEAN)
	{
		HANDLE STDOUT = GetStdHandle(STD_OUTPUT_HANDLE);
		WriteFile(STDOUT, String.Data, String.Size, NULL, 0);
		return;
	}

	SetConsoleOutputCP(CP_UTF8);
	//		MAGIC NUMBERS! (ored rgb)
	//		13 = r | b | intense
	//		4 = r
	//		6 = r | g
	//		8 = intense
	u8 Colors[] = {
		13,
		4,
		6,
		FOREGROUND_GREEN | FOREGROUND_BLUE,
		FOREGROUND_GREEN | FOREGROUND_INTENSITY
	};
	HANDLE STDOUT;
	if (Level > LOG_ERROR)
		STDOUT = GetStdHandle(STD_OUTPUT_HANDLE);
	else
		STDOUT = GetStdHandle(STD_ERROR_HANDLE);
	//		FATAL = 0
	//		ERROR = 1
	//		WARN = 2
	//		INFO = 4
	
	u16 Attrib = Colors[Level];
	SetConsoleTextAttribute(STDOUT, Attrib);
	//OutputDebugStringA(String); 
	unsigned long written = 0;
	if(String.Size != 0)
		WriteFile(STDOUT, String.Data, String.Size, &written, 0);
	SetConsoleTextAttribute(STDOUT, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
}

void PlatformFreeMemory(void *Mem, size_t)
{
	VirtualFree(Mem, 0, MEM_RELEASE);
}

