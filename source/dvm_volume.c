// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dvm.h>
#include "dvm_debug.h"

#define MAX_DRIVERS 8

static const DvmFsDriver* s_dvmFsDrvTable[MAX_DRIVERS];

typedef struct DvmVolume {
	devoptab_t dotab;
	const DvmFsDriver* fsdrv;
	char namebuf[32];

	alignas(2*sizeof(void*)) uint8_t device_data[];
} DvmVolume;

bool dvmRegisterFsDriver(const DvmFsDriver* fsdrv)
{
	if (!fsdrv) {
		return false;
	}

	for (unsigned i = 0; i < MAX_DRIVERS; i ++) {
		if (s_dvmFsDrvTable[i] == fsdrv) {
			return true;
		} else if (!s_dvmFsDrvTable[i]) {
			s_dvmFsDrvTable[i] = fsdrv;
			return true;
		}
	}

	return false;
}

bool dvmMountVolume(const char* name, DvmDisc* disc, sec_t start_sector, const char* fstype)
{
	const DvmFsDriver* fsdrv = NULL;
	for (unsigned i = 0; i < MAX_DRIVERS && s_dvmFsDrvTable[i]; i ++) {
		if (strcmp(s_dvmFsDrvTable[i]->fstype, fstype) == 0) {
			fsdrv = s_dvmFsDrvTable[i];
			break;
		}
	}

	if (!fsdrv) {
		return false;
	}

	size_t vol_sz = sizeof(DvmVolume) + fsdrv->device_data_sz;
	DvmVolume* vol = (DvmVolume*)malloc(vol_sz);
	if (!vol) {
		return false;
	}

	memset(vol, 0, vol_sz);
	memcpy(&vol->dotab, fsdrv->dotab_template, sizeof(vol->dotab));
	vol->dotab.name = vol->namebuf;
	vol->dotab.deviceData = vol->device_data;
	vol->fsdrv = fsdrv;
	memcpy(vol->namebuf, name, strnlen(name, sizeof(vol->namebuf)));

	if (!fsdrv->mount(&vol->dotab, disc, start_sector)) {
		free(vol);
		return false;
	}

	int devid = AddDevice(&vol->dotab);
	if (devid < 0) {
		fsdrv->umount(vol->device_data);
		free(vol);
		return false;
	}

	const devoptab_t* default_dev = GetDeviceOpTab("");
	if (!default_dev || strcmp(default_dev->name, "stdnull") == 0) {
		dvmDebug("Default dev %s (%d)\n", vol->dotab.name, devid);

		char cwd[32+3];
		unsigned pos = strnlen(vol->dotab.name, 32);
		memcpy(cwd, vol->dotab.name, pos);
		cwd[pos+0] = ':';
		cwd[pos+1] = '/';
		cwd[pos+2] = 0;
		chdir(cwd);
	} else {
		dvmDebug("Added dev %s (%d)\n", vol->dotab.name, devid);
	}

	return true;
}

static bool _dvmIsVolume(const devoptab_t* dotab)
{
	// Calculate expected addresses of members
	const char* expected_name = (char*)dotab + offsetof(DvmVolume, namebuf);
	void*    expected_devdata = (char*)dotab + offsetof(DvmVolume, device_data);
	return dotab->name == expected_name && dotab->deviceData == expected_devdata;
}

void dvmUnmountVolume(const char* name)
{
	/*
	char namebuf[32];
	if (!strchr(name, ':')) {
		size_t namelen = strnlen(name, sizeof(namebuf)-2);
		memcpy(namebuf, name, namelen);
		namebuf[namelen] = ':';
		namebuf[namelen+1] = 0;
		name = namebuf;
	}
	*/

	const devoptab_t* dotab = GetDeviceOpTab(name);
	if (!dotab || !_dvmIsVolume(dotab)) {
		return;
	}

	DvmVolume* vol = (DvmVolume*)dotab;
	RemoveDevice(name);
	vol->fsdrv->umount(vol->device_data);
	free(vol);
}

void _dvmSetAppWorkingDir(const char* argv0)
{
	char cwd[PATH_MAX];

	const char* last_slash = strrchr(argv0, '/');
	if (last_slash) {
		size_t cwd_len = last_slash - argv0;
		if ((cwd_len + 2) <= sizeof(cwd)) {
			memcpy(cwd, argv0, cwd_len+1); // including slash
			cwd[cwd_len+1] = 0;
			chdir(cwd);
		}
	}
}
