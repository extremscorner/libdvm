// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/aram.h>
#include <ogc/dvd.h>
#include <ogc/system.h>
#include <ogc/usbstorage.h>
#include <sdcard/gcsd.h>
#include <sdcard/wiisd_io.h>
#include <dvm.h>

__attribute__((weak)) unsigned g_dvmDefaultCachePages = 32;
__attribute__((weak)) unsigned g_dvmDefaultSectorsPerPage = 8;

void _dvmSetAppWorkingDir(const char* argv0);

bool _dvmIsAlignedAccess(const void* ptr, bool is_write)
{
	return SYS_IsDMAAddress(ptr, LIBDVM_BUFFER_ALIGN);
}

static s32 _dvmOnReset(s32 final)
{
	if (!final) {
		dvmDeinit();
	}

	return true;
}

static sys_resetinfo s_dvmResetInfo = {
	.func = _dvmOnReset,
	.prio = 0,
};

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

	DISC_INTERFACE* carda = get_io_gcsda();
	DISC_INTERFACE* cardb = get_io_gcsdb();
#if defined(__wii__)
	DISC_INTERFACE* sd    = &__io_wiisd;
	DISC_INTERFACE* usb   = &__io_usbstorage;
#elif defined(__gamecube__)
	DISC_INTERFACE* sd    = get_io_gcsd2();
	DISC_INTERFACE* dvd   = &__io_gcode;
	DISC_INTERFACE* ram   = &__io_aram;
#else
#error "Neither Wii nor GameCube"
#endif

#if defined(__wii__)
	// Try mounting SD card
	num_mounted += dvmProbeMountDiscIface("sd", sd, cache_pages, sectors_per_page);

	// Try mounting the first found USB drive
	num_mounted += dvmProbeMountDiscIface("usb", usb, cache_pages, sectors_per_page);
#endif

	// Try mounting Memory Slot A / Serial Port 1
	num_mounted += dvmProbeMountDiscIface("carda", carda, cache_pages, sectors_per_page);

	// Try mounting Memory Slot B
	num_mounted += dvmProbeMountDiscIface("cardb", cardb, cache_pages, sectors_per_page);

#if defined(__gamecube__)
	// Try mounting Serial Port 2
	num_mounted += dvmProbeMountDiscIface("sd", sd, cache_pages, sectors_per_page);

	// Try mounting GC Loader
	num_mounted += dvmProbeMountDiscIface("dvd", dvd, cache_pages, sectors_per_page);

	// Try mounting Memory Expansion Pak
	num_mounted += dvmProbeMountDiscIface("ram", ram, cache_pages, sectors_per_page);
#endif

	// Set current working directory if needed
	if (set_app_cwdir && num_mounted != 0) {
		const char* pwd = getenv("PWD");
		if (pwd) {
			chdir(pwd);
		}

		if (__system_argv->argc >= 1) {
			_dvmSetAppWorkingDir(__system_argv->argv[0]);
		}
	}

	// Register reset function
	if (num_mounted != 0) {
		SYS_RegisterResetFunc(&s_dvmResetInfo);
	}

	return num_mounted != 0;
}

void dvmDeinit(void)
{
	for (int fd = 3; fd < 1024; fd ++) {
		fsync(fd);
	}

	for (unsigned i = 3; i < STD_MAX; i ++) {
		const devoptab_t* dotab = devoptab_list[i];
		if (dotab && strcmp(dotab->name, "stdnull") != 0) {
			dvmUnmountVolume(dotab->name);
		}
	}

	SYS_UnregisterResetFunc(&s_dvmResetInfo);
}
