#pragma once


typedef void* DLIB;

DLIB OpenLibrary(const char *Name);
void *GetSymLibrary(DLIB Lib, const char *SymName);

