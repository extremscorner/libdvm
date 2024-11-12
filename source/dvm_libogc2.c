// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <ogc/dvd.h>
#include <ogc/system.h>
#include <ogc/usbstorage.h>
#include <sdcard/gcsd.h>
#include <sdcard/wiisd_io.h>
#include <dvm.h>

__attribute__((weak)) unsigned g_dvmDefaultCachePages = 4;
__attribute__((weak)) unsigned g_dvmDefaultSectorsPerPage = 64;

void _dvmSetAppWorkingDir(const char* argv0);

bool _dvmIsAlignedAccess(const void* ptr, bool is_write)
{
	return SYS_IsDMAAddress(ptr);
}

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

#if defined(__wii__)
	DISC_INTERFACE* sd    = &__io_wiisd;
	DISC_INTERFACE* usb   = &__io_usbstorage;
#elif defined(__gamecube__)
	DISC_INTERFACE* sd    = &__io_gcsd2;
	DISC_INTERFACE* dvd   = &__io_gcode;
#else
#error "Neither Wii nor GameCube"
#endif
	DISC_INTERFACE* carda = &__io_gcsda;
	DISC_INTERFACE* cardb = &__io_gcsdb;

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
#endif

	// Set current working directory if needed
	if (set_app_cwdir && num_mounted != 0 && __system_argv->argc >= 1) {
		_dvmSetAppWorkingDir(__system_argv->argv[0]);
	}

	return num_mounted != 0;
}
