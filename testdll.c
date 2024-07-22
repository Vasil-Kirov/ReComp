#include <stdint.h>
#include <stdio.h>


__declspec(dllexport)
char *read_stdin_line(char *buff, int32_t max, FILE *stream)
{
	//printf("GOT:\nbuff: 0x%llx\nMax: %d\nstream: 0x%llx\n", (uint64_t)buff, max, (uint64_t)stream);
	return fgets(buff, max, stream);
}

