// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <ext2.h>

bool ext2Mount(const char* name, DISC_INTERFACE* iface, sec_t start_sector, unsigned cache_pages, unsigned sectors_per_page)
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
		rc = dvmRegisterFsDriver(&g_ext2FsDriver);
	}

	if (rc && disc) {
		rc = dvmMountVolume(name, disc, start_sector, "ext2");
	}

	if (!rc && disc) {
		disc->vt->destroy(disc);
	}

	return rc;
}
