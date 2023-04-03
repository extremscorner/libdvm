#include <calico.h>
#include <dvm.h>

MEOW_WEAK unsigned g_dvmDefaultCachePages = 16;
MEOW_WEAK unsigned g_dvmDefaultSectorsPerPage = 8;

static void _dvmDiscCalicoDummy(DvmDisc* self_)
{
	// Nothing
}

static bool _dvmDiscCalicoReadSectors(DvmDisc* self, void* buffer, uint32_t first_sector, uint32_t num_sectors)
{
	BlkDevice dev = (BlkDevice)self->io_type;
	return blkDevReadSectors(dev, buffer, first_sector, num_sectors);
}

static bool _dvmDiscCalicoWriteSectors(DvmDisc* self, const void* buffer, uint32_t first_sector, uint32_t num_sectors)
{
	BlkDevice dev = (BlkDevice)self->io_type;
	return blkDevWriteSectors(dev, buffer, first_sector, num_sectors);
}

static const DvmDiscIface s_dvmDiscCalicoIface = {
	.delete        = _dvmDiscCalicoDummy,
	.read_sectors  = _dvmDiscCalicoReadSectors,
	.write_sectors = _dvmDiscCalicoWriteSectors,
	.flush         = _dvmDiscCalicoDummy,
};

static DvmDisc s_dvmDiscDldi = {
	.vt       = &s_dvmDiscCalicoIface,
	.io_type  = BlkDevice_Dldi,
};

static DvmDisc s_dvmDiscSd = {
	.vt       = &s_dvmDiscCalicoIface,
	.io_type  = BlkDevice_TwlSdCard,
	.features = FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
};

static bool _dvmMountCalicoDisc(const char* name, DvmDisc* disc, unsigned cache_pages, unsigned sectors_per_page)
{
	BlkDevice dev = (BlkDevice)disc->io_type;

	// Ensure the disc is initialized and present
	if (!blkDevInit(dev) || !blkDevIsPresent(dev)) {
		return false;
	}

	// Populate DLDI features if needed
	if (dev == BlkDevice_Dldi) {
		disc->features = g_envExtraInfo->dldi_features;
	}

	// Populate disc size
	disc->num_sectors = blkDevGetSectorCount(dev);

	// Create disc cache
	disc = dvmDiscCacheCreate(disc, cache_pages, sectors_per_page);

	// Mount disc partitions
	bool rc = dvmProbeDisc(name, disc);
	if (!rc) {
		disc->vt->delete(disc);
	}

	return rc;
}

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

	// Initialize block device subsystem
	blkInit();

	// Try mounting DLDI
	if (_dvmMountCalicoDisc("fat", &s_dvmDiscDldi, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}

	// Try mounting DSi SD card
	if (systemIsTwlMode() && _dvmMountCalicoDisc("sd", &s_dvmDiscSd, cache_pages, sectors_per_page)) {
		num_mounted ++;
	}

	// Set current working directory if needed
	if (set_app_cwdir && num_mounted != 0) {
		// XX
	}

	return num_mounted != 0;
}
