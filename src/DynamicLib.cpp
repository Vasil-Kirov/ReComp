#include "DynamicLib.h"
#if _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif


DLIB OpenLibrary(const char *Name)
{
#if _WIN32
	return LoadLibrary(Name);
#else
	return dlopen(Name, RTLD_NOW | RTLD_GLOBAL);
#endif
}

const char *DLGetLastError()
{
#if _WIN32
#error IMPLEMENT
#else
	return dlerror();
#endif
}

void *GetSymLibrary(DLIB Lib, const char *FnName)
{
#if _WIN32
	return (void *)GetProcAddress((HMODULE)Lib, FnName);
#else
	return dlsym(Lib, FnName);
#endif
}

