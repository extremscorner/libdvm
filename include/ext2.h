// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include <dvm.h>

#ifdef __cplusplus
extern "C" {
#endif

// ext2 filesystem driver
extern const DvmFsDriver g_ext2FsDriver;

// Compatibility functions
bool ext2Mount(const char* name, DISC_INTERFACE* iface, sec_t start_sector, unsigned cache_pages, unsigned sectors_per_page);

static inline bool ext2Unmount(const char* name) {
	return dvmUnmountVolume(name);
}

#ifdef __cplusplus
}
#endif
