#include "Basic.h"
#include "Log.h"
#include "Platform.h"
#include "VString.h"
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

b32 PlatformDeleteFile(const char *Path)
{
	return unlink(Path) == 0;
}

void PlatformWriteFile(const char *Path, u8 *Data, u32 Size)
{
	int fd = open(Path, O_APPEND);
	if(fd != -1)
	{
		write(fd, Data, Size);
		close(fd);
	}
}

string ReadEntireFile(string Path)
{
	int fd = open(Path.Data, O_APPEND);
	if(fd == -1)
		return STR_LIT("");

	struct stat s_info = {};
	fstat(fd, &s_info);

	char *Data = (char *)malloc(s_info.st_size + 1);
	memset(Data, 0, s_info.st_size+1);
	read(fd, Data, s_info.st_size);

	string Result = MakeString(Data, s_info.st_size);
	free(Data);
	close(fd);
	return Result;
}

void *PlatformReserveMemory(size_t Size)
{
	void *map = mmap(NULL, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if(map == MAP_FAILED)
	{
		perror("mmap reserve failed");
		return NULL;
	}
	return map;
}

void PlatformAllocateReserved(void *Memory, size_t Size)
{
	mmap(Memory, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void PlatformOutputString(string String, log_level Level)
{
	int out_fd = 1;
	if(Level <= LOG_ERROR)
		out_fd = 2;

	const char *Colors[] = {
		"\u001b[35m ",
		"\u001b[31m ",
		"\u001b[33m ",
		"\u001b[36m ",
		"\u001b[32m ",
	};
	write(out_fd, Colors[Level], sizeof(Colors[Level])-1);
	write(out_fd, String.Data, String.Size);
}

void PlatformFreeMemory(void *Mem, uint Size)
{
	munmap(Mem, Size);
}

string RunCommand(const char *Command)
{
	FILE *fp = popen(Command, "r");
	if(!fp)
	{
		LFATAL("Failed to run command %s", Command);
		exit(1);
	}
	string_builder Builder = MakeBuilder();
	char buff[1024] = {};
	while(fgets(buff, sizeof(buff), fp) != NULL) {
		Builder += buff;
		memset(buff, 0, 1024);
	}
	
	pclose(fp);

	return MakeString(Builder);
}

string FindObjectFiles()
{
	string_builder Builder = MakeBuilder();
	// Builder += RunCommand("find / -name crt1.o 2>/dev/null");
	// Builder += RunCommand("find / -name crti.o 2>/dev/null");
	Builder += "/usr/lib64/crt1.o ";
	//Builder += "/usr/lib64/crti.o ";

	return MakeString(Builder);
}

