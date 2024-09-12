#include "DynamicLib.h"
#if _WIN32
#else
#include <dlfcn.h>
#endif


DLIB OpenLibrary(const char *Name)
{
#if _WIN32
#error IMPLEMENT
#else
	return dlopen(Name, RTLD_NOW | RTLD_GLOBAL);
#endif
}

void *GetSymLibrary(DLIB Lib, const char *FnName)
{
#if _WIN32
#error IMPLEMENT
#else
	return dlsym(Lib, FnName);
#endif
}

