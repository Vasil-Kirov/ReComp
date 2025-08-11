#pragma once
#include "Dict.h"
#include <VString.h>
#include <Dynamic.h>
struct symbol;
struct checker;
struct file;

struct needs_resolving_import
{
	string FileName;
	string RelativePath;
	string Name;
	string As;
	struct error_info *ErrorInfo;
};

struct module
{
	string Name;
	dict<symbol *> Globals;
	dynamic<file *> Files;
};

struct import
{
	module *M;
	string As;
};

struct checker;
struct file
{
	token *Tokens;
	dynamic<node *>Nodes;
	ir *IR;
	checker *Checker;
	module *Module;
	slice<import> Imported;
	string Name;
};

b32 FindImportedModule(slice<import> Imports, string &ModuleName, import *Out);
int GetFileIndex(module *m, file *f);
void AddModule(dynamic<module*> &Modules, file *File, string Name);
u32 AssignIRRegistersForModuleSymbols(dynamic<module*> Modules);
slice<import> ResolveImports(slice<needs_resolving_import> ResolveImports, dynamic<module*> Modules, slice<file*> Files);

extern slice<module*> CurrentModules;


