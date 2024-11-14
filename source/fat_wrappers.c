// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <fat.h>

extern unsigned g_dvmDefaultCachePages, g_dvmDefaultSectorsPerPage;

bool fatInitDefault(void)
{
	return fatInit(g_dvmDefaultCachePages, true);
}

bool fatInit(unsigned cache_pages, bool set_app_cwdir)
{
	if (!dvmRegisterFsDriver(&g_vfatFsDriver)) {
		return false;
	}

	if (!dvmRegisterFsDriver(&g_exfatFsDriver)) {
		return false;
	}

	if (!dvmInit(set_app_cwdir, cache_pages, g_dvmDefaultSectorsPerPage)) {
		return false;
	}

	return true;
}

bool fatMountSimple(const char* name, DISC_INTERFACE* iface)
{
	return fatMount(name, iface, 0, g_dvmDefaultCachePages, g_dvmDefaultSectorsPerPage);
}

bool fatMount(const char* name, DISC_INTERFACE* iface, sec_t start_sector, unsigned cache_pages, unsigned sectors_per_page)
{
	bool rc = false;
	DvmDisc* disc = NULL;

	if (iface) {
		disc = dvmDiscCreate(iface);
	}

	if (disc && cache_pages != 0) {
		disc = dvmDiscCacheCreate(disc, cache_pages, sectors_per_page);
	}

	if (disc) {
		rc = dvmRegisterFsDriver(&g_vfatFsDriver);
	}

	if (rc && disc) {
		rc = dvmMountVolume(name, disc, start_sector, "vfat");
	}

	if (!rc && disc) {
		disc->vt->destroy(disc);
	}

	return rc;
}
