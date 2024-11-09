// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/iosupport.h>

#if defined(__gamecube__) || defined(__wii__)
# include <ogc/disc_io.h>
#elif (defined(NDS) || defined(__NDS__)) && defined(ARM9)
# if __has_include(<calico/dev/disc_io.h>)
#  include <calico/dev/disc_io.h>
# elif __has_include(<nds/disc_io.h>)
#  include <nds/disc_io.h>
# else
#  error "Missing DS platform library"
# endif
#elif __has_include(<disc_io.h>)
# include <disc_io.h>
#else
# error "Unsupported platform"
#endif

#define DVM_IDENT_FSTYPE (1U<<0)

typedef struct DvmDisc DvmDisc;
typedef struct DvmDiscIface DvmDiscIface;
typedef struct DvmFsDriver DvmFsDriver;
typedef struct DvmPartInfo DvmPartInfo;

struct DvmDisc {
	const DvmDiscIface* vt;
	uint32_t io_type;
	uint16_t features;
	uint16_t num_users;
	sec_t num_sectors;
};

struct DvmDiscIface {
	void (*destroy)(DvmDisc* self);
	bool (*read_sectors)(DvmDisc* self, void* buffer, sec_t sectors, sec_t num_sectors);
	bool (*write_sectors)(DvmDisc* self, const void* buffer, sec_t sectors, sec_t num_sectors);
	void (*flush)(DvmDisc* self);
};

struct DvmFsDriver {
	const char* fstype;
	const devoptab_t* dotab_template;
	size_t device_data_sz;

	bool (*mount)(devoptab_t* dotab, DvmDisc* disc, sec_t start_sector);
	void (*umount)(void* device_data);
};

struct DvmPartInfo {
	uint16_t index;
	uint16_t type;
	const char* fstype;
	sec_t start_sector;
	sec_t num_sectors;
};

#ifdef __cplusplus
extern "C" {
#endif

// Initialization
bool dvmInitDefault(void);
bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page);

// Disc and cache management
DvmDisc* dvmDiscCreate(const DISC_INTERFACE* iface);
DvmDisc* dvmDiscCacheCreate(DvmDisc* inner_disc, unsigned cache_pages, unsigned sectors_per_page);
void dvmDiscAddUser(DvmDisc* disc);
void dvmDiscRemoveUser(DvmDisc* disc);

static inline bool dvmDiscReadSectors(DvmDisc* disc, void* buffer, sec_t sectors, sec_t num_sectors)
{
	return disc->vt->read_sectors(disc, buffer, sectors, num_sectors);
}

static inline bool dvmDiscWriteSectors(DvmDisc* disc, const void* buffer, sec_t sectors, sec_t num_sectors)
{
	return disc->vt->write_sectors(disc, buffer, sectors, num_sectors);
}

static inline void dvmDiscFlush(DvmDisc* disc)
{
	disc->vt->flush(disc);
}

// Volume management
bool dvmRegisterFsDriver(const DvmFsDriver* fsdrv);
bool dvmMountVolume(const char* name, DvmDisc* disc, sec_t start_sector, const char* fstype);
void dvmUnmountVolume(const char* name);

// Partition table and filesystem probing
unsigned dvmReadPartitionTable(DvmDisc* disc, DvmPartInfo* out, unsigned max_partitions, unsigned flags);
unsigned dvmProbeMountDisc(const char* basename, DvmDisc* disc);
unsigned dvmProbeMountDiscIface(const char* basename, const DISC_INTERFACE* iface, unsigned cache_pages, unsigned sectors_per_page);

#ifdef __cplusplus
}
#endif
