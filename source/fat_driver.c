// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "fat_driver.h"
#include "dvm_debug.h"

static bool _FAT_mount_vfat(devoptab_t* dotab, DvmDisc* disc, DvmPartInfo* part);
static bool _FAT_mount_exfat(devoptab_t* dotab, DvmDisc* disc, DvmPartInfo* part);
static void _FAT_umount(void* device_data);

static int _FAT_open_r(struct _reent*, void*, const char*, int, int);
static int _FAT_close_r(struct _reent*, void*);
static ssize_t _FAT_write_r(struct _reent*, void*, const char*, size_t);
static ssize_t _FAT_read_r(struct _reent*, void*, char*, size_t);
static off_t _FAT_seek_r(struct _reent*, void*, off_t, int);
static int _FAT_fstat_r(struct _reent*, void*, struct stat*);
static int _FAT_stat_r(struct _reent*, const char*, struct stat*);
static int _FAT_unlink_r(struct _reent*, const char*);
static int _FAT_chdir_r(struct _reent*, const char*);
static int _FAT_rename_r(struct _reent*, const char*, const char*);
static int _FAT_mkdir_r(struct _reent*, const char*, int);
static DIR_ITER* _FAT_diropen_r(struct _reent*, DIR_ITER*, const char*);
static int _FAT_dirreset_r(struct _reent*, DIR_ITER*);
static int _FAT_dirnext_r(struct _reent*, DIR_ITER*, char*, struct stat*);
static int _FAT_dirclose_r(struct _reent*, DIR_ITER*);
static int _FAT_statvfs_r(struct _reent*, const char*, struct statvfs*);
static int _FAT_ftruncate_r(struct _reent*, void*, off_t);
static int _FAT_fsync_r(struct _reent*, void*);
static int _FAT_rmdir_r(struct _reent*, const char*);
static int _FAT_utimes_r(struct _reent*, const char*, const struct timeval[2]);
static long _FAT_fpathconf_r(struct _reent*, void*, int);
static long _FAT_pathconf_r(struct _reent*, const char*, int);

static const devoptab_t _FAT_devoptab = {
	.structSize   = sizeof(FFFIL),
	.open_r       = _FAT_open_r,
	.close_r      = _FAT_close_r,
	.write_r      = _FAT_write_r,
	.read_r       = _FAT_read_r,
	.seek_r       = _FAT_seek_r,
	.fstat_r      = _FAT_fstat_r,
	.stat_r       = _FAT_stat_r,
	.unlink_r     = _FAT_unlink_r,
	.chdir_r      = _FAT_chdir_r,
	.rename_r     = _FAT_rename_r,
	.mkdir_r      = _FAT_mkdir_r,
	.dirStateSize = sizeof(FFDIR) + sizeof(FILINFO),
	.diropen_r    = _FAT_diropen_r,
	.dirreset_r   = _FAT_dirreset_r,
	.dirnext_r    = _FAT_dirnext_r,
	.dirclose_r   = _FAT_dirclose_r,
	.statvfs_r    = _FAT_statvfs_r,
	.ftruncate_r  = _FAT_ftruncate_r,
	.fsync_r      = _FAT_fsync_r,
	.rmdir_r      = _FAT_rmdir_r,
	.lstat_r      = _FAT_stat_r,
	.utimes_r     = _FAT_utimes_r,
	.fpathconf_r  = _FAT_fpathconf_r,
	.pathconf_r   = _FAT_pathconf_r,
};

const DvmFsDriver g_vfatFsDriver = {
	.fstype         = "vfat",
	.device_data_sz = sizeof(FatVolume),
	.dotab_template = &_FAT_devoptab,
	.mount          = _FAT_mount_vfat,
	.umount         = _FAT_umount,
};

const DvmFsDriver g_exfatFsDriver = {
	.fstype         = "exfat",
	.device_data_sz = sizeof(FatVolume),
	.dotab_template = &_FAT_devoptab,
	.mount          = _FAT_mount_exfat,
	.umount         = _FAT_umount,
};

static FatVolume* _fatVolumeFromPath(const char* path)
{
	const devoptab_t* dotab = GetDeviceOpTab(path);
	if (!dotab || dotab->open_r != _FAT_open_r) {
		return NULL;
	}

	return (FatVolume*)dotab->deviceData;
}

bool _FAT_mount_vfat(devoptab_t* dotab, DvmDisc* disc, DvmPartInfo* part)
{
	FatVolume* vol    = (FatVolume*)dotab->deviceData;
	vol->disc         = disc;
	vol->start_sector = part->start_sector;
	vol->num_sectors  = part->num_sectors;

	UINT ipart = vol->start_sector ? 0 : part->index;
	FRESULT fr = f_mount(&vol->fs, vol, ipart);

#ifdef FEATURE_MEDIUM_CANFORMAT
	if (fr == FR_NO_FILESYSTEM && (disc->features & FEATURE_MEDIUM_CANFORMAT)) {
		f_umount(&vol->fs);

		MKFS_PARM opt;
		opt.fmt = FM_FAT | FM_FAT32;
		if (ipart == 0) {
			opt.fmt |= FM_SFD;
		}
		opt.n_fat = 2;
		opt.align = 0;
		opt.n_root = 0;
		opt.au_size = 0;

		fr = f_mkfs(vol, ipart, &opt, vol->fs.win, sizeof(vol->fs.win));

		if (fr == FR_OK) {
			fr = f_mount(&vol->fs, vol, ipart);
		}
	}
#endif

	if (fr != FR_OK) {
		f_umount(&vol->fs);
		return false;
	}

	dvmDiscAddUser(disc);
	return true;
}

bool _FAT_mount_exfat(devoptab_t* dotab, DvmDisc* disc, DvmPartInfo* part)
{
	FatVolume* vol    = (FatVolume*)dotab->deviceData;
	vol->disc         = disc;
	vol->start_sector = part->start_sector;
	vol->num_sectors  = part->num_sectors;

	UINT ipart = vol->start_sector ? 0 : part->index;
	FRESULT fr = f_mount(&vol->fs, vol, ipart);

#ifdef FEATURE_MEDIUM_CANFORMAT
	if (fr == FR_NO_FILESYSTEM && (disc->features & FEATURE_MEDIUM_CANFORMAT)) {
		f_umount(&vol->fs);

		MKFS_PARM opt;
		opt.fmt = FM_EXFAT;
		if (ipart == 0) {
			opt.fmt |= FM_SFD;
		}
		opt.n_fat = 0;
		opt.align = 0;
		opt.n_root = 0;
		opt.au_size = 0;

		fr = f_mkfs(vol, ipart, &opt, vol->fs.win, sizeof(vol->fs.win));

		if (fr == FR_OK) {
			fr = f_mount(&vol->fs, vol, ipart);
		}
	}
#endif

	if (fr != FR_OK) {
		f_umount(&vol->fs);
		return false;
	}

	dvmDiscAddUser(disc);
	return true;
}

void _FAT_umount(void* device_data)
{
	FatVolume* vol = (FatVolume*)device_data;

	f_umount(&vol->fs);
	dvmDiscRemoveUser(vol->disc);
}

static inline bool _FAT_set_errno(FRESULT fr, int* _errno)
{
	static const uint8_t fr_to_errno[] = {
		[FR_OK]                  = 0,
		[FR_DISK_ERR]            = EIO,
		[FR_INT_ERR]             = EINVAL,
		[FR_NOT_READY]           = EIO,
		[FR_NO_FILE]             = ENOENT,
		[FR_NO_PATH]             = ENOENT,
		[FR_INVALID_NAME]        = EINVAL,
		[FR_DENIED]              = EACCES,
		[FR_EXIST]               = EEXIST,
		[FR_INVALID_OBJECT]      = EFAULT,
		[FR_WRITE_PROTECTED]     = EROFS,
		[FR_INVALID_DRIVE]       = ENODEV,
		[FR_NOT_ENABLED]         = ENOEXEC,
		[FR_NO_FILESYSTEM]       = ENFILE,
		[FR_MKFS_ABORTED]        = ENOEXEC,
		[FR_TIMEOUT]             = EAGAIN,
		[FR_LOCKED]              = EBUSY,
		[FR_NOT_ENOUGH_CORE]     = ENOMEM,
		[FR_TOO_MANY_OPEN_FILES] = EMFILE,
		[FR_INVALID_PARAMETER]   = EINVAL,
	};

	if ((unsigned)fr >= sizeof(fr_to_errno)) {
		fr = FR_INVALID_PARAMETER;
	}

	if (fr != FR_OK) {
		dvmDebug("FatFs fr = %u\n", fr);
	}

	if (!_errno) {
		_errno = &errno;
	}

	*_errno = fr_to_errno[fr];
	return fr == FR_OK;
}

static inline const char* _FAT_strip_device(const char* path)
{
	char* colonpos = strchr(path, ':');
	return colonpos ? &colonpos[1] : path;
}

static DWORD _FAT_make_fattime(const time_t* time)
{
	struct tm stm;
	localtime_r(time, &stm);

	return
		(DWORD)(stm.tm_year - 80) << 25 |
		(DWORD)(stm.tm_mon + 1) << 21 |
		(DWORD)stm.tm_mday << 16 |
		(DWORD)stm.tm_hour << 11 |
		(DWORD)stm.tm_min << 5 |
		(DWORD)stm.tm_sec >> 1;
}

static time_t _FAT_make_time(WORD fdate, WORD ftime)
{
	struct tm arg = {
		.tm_sec   = (ftime & 0x1f) << 1,
		.tm_min   = (ftime >> 5) & 0x3f,
		.tm_hour  = ftime >> 11,
		.tm_mday  = fdate & 0x1f,
		.tm_mon   = ((fdate >> 5) & 0xf) - 1,
		.tm_year  = (fdate >> 9) + 80,
		.tm_wday  = 0,
		.tm_yday  = 0,
		.tm_isdst = 0,
	};

	return mktime(&arg);
}

static void _FAT_set_stat(struct stat* st, const FILINFO* fno, FatVolume* vol)
{
	// Fill device fields
	st->st_dev = vol->disc->io_type;
	st->st_ino = fno->fclust;

	// Generate fake POSIX mode
	st->st_mode = S_IRUSR | S_IRGRP | S_IROTH;
	if (!(fno->fattrib & AM_RDO)) {
		st->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	}
	if (fno->fattrib & AM_DIR) {
		st->st_mode |= S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	} else {
		st->st_mode |= S_IFREG;
	}

	// Fill other fields
	st->st_nlink = 1; // Always one hard link on a FAT file
	st->st_uid   = 1; // Faked for FAT
	st->st_gid   = 2; // Faked for FAT
	st->st_rdev  = st->st_dev;
	st->st_size  = fno->fsize;

	// Fill file times
	st->st_atime = _FAT_make_time(fno->acdate, fno->actime);
	st->st_mtime = _FAT_make_time(fno->fdate, fno->ftime);
	st->st_ctime = _FAT_make_time(fno->crdate, fno->crtime);

	// Fill sector-wise information
	st->st_blksize = vol->fs.csize * vol->fs.ssize;
	st->st_blocks  = ((st->st_size + st->st_blksize - 1) & ~((off_t)st->st_blksize - 1)) / S_BLKSIZE;
}

int _FAT_open_r(struct _reent* r, void* fd, const char* path, int flags, int mode)
{
	FatVolume* vol = (FatVolume*)r->deviceData;
	FFFIL* fp = (FFFIL*)fd;
	BYTE ffmode = 0;

	// Convert file access mode to FatFs mode flags
	// https://www.gnu.org/software/libc/manual/html_node/Access-Modes.html
	switch (flags & O_ACCMODE) {
		case O_RDONLY:
			ffmode = FA_READ;
			if (flags & O_APPEND) {
				// O_RDONLY|O_APPEND is nonsense - disallow
				r->_errno = EINVAL;
				return -1;
			}
			break;

		case O_WRONLY:
			ffmode = FA_WRITE;
			break;

		case O_RDWR:
			ffmode = FA_READ | FA_WRITE;
			break;

		default:
			r->_errno = EINVAL;
			return -1;
	}

	// Convert open-time flags to FatFs mode flags
	// https://www.gnu.org/software/libc/manual/html_node/Open_002dtime-Flags.html
	if (flags & O_CREAT)  ffmode |= FA_OPEN_ALWAYS;
	if (flags & O_EXCL)   ffmode |= FA_CREATE_NEW;
	if (flags & O_TRUNC)  ffmode |= FA_CREATE_ALWAYS;

	// Convert I/O operating modes to FatFs mode flags
	// https://www.gnu.org/software/libc/manual/html_node/Operating-Modes.html
	if (flags & O_APPEND) ffmode |= FA_OPEN_APPEND;

	// Normalize O_TRUNC|O_CREAT -> O_TRUNC
	if ((ffmode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS)) == (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS)) {
		ffmode &= ~FA_OPEN_ALWAYS;
	}

	FRESULT fr = f_open(fp, &vol->fs, _FAT_strip_device(path), ffmode);

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_close_r(struct _reent* r, void* fd)
{
	FFFIL* fp = (FFFIL*)fd;
	FRESULT fr = f_close(fp);

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

ssize_t _FAT_write_r(struct _reent* r, void* fd, const char* buf, size_t len)
{
	UINT bw;
	FFFIL* fp = (FFFIL*)fd;
	FRESULT fr = f_write(fp, buf, len, &bw);

	return _FAT_set_errno(fr, &r->_errno) ? bw : -1;
}

ssize_t _FAT_read_r(struct _reent* r, void* fd, char* buf, size_t len)
{
	UINT br;
	FFFIL* fp = (FFFIL*)fd;
	FRESULT fr = f_read(fp, buf, len, &br);

	return _FAT_set_errno(fr, &r->_errno) ? br : -1;
}

off_t _FAT_seek_r(struct _reent* r, void* fd, off_t offset, int whence)
{
	FSIZE_t pos;
	FFFIL* fp = (FFFIL*)fd;

	// Retrieve seek base position
	switch (whence) {
		default:
			r->_errno = EINVAL;
			return -1;

		case SEEK_SET:
			pos = 0;
			break;

		case SEEK_CUR:
			pos = f_tell(fp);
			break;

		case SEEK_END:
			pos = f_size(fp);
			break;
	}

	FRESULT fr;
	if (offset >= 0 || pos >= (-offset)) {
		// Apply new position
		pos += (FSIZE_t)offset;
		fr = pos != f_tell(fp) ? f_lseek(fp, pos) : FR_OK;
	} else {
		// Attempted to seek before the beginning of the file
		fr = FR_INVALID_PARAMETER;
	}

	return _FAT_set_errno(fr, &r->_errno) ? (off_t)pos : (off_t)-1;
}

int _FAT_fstat_r(struct _reent* r, void* fd, struct stat* st)
{
	FFFIL* fp = (FFFIL*)fd;
	FatVolume* vol = _fatVolumeFromFatFs(fp->obj.fs);

	// Fill device fields
	st->st_dev = vol->disc->io_type;
	st->st_ino = fp->obj.sclust;

	// Generate fake POSIX mode
	st->st_mode = S_IRUSR | S_IRGRP | S_IROTH | S_IFREG;
	if (fp->flag & FA_WRITE) {
		st->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	}

	// Fill other fields
	st->st_nlink = 1; // Always one hard link on a FAT file
	st->st_uid   = 1; // Faked for FAT
	st->st_gid   = 2; // Faked for FAT
	st->st_rdev  = st->st_dev;
	st->st_size  = st->st_ino ? fp->obj.objsize : 0;

	// XX: Retrieving file times requires reading the directory entry again
	st->st_atime = st->st_mtime = st->st_ctime = 0;

	// Fill sector-wise information
	st->st_blksize = vol->fs.csize * vol->fs.ssize;
	st->st_blocks  = ((st->st_size + st->st_blksize - 1) & ~((off_t)st->st_blksize - 1)) / S_BLKSIZE;

	r->_errno = 0;
	return 0;
}

int _FAT_stat_r(struct _reent* r, const char* path, struct stat* st)
{
	FILINFO fno;
	FatVolume* vol = (FatVolume*)r->deviceData;
	FRESULT fr = f_stat(&vol->fs, _FAT_strip_device(path), &fno);

	if (fr == FR_OK) {
		_FAT_set_stat(st, &fno, vol);
	}

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

static int _FAT_unlink_rmdir_r(struct _reent* r, const char* path, bool is_rmdir)
{
	FatVolume* vol = (FatVolume*)r->deviceData;
	FRESULT fr = f_unlink(&vol->fs, _FAT_strip_device(path), is_rmdir);

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_unlink_r(struct _reent* r, const char* path)
{
	return _FAT_unlink_rmdir_r(r, path, false);
}

int _FAT_rmdir_r(struct _reent* r, const char* path)
{
	return _FAT_unlink_rmdir_r(r, path, true);
}

int _FAT_chdir_r(struct _reent* r, const char* path)
{
	FatVolume* vol = (FatVolume*)r->deviceData;
	FRESULT fr = f_chdir(&vol->fs, _FAT_strip_device(path));

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_rename_r(struct _reent* r, const char* old_path, const char* new_path)
{
	FatVolume* vol = (FatVolume*)r->deviceData;
	FRESULT fr = f_rename(&vol->fs, _FAT_strip_device(old_path), _FAT_strip_device(new_path));

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_mkdir_r(struct _reent* r, const char* path, int mode)
{
	FatVolume* vol = (FatVolume*)r->deviceData;
	FRESULT fr = f_mkdir(&vol->fs, _FAT_strip_device(path));

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

DIR_ITER* _FAT_diropen_r(struct _reent* r, DIR_ITER* it, const char* path)
{
	FatVolume* vol = (FatVolume*)r->deviceData;
	FFDIR* dp = (FFDIR*)it->dirStruct;
	FRESULT fr = f_opendir(dp, &vol->fs, _FAT_strip_device(path));

	return _FAT_set_errno(fr, &r->_errno) ? it : NULL;
}

int _FAT_dirreset_r(struct _reent* r, DIR_ITER* it)
{
	FFDIR* dp = (FFDIR*)it->dirStruct;
	FRESULT fr = f_rewinddir(dp);

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_dirnext_r(struct _reent* r, DIR_ITER* it, char* filename_buf, struct stat* st)
{
	FFDIR* dp = (FFDIR*)it->dirStruct;
	FILINFO* fno = (FILINFO*)&dp[1];
	FRESULT fr = f_readdir(dp, fno);

	// Check for end of directory
	if (fr == FR_OK && !fno->fname[0]) {
		fr = FR_NO_FILE;
	}

	if (fr == FR_OK) {
		// Populate filename buffer if needed
		if (filename_buf) {
			size_t name_len = strnlen(fno->fname, NAME_MAX < FF_LFN_BUF ? NAME_MAX : FF_LFN_BUF);
			memcpy(filename_buf, fno->fname, name_len + 1);
		}

		// Populate stat struct if needed
		if (st) {
			_FAT_set_stat(st, fno, _fatVolumeFromFatFs(dp->obj.fs));
		}
	}

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_dirclose_r(struct _reent* r, DIR_ITER* it)
{
	FFDIR* dp = (FFDIR*)it->dirStruct;
	FRESULT fr = f_closedir(dp);

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_statvfs_r(struct _reent* r, const char* path, struct statvfs* buf)
{
	DWORD nclst;
	FatVolume* vol = (FatVolume*)r->deviceData;
	DvmDisc* disc = vol->disc;
	FRESULT fr = f_getfree(&vol->fs, &nclst);

	if (fr == FR_OK && buf) {
		// Block/fragment size = cluster size
		buf->f_bsize   = vol->fs.csize * vol->fs.ssize;
		buf->f_frsize  = buf->f_bsize;

		// Block information = total/free clusters
		buf->f_blocks  = vol->fs.n_fatent - 2;
		buf->f_bfree   = nclst;
		buf->f_bavail  = buf->f_bfree;

		// Inode information: not applicable to FAT
		buf->f_files   = 0;
		buf->f_ffree   = 0;
		buf->f_favail  = 0;

		// Other information
		buf->f_fsid    = disc->io_type;
		buf->f_flag    = ST_NOSUID | ((disc->features & FEATURE_MEDIUM_CANWRITE) ? 0 : ST_RDONLY);
		buf->f_namemax = FF_LFN_BUF; // Assuming NAME_MAX == FF_LFN_BUF
	}

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_ftruncate_r(struct _reent* r, void* fd, off_t size)
{
	FSIZE_t pos_backup;
	FFFIL* fp = (FFFIL*)fd;
	FRESULT fr = size >= 0 ? FR_OK : FR_INVALID_PARAMETER;

	if (fr == FR_OK) {
		f_expand(fp, (FSIZE_t)size, 1);

		pos_backup = f_tell(fp);
		if (pos_backup != (FSIZE_t)size) {
			if (pos_backup > (FSIZE_t)size) {
				pos_backup = (FSIZE_t)size;
			}

			fr = f_lseek(fp, (FSIZE_t)size);
		}
	}

	if (fr == FR_OK) {
		fr = f_truncate(fp);
	}

	if (fr == FR_OK && f_tell(fp) != pos_backup) {
		fr = f_lseek(fp, pos_backup);
	}

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_fsync_r(struct _reent* r, void* fd)
{
	FFFIL* fp = (FFFIL*)fd;
	FRESULT fr = f_sync(fp);

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

int _FAT_utimes_r(struct _reent* r, const char* path, const struct timeval times[2])
{
	FatVolume* vol = (FatVolume*)r->deviceData;

	DWORD tm[2];
	if (times) {
		tm[0] = _FAT_make_fattime(&times[0].tv_sec);
		tm[1] = _FAT_make_fattime(&times[1].tv_sec);
	} else {
		tm[0] = tm[1] = get_fattime();
	}

	FILINFO fno;
	fno.acdate = (WORD)(tm[0] >> 16);
	fno.actime = (WORD)tm[0];
	fno.fdate  = (WORD)(tm[1] >> 16);
	fno.ftime  = (WORD)tm[1];
	fno.crdate = 0;
	fno.crtime = 0;

	FRESULT fr = f_utime(&vol->fs, _FAT_strip_device(path), &fno);

	return _FAT_set_errno(fr, &r->_errno) ? 0 : -1;
}

long _FAT_fpathconf_r(struct _reent* r, void* fd, int name)
{
	FatVolume* vol = (FatVolume*)r->deviceData;

	switch (name) {
		default:
			r->_errno = EINVAL;
			return -1;

		case _PC_LINK_MAX:
			return 1;

		case _PC_NAME_MAX:
			return FF_LFN_BUF;

		case _PC_PATH_MAX:
			return PATH_MAX;

		case _PC_NO_TRUNC:
			return 1;

		case _PC_FILESIZEBITS:
			return vol->fs.fs_type != FS_EXFAT ? 33 : 64;

		case _PC_ALLOC_SIZE_MIN:
			return vol->fs.csize * vol->fs.ssize;

		case _PC_REC_XFER_ALIGN:
			return LIBDVM_BUFFER_ALIGN;

		case _PC_TIMESTAMP_RESOLUTION:
			return 2000000000L;
	}
}

long _FAT_pathconf_r(struct _reent* r, const char* path, int name)
{
	return _FAT_fpathconf_r(r, NULL, name);
}

bool fatGetVolumeLabel(const char* name, char* label_out)
{
	FatVolume* vol = _fatVolumeFromPath(name);
	if (!vol) {
		return false;
	}

	FRESULT fr = f_getlabel(&vol->fs, label_out, NULL);

	return fr == FR_OK;
}

bool fatSetVolumeLabel(const char* name, const char* label)
{
	FatVolume* vol = _fatVolumeFromPath(name);
	if (!vol) {
		return false;
	}

	FRESULT fr = f_setlabel(&vol->fs, label);

	return fr == FR_OK;
}

int FAT_getAttr(const char* path)
{
	FatVolume* vol = _fatVolumeFromPath(path);
	if (!vol) {
		errno = ENOSYS;
		return -1;
	}

	FILINFO fno;
	FRESULT fr = f_stat(&vol->fs, _FAT_strip_device(path), &fno);

	return _FAT_set_errno(fr, NULL) ? fno.fattrib : -1;
}

int FAT_setAttr(const char* path, unsigned attr)
{
	FatVolume* vol = _fatVolumeFromPath(path);
	if (!vol) {
		errno = ENOSYS;
		return -1;
	}

	FRESULT fr = f_chmod(&vol->fs, _FAT_strip_device(path), attr, 0xff);

	return _FAT_set_errno(fr, NULL) ? 0 : -1;
}

//-----------------------------------------------------------------------------

DWORD get_fattime(void)
{
	time_t utc_time = time(NULL);
	if (utc_time == (time_t)-1) {
		return
			(DWORD)(FF_NORTC_YEAR - 1980) << 25 |
			(DWORD)FF_NORTC_MON << 21 |
			(DWORD)FF_NORTC_MDAY << 16;
	}

	return _FAT_make_fattime(&utc_time);
}

static _LOCK_T s_fatSystemLock;

int ff_mutex_create(FATFS* fs)
{
	if (fs) {
		__lock_init(_fatVolumeFromFatFs(fs)->lock);
	} else {
		__lock_init(s_fatSystemLock);
	}

	return 1;
}

void ff_mutex_delete(FATFS* fs)
{
	if (fs) {
		__lock_close(_fatVolumeFromFatFs(fs)->lock);
	} else {
		__lock_close(s_fatSystemLock);
	}
}

int ff_mutex_take(FATFS* fs)
{
	if (fs) {
		__lock_acquire(_fatVolumeFromFatFs(fs)->lock);
	} else {
		__lock_acquire(s_fatSystemLock);
	}

	return 1;
}

void ff_mutex_give(FATFS* fs)
{
	if (fs) {
		__lock_release(_fatVolumeFromFatFs(fs)->lock);
	} else {
		__lock_release(s_fatSystemLock);
	}
}

DSTATUS disk_initialize(void* pdrv)
{
	return disk_status(pdrv);
}

DSTATUS disk_status(void* pdrv)
{
	FatVolume* vol = (FatVolume*)pdrv;
	DvmDisc* disc = vol->disc;

	DSTATUS status = 0;
	if (!disc) {
		status |= STA_NOINIT;
	} else if (!(disc->features & FEATURE_MEDIUM_CANWRITE)) {
		status |= STA_PROTECT;
	}

	return status;
}

DRESULT disk_read(void* pdrv, BYTE* buff, LBA_t sector_, UINT count)
{
	FatVolume* vol = (FatVolume*)pdrv;
	DvmDisc* disc = vol->disc;
	sec_t sector = vol->start_sector + (sec_t)sector_;

	return disc->vt->read_sectors(disc, buff, sector, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(void* pdrv, const BYTE* buff, LBA_t sector_, UINT count)
{
	FatVolume* vol = (FatVolume*)pdrv;
	DvmDisc* disc = vol->disc;
	sec_t sector = vol->start_sector + (sec_t)sector_;

	return disc->vt->write_sectors(disc, buff, sector, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(void* pdrv, BYTE cmd, void* buff)
{
	FatVolume* vol = (FatVolume*)pdrv;
	DvmDisc* disc = vol->disc;

	switch (cmd) {
		default: {
			return RES_PARERR;
		}

		case CTRL_SYNC: {
			disc->vt->flush(disc);
			return RES_OK;
		}

		case GET_SECTOR_COUNT: {
			*(LBA_t*)buff = (LBA_t)vol->num_sectors;
			return ~vol->num_sectors ? RES_OK : RES_ERROR;
		}

		case GET_SECTOR_SIZE: {
			*(WORD*)buff = (WORD)disc->sector_sz;
			return RES_OK;
		}
	}
}
