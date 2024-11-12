// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdlib.h>
#include <dvm.h>

extern unsigned g_dvmDefaultCachePages, g_dvmDefaultSectorsPerPage;

typedef struct DvmDiscWrap {
	DvmDisc base;
	DISC_INTERFACE* iface;
} DvmDiscWrap;

static void _dvmDiscWrapDestroy(DvmDisc* self_)
{
	DvmDiscWrap* self = (DvmDiscWrap*)self_;
#if defined(__gamecube__) || defined(__wii__)
	self->iface->shutdown(self->iface);
#else
	self->iface->shutdown();
#endif
	free(self);
}

static bool _dvmDiscWrapReadSectors(DvmDisc* self_, void* buffer, sec_t first_sector, sec_t num_sectors)
{
	DvmDiscWrap* self = (DvmDiscWrap*)self_;
#if defined(__gamecube__) || defined(__wii__)
	return self->iface->readSectors(self->iface, first_sector, num_sectors, buffer);
#else
	return self->iface->readSectors(first_sector, num_sectors, buffer);
#endif
}

static bool _dvmDiscWrapWriteSectors(DvmDisc* self_, const void* buffer, sec_t first_sector, sec_t num_sectors)
{
	DvmDiscWrap* self = (DvmDiscWrap*)self_;
#if defined(__gamecube__) || defined(__wii__)
	return self->iface->writeSectors(self->iface, first_sector, num_sectors, buffer);
#else
	return self->iface->writeSectors(first_sector, num_sectors, buffer);
#endif
}

static void _dvmDiscWrapFlush(DvmDisc* self_)
{
}

static const DvmDiscIface s_dvmDiscWrapIface = {
	.destroy       = _dvmDiscWrapDestroy,
	.read_sectors  = _dvmDiscWrapReadSectors,
	.write_sectors = _dvmDiscWrapWriteSectors,
	.flush         = _dvmDiscWrapFlush,
};

bool dvmInitDefault(void)
{
	return dvmInit(true, g_dvmDefaultCachePages, g_dvmDefaultSectorsPerPage);
}

DvmDisc* dvmDiscCreate(DISC_INTERFACE* iface)
{
#if defined(__gamecube__) || defined(__wii__)
	if (!iface || !iface->startup(iface) || !iface->isInserted(iface)) {
		return NULL;
	}
#else
	if (!iface || !iface->startup() || !iface->isInserted()) {
		return NULL;
	}
#endif

	DvmDiscWrap* disc = (DvmDiscWrap*)malloc(sizeof(DvmDiscWrap));
	if (disc) {
		disc->base.vt = &s_dvmDiscWrapIface;
		disc->base.io_type = iface->ioType;
		disc->base.features = iface->features;
		disc->base.num_users = 0;
		disc->base.num_sectors = ~(sec_t)0;
		disc->iface = iface;
	}

	return &disc->base;
}

void dvmDiscAddUser(DvmDisc* disc)
{
	// XX: Consider using atomics if available
	disc->num_users ++;
}

void dvmDiscRemoveUser(DvmDisc* disc)
{
	// XX: Consider using atomics if available
	if (!--disc->num_users) {
		disc->vt->destroy(disc);
	}
}
