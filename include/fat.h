#pragma once
#include <dvm.h>

// File attributes
#define ATTR_READONLY  0x01 // Read only
#define ATTR_HIDDEN    0x02 // Hidden
#define ATTR_SYSTEM    0x04 // System
#define ATTR_VOLUME    0x08 // Volume
#define ATTR_DIRECTORY 0x10 // Directory
#define ATTR_ARCHIVE   0x20 // Archive

#ifdef __cplusplus
extern "C" {
#endif

// vfat filesystem driver
extern const DvmFsDriver g_vfatFsDriver;

// Compatibility functions
bool fatInitDefault(void);
bool fatInit(unsigned cache_pages, bool set_app_cwdir);
bool fatMountSimple(const char* name, const DISC_INTERFACE* iface);
bool fatMount(const char* name, const DISC_INTERFACE* iface, uint32_t start_sector, unsigned cache_pages, unsigned sectors_per_page);

// FAT specific filesystem functions
void fatGetVolumeLabel(const char* name, char* label_out);
void fatSetVolumeLabel(const char* name, const char* label);
int FAT_getAttr(const char* filename);
int FAT_setAttr(const char* filename, unsigned attr);

#ifdef __cplusplus
}
#endif
