#include <nds/system.h>
#include <nds/memory.h>
#include <nds/arm9/dldi.h>
#include <dvm.h>

__attribute__((weak)) unsigned g_dvmDefaultCachePages = 16;
__attribute__((weak)) unsigned g_dvmDefaultSectorsPerPage = 8;

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

	// Try mounting DLDI
	const DISC_INTERFACE* dldi = dldiGetInternal();
	if (dldi && dvmProbeDiscIface("fat", dldi, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}

	// Try mounting DSi SD card
	const DISC_INTERFACE* sd = get_io_dsisd();
	if (sd && dvmProbeDiscIface("sd", sd, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}

	// Set current working directory if needed
	if (set_app_cwdir && num_mounted != 0) {
		// XX
	}

	return num_mounted != 0;
}
