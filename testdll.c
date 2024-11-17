#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>


__declspec(dllexport)
void alotta_params(int a, int b, int c, int d, int e, int f, int g)
{
	printf(
			"a:%d\n"
			"b:%d\n"
			"c:%d\n"
			"d:%d\n"
			"e:%d\n"
			"f:%d\n"
			"g:%d\n",
			a, b, c, d, e, f, g);
}


