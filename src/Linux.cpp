#include "Basic.h"
#include "Log.h"
#include "Platform.h"
#include "VString.h"
#include <cstdio>
#include <cstdlib>
#include <semaphore.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>

typedef void *(*linux_start_thread)(void*);

t_handle PlatformCreateThread(t_proc Proc, void *PassValue)
{
	pthread_t thread = {};
	pthread_create(&thread, NULL, (linux_start_thread)Proc, PassValue);
	return thread;
}

t_semaphore PlatformCreateSemaphore(uint MaxCount)
{
	sem_t *sem = (sem_t *)malloc(sizeof(sem_t));
	sem_init(sem, 0, 0);
	return sem;
}

void PlatformSleepOnSemaphore(t_semaphore Semaphore)
{
	sem_wait(Semaphore);
}

void PlatformSignalSemaphore(t_semaphore Semaphore)
{
	sem_post(Semaphore);
}

b32 PlatformDeleteFile(const char *Path)
{
	return unlink(Path) == 0;
}

void PlatformWriteFile(const char *Path, u8 *Data, u32 Size)
{
	int fd = open(Path, O_TRUNC | O_WRONLY | O_CREAT, 0666);
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
		return string {};

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
	if((int)Level == -1)
	{
		write(2, String.Data, String.Size);
		return;
	}
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
	const char CLEAR[] = "\u001b[0m";
	write(out_fd, Colors[Level], sizeof(Colors[Level])-1);
	write(out_fd, String.Data, String.Size);
	write(out_fd, CLEAR, sizeof(CLEAR)-1);
}

void PlatformFreeMemory(void *Mem, size_t Size)
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

