// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include <stdlib.h>
#include <sys/lock.h>
#include <sys/iosupport.h>
#include <ff.h>
#include <diskio.h>
#include "fat.h"

typedef struct FatVolume {
	_LOCK_T lock;
	DvmDisc* disc;
	sec_t start_sector;

	FATFS fs;
} FatVolume;

static inline FatVolume* _fatVolumeFromFatFs(FATFS* fs)
{
	return (FatVolume*)fs->pdrv;
}

static inline DvmDisc* _fatDiscFromFatFs(FATFS* fs)
{
	return _fatVolumeFromFatFs(fs)->disc;
}
