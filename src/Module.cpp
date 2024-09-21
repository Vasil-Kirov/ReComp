#include "Module.h"
#include "Dynamic.h"
#include "Errors.h"
#include "Log.h"
#include "Parser.h"
#include "Semantics.h"

slice<module> CurrentModules = {};

void AddModule(dynamic<module> &Modules, file *File, string Name)
{
	ForArray(Idx, Modules)
	{
		module *M = &Modules.Data[Idx];
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
	Modules.Push(M);
	File->Module = &Modules.Data[Modules.Count-1];
}

slice<import> ResolveImports(slice<needs_resolving_import> ResolveImports, dynamic<module> Modules)
{
	dynamic<import> Imports = {};

	ForArray(Idx, ResolveImports)
	{
		needs_resolving_import ri = ResolveImports[Idx];
		b32 Found = false;
		ForArray(MIdx, Modules)
		{
			module *m = &Modules.Data[MIdx];
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
			RaiseError(*ri.ErrorInfo, "Couldn't find imported module %s", ri.Name.Data);
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

void AnalyzeModuleForRedifinitions(module *m)
{
	ForArray(Idx, m->Globals)
	{
		symbol *s = m->Globals[Idx];
		string Name = *s->Name;
		for(int OtherIdx = Idx+1; OtherIdx < m->Globals.Count; ++OtherIdx)
		{
			symbol *os = m->Globals[OtherIdx];
			if(*os->Name == Name)
			{
				error_info e = *s->Node->ErrorInfo;
				RaiseError(*os->Node->ErrorInfo, "Symbol %s redifines other symbol in file %s at (%d:%d)", 
						os->Name->Data, e.FileName, e.Line, e.Character);
			}
		}
	}
}

u32 AssignIRRegistersForModuleSymbols(dynamic<module> Modules)
{
	u32 Count = 0;
	ForArray(ModuleIdx, Modules)
	{
		module m = Modules[ModuleIdx];
		ForArray(Idx, m.Globals)
		{
			m.Globals[Idx]->IRRegister = Count++;
		}
	}
	return Count;
}

