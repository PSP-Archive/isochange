// isochange_v3

// header
#include "isochange.h"

// info
PSP_MODULE_INFO("ISOChange", 0x1007, 0, 3);

// define
#define START_KEY (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START)

// enum
enum ISODriverIndex
{
	NormalDriver = 0,
	ME_NP9660_Driver,
	MEDriver,
	PRO_NP9660_Driver,
	PROInfernoDriver,
	M33MarchDriver,
};

// driver_name_list
const char *iso_driver_list[] =
{
	"M33GalaxyController",
	"umd_driver",
	"PROGalaxyController",
	"PRO_Inferno_Driver",
	"pspMarch33_Driver",
};

// function_ptr
int (*_sceCtrlPeekBufferPositive)(SceCtrlData *pad_data, int count);

// global
int sema_flag;
SceUID sema_ctrl;
char isopath[128];
u32 jump_buffer[2];
int cur_driver_index;

// function
void RebootSystem(void)
{
	int clock;

	// アラームをセット
	sceSysconReadClock(&clock);
	sceSysconWriteAlarm(clock + 5);

	// PSPをスリープさせる
	scePowerRequestSuspend();
	return;
}

void SendDummyLBAParams(void)
{
	char *iso_sector_buf;
	LBAParams dummy_params;

	// ダミーのパラメータを指定する
	dummy_params.unk = 0;
	dummy_params.cmd = 0;
	dummy_params.lba_top = 0x01FFFFFF;
	dummy_params.lba_size = 0;
	dummy_params.byte_size_total = 0x800;
	dummy_params.byte_size_center = 0;
	dummy_params.byte_size_start = 0;
	dummy_params.byte_size_last = 0;

	// ISOドライバーのバッファを取得する
	sceIoDevctl("UMD:", 0x01E28035, NULL, 0, &iso_sector_buf, sizeof(int));

	// ISOドライバーへダミーのパラメータを送り、ISO切り替えを行う
	sceIoDevctl("UMD:", 0x01E380C0, &dummy_params, sizeof(LBAParams), iso_sector_buf, 0x800);
	return;
}

int CheckOEIsofsDriver(void)
{
	u32 lba;
	return (sceIoDevctl("isofs:", 0x01020006, NULL, 0, &lba, sizeof(u32)) == 0) ? 1 : 0;
}

int FindISODriver(void)
{
	int i;

	for(i = 0; i < 5; i++)
	{
		if(sceKernelFindModuleByName(iso_driver_list[i]) != NULL)
			return i + 1;
	}

	return 0;
}

int sceCtrlPeekBufferPositivePatched(SceCtrlData *pad_data, int count)
{
	int res;

	if(!sema_flag)
	{
		// 自分のモジュールにインポートされている関数を使う
		res = sceCtrlPeekBufferPositive(pad_data, count);

		if((pad_data->Buttons & START_KEY) == START_KEY)
		{
			sema_flag = 1;
			sceKernelSignalSema(sema_ctrl, 1);

			pad_data->Buttons &= ~START_KEY;
			return res;
		}
	}

	// Imposeモジュールにインポートされた関数へジャンプする
	// 既にパッチされていた場合にはそのパッチ関数へジャンプする
	return _sceCtrlPeekBufferPositive(pad_data, count);
}

void PatchImposeDriver(void)
{
	u32 orgaddr, nid;

	// NIDResolverで変換されたsceCtrlPeekBufferPositiveのNIDを取得する
	nid = FindResolveNid("sceController_Service", "sceCtrl_driver", 0x3A622550);

	// インポートされた関数のアドレスを取得
	orgaddr = FindImportAddr("sceImpose_Driver", "sceCtrl_driver", nid);

	// 既にパッチされていた場合を考えて、インポートされている関数のジャンプ命令を利用する
	jump_buffer[0] = _lw(orgaddr + 0);
	jump_buffer[1] = _lw(orgaddr + 4);

	// 関数を取得する
	_sceCtrlPeekBufferPositive = (void *)jump_buffer;

	// パッチ
	MAKE_STUB(orgaddr, sceCtrlPeekBufferPositivePatched);

	// キャッシュクリア
	ClearCaches();
	return;
}

int ISOChangeThread(SceSize args, void *argp)
{
	clock_t clock;
	int isofs, noumd;
	char *ptr, number;

	// Imposeモジュールが読み込まれるまで待つ
	while(!sceKernelFindModuleByName("sceImpose_Driver"))
		sceKernelDelayThread(1000);

	// 他のモジュールがImposeモジュールへパッチを当て終えるまで待つ
	clock = sceKernelLibcClock() + 3000000;

	while(clock > sceKernelLibcClock())
		sceKernelDelayThread(50000);

	// Imposeモジュールへパッチを当てる
	PatchImposeDriver();

	// OE_isofsドライバーの検出
	isofs = CheckOEIsofsDriver();
	noumd = isofs;

	// ループ
	while(1)
	{
		sema_flag = 0;

		// 待機状態に入る
		sceKernelWaitSema(sema_ctrl, 1, NULL);

		// '1'と'2'の入れ替え
		ptr = strrchr(isopath, '.');

		if(!ptr)
			break;

		ptr--;
		number = '3' - *ptr;
		*ptr = number + '0';

		// ISOファイルへのパスをセット
		sctrlSESetUmdFile(isopath);

		// ISOの切り替え処理
		switch(cur_driver_index)
		{
			case NormalDriver:
				sctrlSEMountUmdFromFile(isopath, noumd, isofs);
				break;

			case ME_NP9660_Driver:
			case PRO_NP9660_Driver:
			case PROInfernoDriver:
				RebootSystem();
				break;

			case MEDriver:
			case M33MarchDriver:
				SendDummyLBAParams();
				break;
		}

		// 安全のために3秒間待つ
		clock = sceKernelLibcClock() + 3000000;

		while(clock > sceKernelLibcClock())
			sceKernelDelayThread(50000);
	}

	return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp)
{
	SceUID thid;

	// ゲームモードの判定
	if(sceKernelInitKeyConfig() != PSP_INIT_KEYCONFIG_GAME)
		return -1;

	// ゲームを起動しているかの判定
	if(sceKernelBootFrom() != PSP_BOOT_DISC)
		return -1;

	// ISOドライバーを検出
	cur_driver_index = FindISODriver();

	// ISOファイルのパスを取得
	if(sctrlSEGetUmdFile() != NULL)
		strncpy(isopath, sctrlSEGetUmdFile(), sizeof(isopath));

	// メインスレッド生成
	thid = sceKernelCreateThread("ISOChangeThread", ISOChangeThread, 16, 0x1000, 0, NULL);

	if(thid < 0)
		return -1;

	// セマフォ生成
	sema_ctrl = sceKernelCreateSema("ISOChangeController", 0, 0, 1, NULL);

	if(sema_ctrl < 0)
		return -1;

	// メインスレッド起動
	sceKernelStartThread(thid, 0, NULL);
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}

