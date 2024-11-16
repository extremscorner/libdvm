// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <dvm.h>
#include "dvm_debug.h"

#define LIBDVM_EMPTY_PAGE (~(sec_t)0)

#ifdef LIBDVM_WITH_CACHE_COPY
void _dvmCacheCopy(void* dst, const void* src, size_t size);
#else
#define _dvmCacheCopy memcpy
#endif

#ifdef LIBDVM_WITH_ALIGNED_ACCESS
bool _dvmIsAlignedAccess(const void* ptr, bool is_write);
#else

// Default implementation
static bool _dvmIsAlignedAccess(const void* ptr, bool is_write)
{
	uintptr_t addr = (uintptr_t)ptr;
	return (addr & (LIBDVM_BUFFER_ALIGN-1)) == 0;
}

#endif

typedef struct DvmDiscCacheNode DvmDiscCacheNode;
typedef struct DvmDiscCacheEntry DvmDiscCacheEntry;
typedef struct DvmDiscCache DvmDiscCache;

struct DvmDiscCacheNode {
	DvmDiscCacheEntry* next;
	DvmDiscCacheEntry* prev;
};

struct DvmDiscCacheEntry {
	DvmDiscCacheNode link;
	sec_t base_sector;
	uint16_t dirty_start;
	uint16_t dirty_end;
};

struct DvmDiscCache {
	DvmDisc base;

	_LOCK_T lock;
	DvmDisc* inner;
	uint8_t* data;
	uint8_t page_shift;
	DvmDiscCacheNode list;

	DvmDiscCacheEntry entries[];
};

static DvmDiscCacheEntry* _dvmDiscCacheSearch(DvmDiscCache* self, sec_t page_sector)
{
	sec_t min_sec = LIBDVM_EMPTY_PAGE;
	DvmDiscCacheEntry* ret = NULL;

	for (DvmDiscCacheEntry* p = self->list.next; p; p = p->link.next) {
		//dvmDebug(" search %p %lx\n", p, p->base_sector);

		// Early exit if we find an unallocated cache entry
		if (p->base_sector == LIBDVM_EMPTY_PAGE) {
			break;
		}

		// Early success on cache hit
		if (p->base_sector == page_sector) {
			return p;
		}

		// Otherwise: retrieve the nearest subsequent cache entry
		if (p->base_sector > page_sector && p->base_sector < min_sec) {
			min_sec = p->base_sector;
			ret = p;
		}
	}

	return ret;
}

static uint8_t* _dvmDiscCacheEntryGetData(DvmDiscCache* self, DvmDiscCacheEntry* p)
{
	return self->data + ((p-self->entries) << self->page_shift)*self->base.sector_sz;
}

static bool _dvmDiscCacheEntryFlush(DvmDiscCache* self, DvmDiscCacheEntry* p)
{
	// Do nothing if this entry is already clean
	if (p->dirty_start >= p->dirty_end) {
		return true;
	}

	uint8_t* data = _dvmDiscCacheEntryGetData(self, p) + p->dirty_start*self->base.sector_sz;
	sec_t sector = p->base_sector + p->dirty_start;
	unsigned sz = p->dirty_end - p->dirty_start;
	dvmDebug(" flush %lx (%u) <- %p\n", sector, sz, data);

	bool ret = dvmDiscWriteSectors(self->inner, data, sector, sz);
	if (ret) {
		// Mark this entry as clean
		p->dirty_start = 1U << self->page_shift;
		p->dirty_end = 0;
	} else {
		dvmDebug(" flush error!\n");
	}

	return ret;
}

static void _dvmDiscCacheFlush(DvmDisc* self_)
{
	DvmDiscCache* self = (DvmDiscCache*)self_;
	__lock_acquire(self->lock);
	dvmDebug("cacheFlush()\n");

	for (DvmDiscCacheEntry* p = self->list.next; p; p = p->link.next) {
		// Early exit if we find an unallocated cache entry
		if (p->base_sector == LIBDVM_EMPTY_PAGE) {
			break;
		}

		_dvmDiscCacheEntryFlush(self, p);
	}

	__lock_release(self->lock);
}

static void _dvmDiscCacheDestroy(DvmDisc* self_)
{
	DvmDiscCache* self = (DvmDiscCache*)self_;

	_dvmDiscCacheFlush(self_);
	dvmDiscRemoveUser(self->inner);
	__lock_close(self->lock);
	free(self->data);
	free(self);
}

static bool _dvmDiscCacheReadWrite(
	DvmDiscCache* self, uint8_t* buffer, sec_t first_sector, sec_t num_sectors,
	bool is_write)
{
	// Early fail on first sector being out of bounds
	if (first_sector >= self->base.num_sectors) {
		return false;
	}

	// Early fail on last sector being out of bounds
	sec_t max_sectors = self->base.num_sectors - first_sector;
	if (num_sectors > max_sectors) {
		return false;
	}

	const bool is_aligned = _dvmIsAlignedAccess(buffer, is_write);
	const unsigned page_sz = 1U << self->page_shift;
	const unsigned page_mask = page_sz - 1;

	DvmDiscCacheEntry* p = NULL;
	sec_t search_base = 0;

	while (num_sectors) {
		// Calculate associated page & offset within page
		sec_t cur_page_sector = first_sector & ~(sec_t)page_mask;
		unsigned cur_page_offset = first_sector & page_mask;

		// Calculate max sectors to access within this page
		sec_t max_cur_sectors = page_sz - cur_page_offset;
		sec_t cur_sectors = num_sectors < max_cur_sectors ? num_sectors : max_cur_sectors;

		// Check if the entire page is accessed (i.e. not a partial read/write)
		bool is_whole = cur_page_offset == 0 && cur_sectors == page_sz;

		// Search the cache for this page if needed
		if (cur_page_sector >= search_base) {
			p = _dvmDiscCacheSearch(self, cur_page_sector);
			if (p) {
				dvmDebug(" search %lx -> %p %lx\n", cur_page_sector, p, p->base_sector);
				search_base = p->base_sector+1;
			} else {
				dvmDebug(" miss %lx\n", cur_page_sector);
				search_base = LIBDVM_EMPTY_PAGE;
			}
		}

		// Cache hit:
		if (p && p->base_sector == cur_page_sector) {
_cacheHit:
			uint8_t* data = _dvmDiscCacheEntryGetData(self, p) + cur_page_offset*self->base.sector_sz;
			if (is_write) {
				_dvmCacheCopy(data, buffer, cur_sectors*self->base.sector_sz);

				// Update dirty range
				unsigned dirty_end = cur_page_offset + cur_sectors;
				if (cur_page_offset < p->dirty_start) {
					p->dirty_start = cur_page_offset;
				}
				if (dirty_end > p->dirty_end) {
					p->dirty_end = dirty_end;
				}
			} else {
				_dvmCacheCopy(buffer, data, cur_sectors*self->base.sector_sz);
			}

			if (!is_whole && p != self->list.next) {
				// Make this the MRU
				p->link.prev->link.next = p->link.next;
				(p->link.next ? &p->link.next->link : &self->list)->prev = p->link.prev;
				p->link.next = self->list.next;
				p->link.next->link.prev = p;
				p->link.prev = NULL;
				self->list.next = p;
			}
		}

		// Partial access or misaligned access:
		else if (!is_whole || !is_aligned) {
			p = self->list.prev;
			while (p->base_sector == LIBDVM_EMPTY_PAGE && p->link.prev && p->link.prev->base_sector == LIBDVM_EMPTY_PAGE) {
				p = p->link.prev;
			}

			if (!_dvmDiscCacheEntryFlush(self, p)) {
				return false;
			}

			p->base_sector = cur_page_sector;

			if (!is_write || !is_whole) {
				// Read in...
				void* data = _dvmDiscCacheEntryGetData(self, p);
				sec_t max_sz = self->base.num_sectors - cur_page_sector;
				unsigned sz = page_sz < max_sz ? page_sz : max_sz;
				dvmDebug(" load %lx (%u) -> %p\n", cur_page_sector, sz, data);

				if (!dvmDiscReadSectors(self->inner, data, cur_page_sector, sz)) {
					p->base_sector = LIBDVM_EMPTY_PAGE;
					dvmDebug(" load error!\n");
					return false;
				}
			}

			goto _cacheHit;
		}

		// Direct access (straight into user buffer):
		else {
			// Calculate maximum sectors that can be directly accessed
			// (up until the next cached page or disc end if no more pages)
			max_cur_sectors = (p ? p->base_sector : self->base.num_sectors) - first_sector;
			cur_sectors = num_sectors < max_cur_sectors ? num_sectors : max_cur_sectors;
			dvmDebug(" direct %lx (%lu) %s %p\n", first_sector, cur_sectors, is_write ? "<-" : "->", buffer);

			bool ret;
			if (is_write) {
				ret = dvmDiscWriteSectors(self->inner, buffer, first_sector, cur_sectors);
			} else {
				ret = dvmDiscReadSectors(self->inner, buffer, first_sector, cur_sectors);
			}

			if (!ret) {
				dvmDebug(" direct fail\n");
				return false;
			}
		}

		//dvmDebug("advance %lx %lx\n", first_sector, cur_sectors);
		buffer += cur_sectors*self->base.sector_sz;
		first_sector += cur_sectors;
		num_sectors -= cur_sectors;
	}

	return true;
}

static bool _dvmDiscCacheReadSectors(DvmDisc* self_, void* buffer, sec_t first_sector, sec_t num_sectors)
{
	DvmDiscCache* self = (DvmDiscCache*)self_;
	__lock_acquire(self->lock);
	dvmDebug("cacheRead(%p,0x%lx,%lu)\n", buffer, first_sector, num_sectors);
	bool ret = _dvmDiscCacheReadWrite(self, (uint8_t*)buffer, first_sector, num_sectors, false);
	__lock_release(self->lock);
	return ret;
}

static bool _dvmDiscCacheWriteSectors(DvmDisc* self_, const void* buffer, sec_t first_sector, sec_t num_sectors)
{
	DvmDiscCache* self = (DvmDiscCache*)self_;
	__lock_acquire(self->lock);
	dvmDebug("cacheWrite(%p,0x%lx,%lu)\n", buffer, first_sector, num_sectors);
	bool ret = _dvmDiscCacheReadWrite(self, (uint8_t*)buffer, first_sector, num_sectors, true);
	__lock_release(self->lock);
	return ret;
}

static const DvmDiscIface s_dvmDiscCacheIface = {
	.destroy       = _dvmDiscCacheDestroy,
	.read_sectors  = _dvmDiscCacheReadSectors,
	.write_sectors = _dvmDiscCacheWriteSectors,
	.flush         = _dvmDiscCacheFlush,
};

DvmDisc* dvmDiscCacheCreate(DvmDisc* inner_disc, unsigned cache_pages, unsigned sectors_per_page)
{
	// Parameter validation
	if (!cache_pages || !sectors_per_page || (sectors_per_page & (sectors_per_page-1))) {
		return inner_disc;
	}

	DvmDiscCache* disc = (DvmDiscCache*)malloc(sizeof(DvmDiscCache) + cache_pages*sizeof(DvmDiscCacheEntry));
	if (!disc) {
		return inner_disc;
	}

	void* data = aligned_alloc(LIBDVM_BUFFER_ALIGN, cache_pages*sectors_per_page*inner_disc->sector_sz);
	if (!data) {
		free(disc);
		return inner_disc;
	}

	memset(disc, 0, sizeof(DvmDiscCache));
	disc->base.vt = &s_dvmDiscCacheIface;
	disc->base.io_type = inner_disc->io_type;
	disc->base.features = inner_disc->features;
	disc->base.num_sectors = inner_disc->num_sectors;
	disc->base.sector_sz = inner_disc->sector_sz;
	__lock_init(disc->lock);
	dvmDiscAddUser(inner_disc);
	disc->inner = inner_disc;
	disc->data = (uint8_t*)data;
	disc->page_shift = 0;

	// Calculate page shift (log2)
	while ((1U << disc->page_shift) != sectors_per_page) {
		disc->page_shift ++;
	}

	// Initialize cache entries
	disc->list.next = &disc->entries[0];
	disc->list.prev = &disc->entries[cache_pages-1];
	for (unsigned i = 0; i < cache_pages; i ++) {
		DvmDiscCacheEntry* p = &disc->entries[i];

		p->link.next = (i+1) < cache_pages ? &p[1] : NULL;
		p->link.prev = i ? &p[-1] : NULL;

		p->base_sector = LIBDVM_EMPTY_PAGE;
		p->dirty_start = sectors_per_page;
		p->dirty_end = 0;
	}

	return &disc->base;
}
