#include "Module.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Log.h"
#include "Memory.h"
#include "Parser.h"
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

slice<import> ResolveImports(slice<needs_resolving_import> ResolveImports, dynamic<module*> Modules)
{
	dynamic<import> Imports = {};

	ForArray(Idx, ResolveImports)
	{
		needs_resolving_import ri = ResolveImports[Idx];
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

