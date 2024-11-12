#include <dvm.h>
#include <disc.h>

__attribute__((weak)) unsigned g_dvmDefaultCachePages = 2;
__attribute__((weak)) unsigned g_dvmDefaultSectorsPerPage = 8;

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

	// Try mounting the disc interface
	DISC_INTERFACE* fat = (DISC_INTERFACE*)discGetInterface();
	num_mounted += dvmProbeMountDiscIface("fat", fat, cache_pages, sectors_per_page);

	return num_mounted != 0;
}
