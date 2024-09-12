#pragma once


#if _WIN32
typedef HMODULE DLIB;
#else
typedef void* DLIB;
#endif

DLIB OpenLibrary(const char *Name);
void *GetSymLibrary(DLIB Lib, const char *SymName);

