#include <calico.h>
#include <dvm.h>

MK_WEAK unsigned g_dvmDefaultCachePages = 16;
MK_WEAK unsigned g_dvmDefaultSectorsPerPage = 8;

void _dvmSetAppWorkingDir(const char* argv0);

bool _dvmIsAlignedAccess(const void* ptr, bool is_write)
{
	uptr addr = (uptr)ptr;
	uptr align = is_write ? 4 : ARM_CACHE_LINE_SZ;
	return ((addr & (align-1)) == 0) && addr >= MM_MAINRAM && addr < MM_DTCM;
}

void _dvmCacheCopy(void* dst, const void* src, size_t size)
{
	if_likely ((((uptr)dst | (uptr)src) & 3) == 0) {
		armCopyMem32(dst, src, size);
	} else {
		__builtin_memcpy(dst, src, size);
	}
}

static void _dvmDiscCalicoDummy(DvmDisc* self_)
{
	// Nothing
}

static bool _dvmDiscCalicoReadSectors(DvmDisc* self, void* buffer, sec_t first_sector, sec_t num_sectors)
{
	BlkDevice dev = (BlkDevice)self->io_type;
	return blkDevReadSectors(dev, buffer, first_sector, num_sectors);
}

static bool _dvmDiscCalicoWriteSectors(DvmDisc* self, const void* buffer, sec_t first_sector, sec_t num_sectors)
{
	BlkDevice dev = (BlkDevice)self->io_type;
	return blkDevWriteSectors(dev, buffer, first_sector, num_sectors);
}

static const DvmDiscIface s_dvmDiscCalicoIface = {
	.destroy       = _dvmDiscCalicoDummy,
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

static DvmDisc* _dvmGetCalicoDisc(DvmDisc* disc, unsigned cache_pages, unsigned sectors_per_page)
{
	BlkDevice dev = (BlkDevice)disc->io_type;

	// Ensure the disc is initialized and present
	if (!blkDevInit(dev) || !blkDevIsPresent(dev)) {
		return NULL;
	}

	// Populate DLDI features if needed
	if (dev == BlkDevice_Dldi) {
		disc->features = g_envExtraInfo->dldi_features;
	}

	// Populate disc size
	disc->num_sectors = blkDevGetSectorCount(dev);

	// Create disc cache
	disc = dvmDiscCacheCreate(disc, cache_pages, sectors_per_page);

	return disc;
}

static inline unsigned _dvmGetAndMountCalicoDisc(const char* name, DvmDisc* disc, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;
	disc = _dvmGetCalicoDisc(disc, cache_pages, sectors_per_page);
	if (disc) {
		num_mounted = dvmProbeMountDisc(name, disc);
		if (!num_mounted) {
			disc->vt->destroy(disc);
		}
	}

	return num_mounted;
}

bool dvmInit(bool set_app_cwdir, unsigned cache_pages, unsigned sectors_per_page)
{
	unsigned num_mounted = 0;

	// Initialize block device subsystem
	blkInit();

	// Try mounting DLDI
	num_mounted += _dvmGetAndMountCalicoDisc("fat", &s_dvmDiscDldi, cache_pages, sectors_per_page);

	if (systemIsTwlMode()) {
		// Try mounting DSi SD card
		num_mounted += _dvmGetAndMountCalicoDisc("sd", &s_dvmDiscSd, cache_pages, sectors_per_page);
	}

	// Set current working directory if needed
	if (set_app_cwdir && num_mounted != 0) {
		const char* argv0 = g_envNdsArgvHeader->argv[0];
		if (argv0) {
			_dvmSetAppWorkingDir(argv0);
		}
	}

	return num_mounted != 0;
}
