#include <nds/system.h>
#include <nds/memory.h>
#include <nds/arm9/dldi.h>
#include <dvm.h>

__attribute__((weak)) unsigned g_dvmDefaultCachePages = 16;
__attribute__((weak)) unsigned g_dvmDefaultSectorsPerPage = 8;

void _dvmSetAppWorkingDir(const char* argv0);

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

	// Try mounting DLDI
	num_mounted += dvmProbeMountDiscIface("fat", dldiGetInternal(), cache_pages, sectors_per_page);

	// Try mounting DSi SD card
	num_mounted += dvmProbeMountDiscIface("sd", get_io_dsisd(), cache_pages, sectors_per_page);

	// Set current working directory if needed
	if (set_app_cwdir && num_mounted != 0 && __system_argv->argvMagic == ARGV_MAGIC && __system_argv->argc >= 1) {
		_dvmSetAppWorkingDir(__system_argv->argv[0]);
	}

	return num_mounted != 0;
}
