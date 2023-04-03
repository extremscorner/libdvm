#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include <sdcard/gcsd.h>
#include <dvm.h>

__attribute__((weak)) unsigned g_dvmDefaultCachePages = 4;
__attribute__((weak)) unsigned g_dvmDefaultSectorsPerPage = 64;

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

#if defined(__wii__)
	const DISC_INTERFACE* sd    = &__io_wiisd;
	const DISC_INTERFACE* usb   = &__io_usbstorage;
#elif defined(__gamecube__)
	const DISC_INTERFACE* sd    = &__io_gcsd2;
#else
#error "Neither Wii nor GameCube"
#endif
	const DISC_INTERFACE* carda = &__io_gcsda;
	const DISC_INTERFACE* cardb = &__io_gcsdb;

	// Try mounting SD card
	if (dvmProbeDiscIface("sd", sd, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}

#if defined(__wii__)
	// Try mounting the first found USB drive
	if (dvmProbeDiscIface("usb", usb, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}
#endif

	// Try mounting Card A
	if (dvmProbeDiscIface("carda", carda, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}

	// Try mounting Card B
	if (dvmProbeDiscIface("cardb", cardb, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}

	if (num_mounted && set_app_cwdir) {
		// TODO
	}

	return num_mounted != 0;
}
