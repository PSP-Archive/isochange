	.set noreorder

#include "pspstub.s"

	STUB_START "sceSyscon_driver",0x00010011,0x00020005
	STUB_FUNC  0xC4D66C1D,sceSysconReadClock
	STUB_FUNC  0x6C911742,sceSysconWriteAlarm
	STUB_END

