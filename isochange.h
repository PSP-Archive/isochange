#ifndef __ISOCHANGE_H__
#define __ISOCHANGE_H__

// header
#include <pspkernel.h>
#include <systemctrl.h>
#include <systemctrl_se.h>
#include <psppower.h>
#include <pspctrl.h>
#include <pspinit.h>
#include <string.h>

// define
#define MAKE_STUB(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a); _sw(0, a + 4);

// struct
typedef struct
{
	int unk;
	int cmd;
	int lba_top;
	int lba_size;
	int byte_size_total;
	int byte_size_center;
	int byte_size_start;
	int byte_size_last;
} LBAParams;

// stub.S(sceSyscon_driver)
int sceSysconReadClock(int *clock);
void sceSysconWriteAlarm(int alarm);

// utils.c
void ClearCaches(void);
u32 FindImportAddr(const char *module_name, const char *library_name, u32 nid);
u32 FindResolveNid(const char *module_name, const char *library_name, u32 old_nid);

#endif

