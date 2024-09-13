#include "Windows.h"
#include "Platform.h"

b32 PlatformDeleteFile(const char *Path)
{
	return DeleteFileA(Path);
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
	//		MAGIC NUMBERS! (ored rgb)
	//		13 = r | b | intense
	//		4 = r
	//		6 = r | g
	//		8 = intense
	u8 Colors[] = {13, 4, 6, FOREGROUND_GREEN | FOREGROUND_INTENSITY, 
	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE};
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

void PlatformFreeMemory(void *Mem, uint)
{
	VirtualFree(Mem, 0, MEM_RELEASE);
}

