#include <stdlib.h>
#include <dvm.h>

#ifndef LIBDVM_CALICO
#define m_ioType ioType
#define m_isInserted isInserted
#define m_readSectors readSectors
#define m_writeSectors writeSectors
#else
#define m_ioType io_type
#define m_isInserted is_inserted
#define m_readSectors read_sectors
#define m_writeSectors write_sectors
#endif

extern unsigned g_dvmDefaultCachePages, g_dvmDefaultSectorsPerPage;

typedef struct DvmDiscWrap {
	DvmDisc base;
	const DISC_INTERFACE* iface;
} DvmDiscWrap;

static void _dvmDiscWrapDelete(DvmDisc* self_)
{
	free(self_);
}

static bool _dvmDiscWrapReadSectors(DvmDisc* self_, void* buffer, uint32_t first_sector, uint32_t num_sectors)
{
	DvmDiscWrap* self = (DvmDiscWrap*)self_;
	return self->iface->m_readSectors(first_sector, num_sectors, buffer);
}

static bool _dvmDiscWrapWriteSectors(DvmDisc* self_, const void* buffer, uint32_t first_sector, uint32_t num_sectors)
{
	DvmDiscWrap* self = (DvmDiscWrap*)self_;
	return self->iface->m_writeSectors(first_sector, num_sectors, buffer);
}

static void _dvmDiscWrapFlush(DvmDisc* self_)
{
}

static const DvmDiscIface s_dvmDiscWrapIface = {
	.delete        = _dvmDiscWrapDelete,
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
	if (!iface || !iface->startup() || !iface->m_isInserted()) {
		return NULL;
	}

	DvmDiscWrap* disc = (DvmDiscWrap*)malloc(sizeof(DvmDiscWrap));
	if (disc) {
		disc->base.vt = &s_dvmDiscWrapIface;
		disc->base.io_type = iface->m_ioType;
		disc->base.features = iface->features;
		disc->base.num_users = 0;
		disc->base.num_sectors = UINT32_MAX;
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
		disc->vt->delete(disc);
	}
}
