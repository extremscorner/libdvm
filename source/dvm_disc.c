#include <stdlib.h>
#include <dvm.h>

extern unsigned g_dvmDefaultCachePages, g_dvmDefaultSectorsPerPage;

typedef struct DvmDiscWrap {
	DvmDisc base;
	const DISC_INTERFACE* iface;
} DvmDiscWrap;

static void _dvmDiscWrapDestroy(DvmDisc* self_)
{
	free(self_);
}

static bool _dvmDiscWrapReadSectors(DvmDisc* self_, void* buffer, sec_t first_sector, sec_t num_sectors)
{
	DvmDiscWrap* self = (DvmDiscWrap*)self_;
	return self->iface->readSectors(first_sector, num_sectors, buffer);
}

static bool _dvmDiscWrapWriteSectors(DvmDisc* self_, const void* buffer, sec_t first_sector, sec_t num_sectors)
{
	DvmDiscWrap* self = (DvmDiscWrap*)self_;
	return self->iface->writeSectors(first_sector, num_sectors, buffer);
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

DvmDisc* dvmDiscCreate(const DISC_INTERFACE* iface)
{
	if (!iface || !iface->startup() || !iface->isInserted()) {
		return NULL;
	}

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
