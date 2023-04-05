#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
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

bool dvmMountVolume(const char* name, DvmDisc* disc, uint32_t start_sector, const char* fstype)
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
		setDefaultDevice(devid);
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
