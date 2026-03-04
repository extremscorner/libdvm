// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include <stdlib.h>
#include <sys/lock.h>
#include <sys/iosupport.h>
#include <ext4.h>
#include <ext4_inode.h>
#include <ext4_super.h>
#include "ext2.h"

typedef struct FatVolume {
	_LOCK_T lock;
	DvmDisc* disc;

	struct ext4_lock locks;
	struct ext4_blockdev_iface bdif;
	struct ext4_blockdev bdev;
	struct ext4_mountpoint mp;
} Ext4Volume;
