// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "ext4_driver.h"
#include "dvm_debug.h"

static bool _ext4_mount(devoptab_t* dotab, DvmDisc* disc, DvmPartInfo* part);
static void _ext4_umount(void* device_data);

static void _ext4_lock(struct ext4_lock*);
static void _ext4_unlock(struct ext4_lock*);

static int _ext4_dev_open(struct ext4_blockdev*);
static int _ext4_dev_bread(struct ext4_blockdev*, void*, uint64_t, uint32_t);
static int _ext4_dev_bwrite(struct ext4_blockdev*, const void*, uint64_t, uint32_t);
static int _ext4_dev_flush(struct ext4_blockdev*);
static int _ext4_dev_close(struct ext4_blockdev*);

static int _ext4_open_r(struct _reent*, void*, const char*, int, int);
static int _ext4_close_r(struct _reent*, void*);
static ssize_t _ext4_write_r(struct _reent*, void*, const char*, size_t);
static ssize_t _ext4_read_r(struct _reent*, void*, char*, size_t);
static off_t _ext4_seek_r(struct _reent*, void*, off_t, int);
static int _ext4_fstat_r(struct _reent*, void*, struct stat*);
static int _ext4_stat_r(struct _reent*, const char*, struct stat*);
static int _ext4_link_r(struct _reent* r, const char*, const char*);
static int _ext4_unlink_r(struct _reent*, const char*);
static int _ext4_chdir_r(struct _reent*, const char*);
static int _ext4_rename_r(struct _reent*, const char*, const char*);
static int _ext4_mkdir_r(struct _reent*, const char*, int);
static DIR_ITER* _ext4_diropen_r(struct _reent*, DIR_ITER*, const char*);
static int _ext4_dirreset_r(struct _reent*, DIR_ITER*);
static int _ext4_dirnext_r(struct _reent*, DIR_ITER*, char*, struct stat*);
static int _ext4_dirclose_r(struct _reent*, DIR_ITER*);
static int _ext4_statvfs_r(struct _reent*, const char*, struct statvfs*);
static int _ext4_ftruncate_r(struct _reent*, void*, off_t);
static int _ext4_fsync_r(struct _reent*, void*);
static int _ext4_chmod_r(struct _reent*, const char*, mode_t);
static int _ext4_rmdir_r(struct _reent*, const char*);
static int _ext4_utimes_r(struct _reent*, const char*, const struct timeval[2]);
static long _ext4_fpathconf_r(struct _reent*, void*, int);
static long _ext4_pathconf_r(struct _reent*, const char*, int);
static int _ext4_symlink_r(struct _reent*, const char*, const char*);
static ssize_t _ext4_readlink_r(struct _reent*, const char*, char*, size_t);

static const devoptab_t _ext4_devoptab = {
	.structSize   = sizeof(ext4_file),
	.open_r       = _ext4_open_r,
	.close_r      = _ext4_close_r,
	.write_r      = _ext4_write_r,
	.read_r       = _ext4_read_r,
	.seek_r       = _ext4_seek_r,
	.fstat_r      = _ext4_fstat_r,
	.stat_r       = _ext4_stat_r,
	.link_r       = _ext4_link_r,
	.unlink_r     = _ext4_unlink_r,
	.chdir_r      = _ext4_chdir_r,
	.rename_r     = _ext4_rename_r,
	.mkdir_r      = _ext4_mkdir_r,
	.dirStateSize = sizeof(ext4_dir),
	.diropen_r    = _ext4_diropen_r,
	.dirreset_r   = _ext4_dirreset_r,
	.dirnext_r    = _ext4_dirnext_r,
	.dirclose_r   = _ext4_dirclose_r,
	.statvfs_r    = _ext4_statvfs_r,
	.ftruncate_r  = _ext4_ftruncate_r,
	.fsync_r      = _ext4_fsync_r,
	.chmod_r      = _ext4_chmod_r,
	.rmdir_r      = _ext4_rmdir_r,
	.lstat_r      = _ext4_stat_r,
	.utimes_r     = _ext4_utimes_r,
	.fpathconf_r  = _ext4_fpathconf_r,
	.pathconf_r   = _ext4_pathconf_r,
	.symlink_r    = _ext4_symlink_r,
	.readlink_r   = _ext4_readlink_r,
};

const DvmFsDriver g_ext2FsDriver = {
	.fstype         = "ext2",
	.device_data_sz = sizeof(Ext4Volume),
	.dotab_template = &_ext4_devoptab,
	.mount          = _ext4_mount,
	.umount         = _ext4_umount,
};

static const struct ext4_lock _ext4_locks = {
	.lock   = _ext4_lock,
	.unlock = _ext4_unlock,
};

static const struct ext4_blockdev_iface _ext4_blockdev_iface = {
	.open   = _ext4_dev_open,
	.bread  = _ext4_dev_bread,
	.bwrite = _ext4_dev_bwrite,
	.flush  = _ext4_dev_flush,
	.close  = _ext4_dev_close,
};

bool _ext4_mount(devoptab_t* dotab, DvmDisc* disc, DvmPartInfo* part)
{
	Ext4Volume* vol = (Ext4Volume*)dotab->deviceData;
	vol->disc       = disc;

	memcpy(&vol->locks, &_ext4_locks, sizeof(vol->locks));
	memcpy(&vol->bdif, &_ext4_blockdev_iface, sizeof(vol->bdif));
	vol->locks.p_user     = vol;
	vol->bdif.ph_bsize    = disc->sector_sz;
	vol->bdif.ph_bcnt     = disc->num_sectors;
	vol->bdif.p_user      = disc;
	vol->bdev.bdif        = &vol->bdif;
	vol->bdev.part_offset = (uint64_t)part->start_sector * disc->sector_sz;
	vol->bdev.part_size   = (uint64_t)part->num_sectors * disc->sector_sz;

	int rc = ext4_mount(&vol->bdev, &vol->mp, !(disc->features & FEATURE_MEDIUM_CANWRITE));
	if (rc != EOK) {
		return false;
	}

	rc = ext4_recover(&vol->mp);
	if (rc != EOK && rc != ENOTSUP) {
		ext4_umount(&vol->mp);
		return false;
	}

	rc = ext4_journal_start(&vol->mp);
	if (rc != EOK) {
		ext4_umount(&vol->mp);
		return false;
	}

	__lock_init(vol->lock);
	ext4_mount_setup_locks(&vol->mp, &vol->locks);
	dvmDiscAddUser(disc);
	return true;
}

void _ext4_umount(void* device_data)
{
	Ext4Volume* vol = (Ext4Volume*)device_data;

	ext4_journal_stop(&vol->mp);
	ext4_umount(&vol->mp);
	dvmDiscRemoveUser(vol->disc);
	__lock_close(vol->lock);
}

static inline const char* _ext4_strip_device(const char* path)
{
	char* colonpos = strchr(path, ':');
	return colonpos ? &colonpos[1] : path;
}

static void _ext4_set_stat(struct stat* st, struct ext4_mountpoint* mp, ino_t ino, struct ext4_inode* inode)
{
	struct ext4_sblock* sb = &mp->fs.sb;
	struct ext4_blockdev* bdev = mp->fs.bdev;
	struct ext4_blockdev_iface* bdif = bdev->bdif;
	DvmDisc* disc = (DvmDisc*)bdif->p_user;

	// Fill device fields
	st->st_dev = disc->io_type;
	st->st_ino = ino;

	// Fill other fields
	st->st_mode  = ext4_inode_get_mode(sb, inode);
	st->st_nlink = ext4_inode_get_links_cnt(inode);
	st->st_uid   = ext4_inode_get_uid(inode);
	st->st_gid   = ext4_inode_get_gid(inode);
	st->st_rdev  = st->st_dev;
	st->st_size  = ext4_inode_get_size(sb, inode);

	// Fill file times
	st->st_atime = ext4_inode_get_access_time(inode);
	st->st_mtime = ext4_inode_get_modif_time(inode);
	st->st_ctime = ext4_inode_get_change_inode_time(inode);

	// Fill sector-wise information
	st->st_blksize = ext4_sb_get_block_size(sb);
	st->st_blocks  = (ext4_inode_get_blocks_count(sb, inode) * EXT4_INODE_BLOCK_SIZE + S_BLKSIZE - 1) / S_BLKSIZE;
}

int _ext4_open_r(struct _reent* r, void* fd, const char* path, int flags, int mode)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	ext4_file* fil = (ext4_file*)fd;
	r->_errno = ext4_fopen2(fil, &vol->mp, _ext4_strip_device(path), flags);

	if (r->_errno == EOK) {
		r->_errno = ext4_cache_write_back(fil->mp, true);
	}

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_close_r(struct _reent* r, void* fd)
{
	ext4_file* fil = (ext4_file*)fd;
	ext4_cache_write_back(fil->mp, false);
	r->_errno = ext4_fclose(fil);

	return r->_errno == EOK ? 0 : -1;
}

ssize_t _ext4_write_r(struct _reent* r, void* fd, const char* buf, size_t len)
{
	size_t wcnt;
	ext4_file* fil = (ext4_file*)fd;
	r->_errno = ext4_fwrite(fil, buf, len, &wcnt);

	return r->_errno == EOK ? wcnt : -1;
}

ssize_t _ext4_read_r(struct _reent* r, void* fd, char* buf, size_t len)
{
	size_t rcnt;
	ext4_file* fil = (ext4_file*)fd;
	r->_errno = ext4_fread(fil, buf, len, &rcnt);

	return r->_errno == EOK ? rcnt : -1;
}

off_t _ext4_seek_r(struct _reent* r, void* fd, off_t offset, int whence)
{
	ext4_file* fil = (ext4_file*)fd;
	r->_errno = ext4_fseek(fil, (int64_t)offset, whence);

	return r->_errno == EOK ? (off_t)ext4_ftell(fil) : (off_t)-1;
}

int _ext4_fstat_r(struct _reent* r, void* fd, struct stat* st)
{
	struct ext4_inode inode;
	ext4_file* fil = (ext4_file*)fd;
	r->_errno = ext4_raw_inode_fill2(fil->mp, fil->inode, &inode);

	if (r->_errno == EOK) {
		_ext4_set_stat(st, fil->mp, fil->inode, &inode);
	}

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_stat_r(struct _reent* r, const char* path, struct stat* st)
{
	uint32_t ino;
	struct ext4_inode inode;
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_raw_inode_fill(&vol->mp, _ext4_strip_device(path), &inode, &ino);

	if (r->_errno == EOK) {
		_ext4_set_stat(st, &vol->mp, ino, &inode);
	}

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_link_r(struct _reent* r, const char* path, const char* new_path)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_flink(&vol->mp, _ext4_strip_device(path), _ext4_strip_device(new_path));

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_unlink_r(struct _reent* r, const char* path)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_fremove(&vol->mp, _ext4_strip_device(path));

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_chdir_r(struct _reent* r, const char* path)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_dir_ch(&vol->mp, _ext4_strip_device(path));

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_rename_r(struct _reent* r, const char* old_path, const char* new_path)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_frename(&vol->mp, _ext4_strip_device(old_path), _ext4_strip_device(new_path));

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_mkdir_r(struct _reent* r, const char* path, int mode)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_dir_mk(&vol->mp, _ext4_strip_device(path));

	return r->_errno == EOK ? 0 : -1;
}

DIR_ITER* _ext4_diropen_r(struct _reent* r, DIR_ITER* it, const char* path)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	ext4_dir* dir = (ext4_dir*)it->dirStruct;
	r->_errno = ext4_dir_open(dir, &vol->mp, _ext4_strip_device(path));

	return r->_errno == EOK ? it : NULL;
}

int _ext4_dirreset_r(struct _reent* r, DIR_ITER* it)
{
	ext4_dir* dir = (ext4_dir*)it->dirStruct;
	ext4_dir_entry_rewind(dir);
	return 0;
}

int _ext4_dirnext_r(struct _reent* r, DIR_ITER* it, char* filename_buf, struct stat* st)
{
	struct ext4_inode inode;
	ext4_dir* dir = (ext4_dir*)it->dirStruct;
	const ext4_direntry* de = ext4_dir_entry_next(dir);
	r->_errno = de ? EOK : ENOENT;

	if (r->_errno == EOK) {
		r->_errno = ext4_raw_inode_fill2(dir->f.mp, de->inode, &inode);
	}

	if (r->_errno == EOK) {
		// Populate filename buffer if needed
		if (filename_buf) {
			size_t name_len = NAME_MAX < de->name_length ? NAME_MAX : de->name_length;
			memcpy(filename_buf, de->name, name_len);
			filename_buf[name_len] = 0;
		}

		// Populate stat struct if needed
		if (st) {
			_ext4_set_stat(st, dir->f.mp, de->inode, &inode);
		}
	}

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_dirclose_r(struct _reent* r, DIR_ITER* it)
{
	ext4_dir* dir = (ext4_dir*)it->dirStruct;
	r->_errno = ext4_dir_close(dir);

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_statvfs_r(struct _reent* r, const char* path, struct statvfs* buf)
{
	struct ext4_mount_stats stats;
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_mount_point_stats(&vol->mp, &stats);

	if (r->_errno == EOK && buf) {
		// Block/fragment size
		buf->f_bsize   = stats.block_size;
		buf->f_frsize  = stats.block_size;

		// Block information
		buf->f_blocks  = stats.blocks_count;
		buf->f_bfree   = stats.free_blocks_count;
		buf->f_bavail  = stats.free_blocks_count;

		// Inode information
		buf->f_files   = stats.inodes_count;
		buf->f_ffree   = stats.free_inodes_count;
		buf->f_favail  = stats.free_inodes_count;

		// Other information
		buf->f_fsid    = vol->disc->io_type;
		buf->f_flag    = ST_NOSUID | (vol->mp.fs.read_only ? ST_RDONLY : 0);
		buf->f_namemax = EXT4_DIRECTORY_FILENAME_LEN;
	}

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_ftruncate_r(struct _reent* r, void* fd, off_t size)
{
	ext4_file* fil = (ext4_file*)fd;
	r->_errno = size >= 0 ? ext4_ftruncate(fil, (uint64_t)size) : EINVAL;

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_fsync_r(struct _reent* r, void* fd)
{
	ext4_file* fil = (ext4_file*)fd;
	r->_errno = ext4_cache_flush(fil->mp);

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_chmod_r(struct _reent* r, const char* path, mode_t mode)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_mode_set(&vol->mp, _ext4_strip_device(path), mode);

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_rmdir_r(struct _reent* r, const char* path)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_dir_rm(&vol->mp, _ext4_strip_device(path));

	return r->_errno == EOK ? 0 : -1;
}

int _ext4_utimes_r(struct _reent* r, const char* path, const struct timeval times[2])
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;

	uint32_t atime, mtime;
	if (times) {
		atime = times[0].tv_sec;
		mtime = times[1].tv_sec;
	} else {
		atime = mtime = time(NULL);
	}

	r->_errno = ext4_cache_write_back(&vol->mp, true);

	if (r->_errno == EOK) {
		r->_errno = ext4_atime_set(&vol->mp, _ext4_strip_device(path), atime);
	}

	if (r->_errno == EOK) {
		r->_errno = ext4_mtime_set(&vol->mp, _ext4_strip_device(path), mtime);
	}

	ext4_cache_write_back(&vol->mp, false);

	return r->_errno == EOK ? 0 : -1;
}

long _ext4_fpathconf_r(struct _reent* r, void* fd, int name)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	DvmDisc* disc = vol->disc;

	switch (name) {
		default:
			r->_errno = EINVAL;
			return -1;

		case _PC_LINK_MAX:
			return EXT4_LINK_MAX;

		case _PC_NAME_MAX:
			return EXT4_DIRECTORY_FILENAME_LEN;

		case _PC_PATH_MAX:
			return PATH_MAX;

		case _PC_NO_TRUNC:
			return 1;

		case _PC_FILESIZEBITS:
			return ext4_get32(&vol->mp.fs.sb, rev_level) == 0 ? 32 : 64;

		case _PC_2_SYMLINKS:
			return 1;

		case _PC_ALLOC_SIZE_MIN:
			return ext4_sb_get_block_size(&vol->mp.fs.sb);

		case _PC_REC_MIN_XFER_SIZE:
			return disc->block_sz ? disc->block_sz * disc->sector_sz : disc->sector_sz;

		case _PC_REC_XFER_ALIGN:
			return LIBDVM_BUFFER_ALIGN;

		case _PC_TIMESTAMP_RESOLUTION:
			return 1000000000L;
	}
}

long _ext4_pathconf_r(struct _reent* r, const char* path, int name)
{
	return _ext4_fpathconf_r(r, NULL, name);
}

int _ext4_symlink_r(struct _reent* r, const char* target, const char* path)
{
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_fsymlink(&vol->mp, _ext4_strip_device(target), _ext4_strip_device(path));

	return r->_errno == EOK ? 0 : -1;
}

ssize_t _ext4_readlink_r(struct _reent* r, const char* path, char* buf, size_t bufsiz)
{
	size_t rcnt;
	Ext4Volume* vol = (Ext4Volume*)r->deviceData;
	r->_errno = ext4_readlink(&vol->mp, _ext4_strip_device(path), buf, bufsiz, &rcnt);

	return r->_errno == EOK ? rcnt : -1;
}

//-----------------------------------------------------------------------------

void _ext4_lock(struct ext4_lock* locks)
{
	Ext4Volume* vol = (Ext4Volume*)locks->p_user;
	__lock_acquire(vol->lock);
}

void _ext4_unlock(struct ext4_lock* locks)
{
	Ext4Volume* vol = (Ext4Volume*)locks->p_user;
	__lock_release(vol->lock);
}

int _ext4_dev_open(struct ext4_blockdev* bdev)
{
	struct ext4_blockdev_iface* bdif = bdev->bdif;

	void* buf = aligned_alloc(LIBDVM_BUFFER_ALIGN, bdif->ph_bsize);
	if (!buf) {
		return ENOMEM;
	}

	bdif->ph_bbuf = (uint8_t*)buf;
	return EOK;
}

int _ext4_dev_bread(struct ext4_blockdev* bdev, void* buf, uint64_t blk_id, uint32_t blk_cnt)
{
	struct ext4_blockdev_iface* bdif = bdev->bdif;
	DvmDisc* disc = (DvmDisc*)bdif->p_user;

	return disc->vt->read_sectors(disc, buf, blk_id, blk_cnt, bdif->ph_bbuf == buf) ? EOK : EIO;
}

int _ext4_dev_bwrite(struct ext4_blockdev* bdev, const void* buf, uint64_t blk_id, uint32_t blk_cnt)
{
	struct ext4_blockdev_iface* bdif = bdev->bdif;
	DvmDisc* disc = (DvmDisc*)bdif->p_user;

	return disc->vt->write_sectors(disc, buf, blk_id, blk_cnt, bdif->ph_bbuf == buf) ? EOK : EIO;
}

int _ext4_dev_flush(struct ext4_blockdev* bdev)
{
	struct ext4_blockdev_iface* bdif = bdev->bdif;
	DvmDisc* disc = (DvmDisc*)bdif->p_user;

	return disc->vt->flush(disc) ? EOK : EIO;
}

int _ext4_dev_close(struct ext4_blockdev* bdev)
{
	struct ext4_blockdev_iface* bdif = bdev->bdif;
	free(bdif->ph_bbuf);
	return EOK;
}
