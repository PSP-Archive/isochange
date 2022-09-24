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

	// �A���[�����Z�b�g
	sceSysconReadClock(&clock);
	sceSysconWriteAlarm(clock + 5);

	// PSP���X���[�v������
	scePowerRequestSuspend();
	return;
}

void SendDummyLBAParams(void)
{
	char *iso_sector_buf;
	LBAParams dummy_params;

	// �_�~�[�̃p�����[�^���w�肷��
	dummy_params.unk = 0;
	dummy_params.cmd = 0;
	dummy_params.lba_top = 0x01FFFFFF;
	dummy_params.lba_size = 0;
	dummy_params.byte_size_total = 0x800;
	dummy_params.byte_size_center = 0;
	dummy_params.byte_size_start = 0;
	dummy_params.byte_size_last = 0;

	// ISO�h���C�o�[�̃o�b�t�@���擾����
	sceIoDevctl("UMD:", 0x01E28035, NULL, 0, &iso_sector_buf, sizeof(int));

	// ISO�h���C�o�[�փ_�~�[�̃p�����[�^�𑗂�AISO�؂�ւ����s��
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
		// �����̃��W���[���ɃC���|�[�g����Ă���֐����g��
		res = sceCtrlPeekBufferPositive(pad_data, count);

		if((pad_data->Buttons & START_KEY) == START_KEY)
		{
			sema_flag = 1;
			sceKernelSignalSema(sema_ctrl, 1);

			pad_data->Buttons &= ~START_KEY;
			return res;
		}
	}

	// Impose���W���[���ɃC���|�[�g���ꂽ�֐��փW�����v����
	// ���Ƀp�b�`����Ă����ꍇ�ɂ͂��̃p�b�`�֐��փW�����v����
	return _sceCtrlPeekBufferPositive(pad_data, count);
}

void PatchImposeDriver(void)
{
	u32 orgaddr, nid;

	// NIDResolver�ŕϊ����ꂽsceCtrlPeekBufferPositive��NID���擾����
	nid = FindResolveNid("sceController_Service", "sceCtrl_driver", 0x3A622550);

	// �C���|�[�g���ꂽ�֐��̃A�h���X���擾
	orgaddr = FindImportAddr("sceImpose_Driver", "sceCtrl_driver", nid);

	// ���Ƀp�b�`����Ă����ꍇ���l���āA�C���|�[�g����Ă���֐��̃W�����v���߂𗘗p����
	jump_buffer[0] = _lw(orgaddr + 0);
	jump_buffer[1] = _lw(orgaddr + 4);

	// �֐����擾����
	_sceCtrlPeekBufferPositive = (void *)jump_buffer;

	// �p�b�`
	MAKE_STUB(orgaddr, sceCtrlPeekBufferPositivePatched);

	// �L���b�V���N���A
	ClearCaches();
	return;
}

int ISOChangeThread(SceSize args, void *argp)
{
	clock_t clock;
	int isofs, noumd;
	char *ptr, number;

	// Impose���W���[�����ǂݍ��܂��܂ő҂�
	while(!sceKernelFindModuleByName("sceImpose_Driver"))
		sceKernelDelayThread(1000);

	// ���̃��W���[����Impose���W���[���փp�b�`�𓖂ďI����܂ő҂�
	clock = sceKernelLibcClock() + 3000000;

	while(clock > sceKernelLibcClock())
		sceKernelDelayThread(50000);

	// Impose���W���[���փp�b�`�𓖂Ă�
	PatchImposeDriver();

	// OE_isofs�h���C�o�[�̌��o
	isofs = CheckOEIsofsDriver();
	noumd = isofs;

	// ���[�v
	while(1)
	{
		sema_flag = 0;

		// �ҋ@��Ԃɓ���
		sceKernelWaitSema(sema_ctrl, 1, NULL);

		// '1'��'2'�̓���ւ�
		ptr = strrchr(isopath, '.');

		if(!ptr)
			break;

		ptr--;
		number = '3' - *ptr;
		*ptr = number + '0';

		// ISO�t�@�C���ւ̃p�X���Z�b�g
		sctrlSESetUmdFile(isopath);

		// ISO�̐؂�ւ�����
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

		// ���S�̂��߂�3�b�ԑ҂�
		clock = sceKernelLibcClock() + 3000000;

		while(clock > sceKernelLibcClock())
			sceKernelDelayThread(50000);
	}

	return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp)
{
	SceUID thid;

	// �Q�[�����[�h�̔���
	if(sceKernelInitKeyConfig() != PSP_INIT_KEYCONFIG_GAME)
		return -1;

	// �Q�[�����N�����Ă��邩�̔���
	if(sceKernelBootFrom() != PSP_BOOT_DISC)
		return -1;

	// ISO�h���C�o�[�����o
	cur_driver_index = FindISODriver();

	// ISO�t�@�C���̃p�X���擾
	if(sctrlSEGetUmdFile() != NULL)
		strncpy(isopath, sctrlSEGetUmdFile(), sizeof(isopath));

	// ���C���X���b�h����
	thid = sceKernelCreateThread("ISOChangeThread", ISOChangeThread, 16, 0x1000, 0, NULL);

	if(thid < 0)
		return -1;

	// �Z�}�t�H����
	sema_ctrl = sceKernelCreateSema("ISOChangeController", 0, 0, 1, NULL);

	if(sema_ctrl < 0)
		return -1;

	// ���C���X���b�h�N��
	sceKernelStartThread(thid, 0, NULL);
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}

