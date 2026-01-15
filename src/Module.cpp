#include "Module.h"
#include "CommandLine.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Globals.h"
#include "Log.h"
#include "Memory.h"
#include "Pipeline.h"
#include "Semantics.h"

slice<module*> CurrentModules = {};

void AddModule(dynamic<module*> &Modules, file *File, string Name)
{
	ForArray(Idx, Modules)
	{
		module *M = Modules[Idx];
		if(M->Name == Name)
		{
			M->Files.Push(File);
			File->Module = M;
			return;
		}
	}

	module M = {};
	M.Name = Name;
	M.Files.Push(File);
	Modules.Push(DupeType(M, module));
	File->Module = Modules[Modules.Count-1];
}

slice<import> ResolveImports(slice<needs_resolving_import> ResolveImports, dynamic<module*> Modules, slice<file*> Files)
{
	dynamic<import> Imports = {};

	ForArray(Idx, ResolveImports)
	{
		needs_resolving_import ri = ResolveImports[Idx];
		if(ri.FileName.Size != 0)
		{
			string Path = FindFile(ri.FileName, ri.RelativePath);
			b32 Found = false;
			For(Files)
			{
				if((*it)->Name == Path)
				{
					Found = true;
					import NewImport = {
						.M = (*it)->Module,
						.As = ri.As
					};
					Imports.Push(NewImport);
					break;
				}
			}
			if(!Found)
			{
				RaiseError(true, *ri.ErrorInfo, "Couldn't find imported file %.*s",
						ri.FileName.Size, ri.FileName.Data);
			}
		}
		else
		{
			b32 Found = false;
			ForArray(MIdx, Modules)
			{
				module *m = Modules[MIdx];
				if(m->Name == ri.Name)
				{
					Found = true;
					import NewImport = {
						.M = m,
						.As = ri.As
					};
					Imports.Push(NewImport);
					break;
				}
			}
			if(!Found)
			{
				RaiseError(true, *ri.ErrorInfo, "Couldn't find imported module %s", ri.Name.Data);
			}
		}
	}


	string Internal = STR_LIT("internal");
	string Base = STR_LIT("base");
	b32 HasInternal = false;
	b32 HasBase = false;
	For(Imports)
	{
		if(it->M->Name == Internal)
		{
			HasInternal = true;
		}
		else if(it->M->Name == Base)
		{
			HasBase = true;
		}
	}

	if(!HasInternal)
	{
		module *InternalMod = NULL;
		For(Modules)
		{
			if((*it)->Name == Internal)
			{
				InternalMod = (*it);
				break;
			}
		}
		if(InternalMod == NULL)
		{
			LogCompilerError("Error: internal module is not found, if you replaced it in CompileInfo, make sure the module name is `internal`\n");
			exit(1);
		}
		Imports.Push(import{.M = InternalMod, .As = STR_LIT("")});
	}
	if(!HasBase)
	{
		module *BaseMod = NULL;
		For(Modules)
		{
			if((*it)->Name == Base)
			{
				BaseMod = (*it);
				break;
			}
		}
		if(BaseMod == NULL)
		{
			LogCompilerError("Error: base module is missing\n");
			exit(1);
		}
		Imports.Push(import{.M = BaseMod, .As = STR_LIT("*")});
	}

	return SliceFromArray(Imports);
}

b32 FindImportedModule(slice<import> Imports, string &ModuleName, import *Out)
{
	ForArray(Idx, Imports)
	{
		import Imported = Imports[Idx];
		if(Imported.M->Name == ModuleName
				|| Imported.As == ModuleName)
		{
			*Out = Imported;
			return true;
		}
	}
	return false;
}

int GetFileIndex(module *m, file *f)
{
	ForArray(Idx, m->Files)
	{
		file *mf = m->Files[Idx];
		if(mf->Name == f->Name)
			return Idx;
	}
	unreachable;
}

u32 AssignIRRegistersForModuleSymbols(dynamic<module*> Modules)
{
	u32 Count = 0;
	ForArray(ModuleIdx, Modules)
	{
		module *m = Modules[ModuleIdx];
		ForArray(Idx, m->Globals.Data)
		{
			m->Globals.Data[Idx]->Register = Count++;
		}
	}
	return Count;
}

void CheckInternalModule(module *Module)
{
	slice<string> InternalFns = SliceFromConst({
		STR_LIT("advance"),
		STR_LIT("deref"),
		STR_LIT("stdout"),
		STR_LIT("write"),
		STR_LIT("abort"),
	});

	bool Abort = false;
	For(InternalFns)
	{
		if(!Module->Globals.Contains(*it))
		{
			LogCompilerError("Error: Internals file does not contain a definition for %.*s.\n", (int)it->Size, it->Data);
			Abort = true;
		}
	}
	if(Abort)
		exit(1);
	
}

