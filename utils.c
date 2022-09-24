// isochange_v3

// header
#include "isochange.h"

// function
void ClearCaches(void)
{
	sceKernelIcacheClearAll();
	sceKernelDcacheWritebackAll();
}

u32 FindImportAddr(const char *module_name, const char *library_name, u32 nid)
{
	int i;
	SceModule2 *mod;
	u32 stub_top, stub_end;
	SceLibraryStubTable *stub;

	if(!module_name || !library_name || !nid)
		return 0;

	mod = sceKernelFindModuleByName(module_name);

	if(!mod)
		return 0;

	stub_top = (u32)mod->stub_top;
	stub_end = stub_top + mod->stub_size;

	while(stub_top < stub_end)
	{
		stub = (SceLibraryStubTable *)stub_top;

		if(strcmp(stub->libname, library_name) == 0)
		{
			for(i = 0; i < stub->stubcount; i++)
			{
				if(stub->nidtable[i] == nid)
					return (u32)stub->stubtable + (i << 3);
			}

			break;
		}

		stub_top += (stub->len << 2);
	}

	return 0;
}

u32 FindResolveNid(const char *module_name, const char *library_name, u32 old_nid)
{
	int i, total;
	SceModule2 *mod;
	SceLibraryEntryTable *entry;
	u32 orgaddr, ent_top, ent_end, *vars;

	orgaddr = sctrlHENFindFunction(module_name, library_name, old_nid);

	if(!orgaddr)
		return 0;

	mod = sceKernelFindModuleByName(module_name);

	if(!mod)
		return 0;

	ent_top = (u32)mod->ent_top;
	ent_end = ent_top + mod->ent_size;

	while(ent_top < ent_end)
	{
		entry = (SceLibraryEntryTable *)ent_top;

		if(strcmp(entry->libname, library_name) == 0)
		{
			vars = entry->entrytable;
			total = entry->stubcount + entry->vstubcount;

			if(total > 0)
			{
				for(i = 0; i < total; i++)
				{
					if(vars[i + total] == orgaddr)
					{
						return vars[i];
					}
				}
			}
		}

		ent_top += (entry->len << 2);
	}

	return 0;
}

