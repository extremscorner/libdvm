#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/iosupport.h>
#include <calico/nds/env.h>
#include <fat.h>
#include <filesystem.h>

static int _nitroFS_open_r(struct _reent*, void*, const char*, int, int);
static int _nitroFS_close_r(struct _reent*, void*);
static ssize_t _nitroFS_read_r(struct _reent*, void*, char*, size_t);
static off_t _nitroFS_seek_r(struct _reent*, void*, off_t, int);
static int _nitroFS_fstat_r(struct _reent*, void*, struct stat*);
static int _nitroFS_stat_r(struct _reent*, const char*, struct stat*);
static int _nitroFS_chdir_r(struct _reent*, const char*);
static DIR_ITER* _nitroFS_diropen_r(struct _reent*, DIR_ITER*, const char*);
static int _nitroFS_dirreset_r(struct _reent*, DIR_ITER*);
static int _nitroFS_dirnext_r(struct _reent*, DIR_ITER*, char*, struct stat*);
static int _nitroFS_dirclose_r(struct _reent*, DIR_ITER*);

typedef struct _nitroFS_file {
	u32 pos;
	u16 file_id;
} _nitroFS_file;

typedef struct _nitroFS_dir {
	NitroRomIter it;
	u16 dir_id;
	u8 it_pos;
	NitroRomIterEntry ent;
} _nitroFS_dir;

static u16 _nitroFS_cwd;

static devoptab_t _nitroFS_devoptab = {
	.name         = "nitro",
	.structSize   = sizeof(_nitroFS_file),
	.open_r       = _nitroFS_open_r,
	.close_r      = _nitroFS_close_r,
	.read_r       = _nitroFS_read_r,
	.seek_r       = _nitroFS_seek_r,
	.fstat_r      = _nitroFS_fstat_r,
	.stat_r       = _nitroFS_stat_r,
	.chdir_r      = _nitroFS_chdir_r,
	.dirStateSize = sizeof(_nitroFS_dir),
	.diropen_r    = _nitroFS_diropen_r,
	.dirreset_r   = _nitroFS_dirreset_r,
	.dirnext_r    = _nitroFS_dirnext_r,
	.dirclose_r   = _nitroFS_dirclose_r,
};

static inline const char* _nitroFS_strip_device(const char* path)
{
	char* colonpos = strchr(path, ':');
	return colonpos ? &colonpos[1] : path;
}

static int _nitroFS_set_stat(NitroRom* nr, u16 id, struct stat* st)
{
	memset(st, 0, sizeof(*st));

	st->st_ino = id;
	st->st_mode = S_IRUSR | S_IRGRP | S_IROTH;
	st->st_nlink = 1;
	st->st_uid   = 1;
	st->st_gid   = 2;
	st->st_rdev  = st->st_dev;

	if (id >= NITROROM_ROOT_DIR) {
		st->st_mode |= S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	} else {
		st->st_mode |= S_IFREG;
		st->st_size = nitroromGetFileSize(nr, id);
	}

	return 0;
}

bool nitroFSInit(char** basepath)
{
	fatInitDefault();

	bool ok = false;
	NitroRom* nr = nitroromGetSelf();
	if (nr) {
		ok = nitroFSMount(nr);
	}

	if (basepath) {
		*basepath = ok ? g_envNdsArgvHeader->argv[0] : NULL;
	}

	return ok;
}

bool nitroFSMount(NitroRom* nr)
{
	_nitroFS_devoptab.deviceData = nr;
	_nitroFS_cwd = NITROROM_ROOT_DIR;
	return AddDevice(&_nitroFS_devoptab) >= 0;
}

int _nitroFS_open_r(struct _reent* r, void* fd, const char* path, int flags, int mode)
{
	if ((flags & O_ACCMODE) != O_RDONLY) {
		r->_errno = EROFS;
		return -1;
	}

	NitroRom* nr = r->deviceData;
	int file_id = nitroromResolvePath(nr, _nitroFS_cwd, _nitroFS_strip_device(path));
	if (file_id < 0) {
		r->_errno = ENOENT;
		return -1;
	}

	_nitroFS_file* fil = fd;
	fil->file_id = file_id;
	fil->pos = 0;
	return 0;
}

int _nitroFS_close_r(struct _reent* r, void* fd)
{
	return 0;
}

ssize_t _nitroFS_read_r(struct _reent* r, void* fd, char* buf, size_t len)
{
	NitroRom* nr = r->deviceData;
	_nitroFS_file* fil = fd;
	size_t max_read = nitroromGetFileSize(nr, fil->file_id) - fil->pos;
	len = len < max_read ? len : max_read;

	if (len && !nitroromReadFile(nr, fil->file_id, fil->pos, buf, len)) {
		r->_errno = EIO;
		return -1;
	}

	fil->pos += len;
	return len;
}

off_t _nitroFS_seek_r(struct _reent* r, void* fd, off_t offset, int whence)
{
	NitroRom* nr = r->deviceData;
	_nitroFS_file* fil = fd;

	u32 pos;
	u32 fil_sz = nitroromGetFileSize(nr, fil->file_id);
	switch (whence) {
		default:
			r->_errno = EINVAL;
			return -1;

		case SEEK_SET:
			pos = 0;
			break;

		case SEEK_CUR:
			pos = fil->pos;
			break;

		case SEEK_END:
			pos = fil_sz;
			break;
	}

	if (offset >= 0 || pos >= (-offset)) {
		// Apply new offset
		pos += offset;
		pos = pos < fil_sz ? pos : fil_sz;
		fil->pos = pos;
	} else {
		// Attempted to seek before the beginning of the file
		r->_errno = EINVAL;
		return -1;
	}

	return fil->pos;
}

int _nitroFS_fstat_r(struct _reent* r, void* fd, struct stat* st)
{
	NitroRom* nr = r->deviceData;
	_nitroFS_file* fil = fd;
	return _nitroFS_set_stat(nr, fil->file_id, st);
}

int _nitroFS_stat_r(struct _reent* r, const char* path, struct stat* st)
{
	NitroRom* nr = r->deviceData;
	int file_id = nitroromResolvePath(nr, _nitroFS_cwd, _nitroFS_strip_device(path));
	if (file_id < 0) {
		r->_errno = ENOENT;
		return -1;
	}

	return _nitroFS_set_stat(nr, file_id, st);
}

int _nitroFS_chdir_r(struct _reent* r, const char* path)
{
	NitroRom* nr = r->deviceData;
	int dir_id = nitroromResolvePath(nr, _nitroFS_cwd, _nitroFS_strip_device(path));
	if (dir_id < NITROROM_ROOT_DIR) {
		r->_errno = (dir_id >= 0) ? ENOTDIR : ENOENT;
		return -1;
	}

	_nitroFS_cwd = dir_id;
	return 0;
}

DIR_ITER* _nitroFS_diropen_r(struct _reent* r, DIR_ITER* it, const char* path)
{
	NitroRom* nr = r->deviceData;
	int dir_id = nitroromResolvePath(nr, _nitroFS_cwd, _nitroFS_strip_device(path));
	if (dir_id < NITROROM_ROOT_DIR) {
		r->_errno = (dir_id >= 0) ? ENOTDIR : ENOENT;
		return NULL;
	}

	_nitroFS_dir* d = it->dirStruct;
	nitroromOpenIter(nr, dir_id, &d->it);
	d->dir_id = 0;
	d->it_pos = 0;
	return it;
}

int _nitroFS_dirreset_r(struct _reent* r, DIR_ITER* it)
{
	_nitroFS_dir* d = it->dirStruct;
	nitroromRewindIter(&d->it);
	d->it_pos = 0;
	return 0;
}

int _nitroFS_dirnext_r(struct _reent* r, DIR_ITER* it, char* filename_buf, struct stat* st)
{
	_nitroFS_dir* d = it->dirStruct;

	// Handle fake entry for current directory (.)
	if (d->it_pos == 0) {
		d->it_pos = d->dir_id > NITROROM_ROOT_DIR ? 1 : 2; // skip parent dir on root
		filename_buf[0] = '.';
		filename_buf[1] = 0;
		return _nitroFS_set_stat(d->it.nr, d->dir_id, st);
	}

	// Handle fake entry for parent directory (..)
	if (d->it_pos == 1) {
		d->it_pos = 2;
		filename_buf[0] = '.';
		filename_buf[1] = '.';
		filename_buf[2] = 0;
		return _nitroFS_set_stat(d->it.nr, nitroromGetParentDir(d->it.nr, d->dir_id), st);
	}

	// Handle real entries
	if (nitroromReadIter(&d->it, &d->ent)) {
		memcpy(filename_buf, d->ent.name, d->ent.name_len+1);
		return _nitroFS_set_stat(d->it.nr, d->ent.id, st);
	}

	r->_errno = ENOENT;
	return -1;
}

int _nitroFS_dirclose_r(struct _reent* r, DIR_ITER* it)
{
	return 0;
}
