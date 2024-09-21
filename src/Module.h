#pragma once
#include <VString.h>
#include <Dynamic.h>
struct symbol;
struct checker;
struct file;

struct module
{
	string Name;
	dynamic<symbol *> Globals;
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

extern slice<module> CurrentModules;


