/* Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Author: Barry Leslie
 *
 * 2010-02-05
 *
 * CORE SYSTEM:
 * Basic UNIX specific file I/O classes.
 *
 */

#include "CSConfig.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#include <signal.h>

#include "CSGlobal.h"
#include "CSDefs.h"
#include "CSStrUtil.h"
#include "CSSys.h"

#define CS_MASK		((S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP) | (S_IROTH))
//=====================
// CSSysFile
//=====================
bool CSSysFile::isDirNotFound(CSException *e) { return e->getErrorCode() == ENOENT; }
bool CSSysFile::isFileNotFound(CSException *e) { return e->getErrorCode() == ENOENT; }
bool CSSysFile::isDirExists(CSException *e) { return e->getErrorCode() == EEXIST; }

//--------------
void CSSysFile::sf_open(const char *path, bool readonly, bool create)
{
	int flags;
	
	flags = (readonly)?O_RDONLY:O_RDWR;
	
	if (create)
		flags |= O_CREAT;
		
	if (sf_fh != -1)
		sf_close();
	
	sf_path = CSString::newString(path);
	
	sf_fh = open(path, flags, CS_MASK);
	if (sf_fh == -1) {
		sf_path->release();
		sf_path = NULL;
		CSException::throwFileError(CS_CONTEXT, path, errno);
	}
}

//--------------
void CSSysFile::sf_close()
{
	if (sf_fh != -1) {
		close(sf_fh);
		sf_fh = -1;
		sf_path->release();
		sf_path = NULL;
	}	
}

//--------------
size_t CSSysFile::sf_pread(void *data, size_t size, off64_t offset)
{
	ssize_t read_size;
	
	read_size = pread(sf_fh, data, size, offset);
	if (read_size ==  -1)
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), errno);

	return read_size;
}

//--------------
void CSSysFile::sf_pwrite(const void *data, size_t size, off64_t offset)
{
	size_t write_size;
	
	write_size = pwrite(sf_fh, data, size, offset);
	if (write_size != size)
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), errno);

}

//--------------
void CSSysFile::sf_setEOF(off64_t offset)
{
	if (ftruncate(sf_fh, offset) == -1)
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), errno);
}

//--------------
off64_t CSSysFile::sf_getEOF()
{
	off64_t eof;

	if ((eof = lseek(sf_fh, 0, SEEK_END)) == ((off64_t)-1))
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), errno);

     return eof;
}

//--------------
void CSSysFile::sf_sync()
{
	fsync(sf_fh);
}

//--------------
void CSSysFile::sf_lock(bool shared)
{
	if (flock(sf_fh, (shared)?LOCK_SH:LOCK_EX) == -1)
		CSException::throwOSError(CS_CONTEXT, errno);
}

//--------------
void CSSysFile::sf_unlock()
{
	if (flock(sf_fh, LOCK_UN) == -1)
		CSException::throwOSError(CS_CONTEXT, errno);
}

//=====================
// CSSys
//=====================
//--------------
bool CSSys::sys_exists(const char *path)
{
	 return (access(path, F_OK) != -1);
}

//--------------
void CSSys::sys_makeDir(const char *path)
{
	char		super_path[PATH_MAX];
	struct stat	stats;
	char		*ptr;

	if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
		CSException::throwFileError(CS_CONTEXT, path, errno);

	// Set the access privileges.
	ptr = cs_last_name_of_path(path);
	if (ptr == path) 
		strcpy(super_path, ".");
	else {
		cs_strcpy(PATH_MAX, super_path, path);

		if ((ptr = cs_last_name_of_path(super_path)))
			*ptr = 0;
	}
	
	if (stat(super_path, &stats) == -1)
		CSException::throwFileError(CS_CONTEXT, path, errno);

	if (chmod(path, stats.st_mode) == -1)
		CSException::throwFileError(CS_CONTEXT, path, errno);

}

//--------------
void CSSys::sys_removeDir(const char *path)
{
	if (rmdir(path) == -1) {
		int err = errno;

		if (err != ENOENT)
			CSException::throwFileError(CS_CONTEXT, path, err);
	}
}

//--------------
void CSSys::sys_removeFile(const char *path)
{
	if (unlink(path) == -1) {
		int err = errno;

		if (err != ENOENT)
			CSException::throwFileError(CS_CONTEXT, path, err);
	}
}


//--------------

void CSSys::sys_stat(const char *path, bool *is_dir, off64_t *size, CSTime *mod_time)
{
	struct stat sb;

	if (stat(path, &sb) == -1)
		CSException::throwFileError(CS_CONTEXT, path, errno);
	if (is_dir)
		*is_dir = sb.st_mode & S_IFDIR;
	if (size)
		*size = sb.st_size;
	if (mod_time)
#ifdef __USE_MISC
		/* This is the Linux version: */
		mod_time->setUTC1970(sb.st_mtim.tv_sec, sb.st_mtim.tv_nsec);
#else
		/* This is the Mac OS X version: */
		mod_time->setUTC1970(sb.st_mtimespec.tv_sec, sb.st_mtimespec.tv_nsec);
#endif
}

//--------------
bool CSSys::sys_isLink(const char *path)
{
	struct stat sb;

	if (lstat(path, &sb) == -1)
		CSException::throwFileError(CS_CONTEXT, path, errno);
		
	return S_ISLNK(sb.st_mode);
}

//--------------
void CSSys::sys_rename(const char *old_path, const char *new_path)
{
	 if (rename(old_path, new_path) == -1)
		CSException::throwFileError(CS_CONTEXT, old_path, errno);
}

//--------------
void CSSys::sys_getcwd(char *path, size_t size)
{
	if (getcwd(path, size) == NULL)
		CSException::throwOSError(CS_CONTEXT, errno);
}

//--------------
void CSSys::sys_setcwd(const char *path)
{
	if (chdir(path) == -1)
		CSException::throwFileError(CS_CONTEXT, path, errno);
}

//--------------
uint32_t CSSys::sys_getpid()
{
	return getpid();
}

//--------------
bool CSSys::sys_isAlive(uint32_t pid)
{
	return (kill(pid, 0) == 0);
}

//=====================
// CSSysDir
//=====================
CSSysDir::~CSSysDir()
{
	close();
	if (sd_path)
		sd_path->release();
		
	if (sd_filter)
		sd_filter->release();
}

//--------------
void CSSysDir::open()
{
	enter_();
	if (!(sd_dir = opendir(sd_path->getCString())))
		CSException::throwFileError(CS_CONTEXT, sd_path->getCString(), errno);
	exit_();
}

//--------------
void CSSysDir::close()
{
	enter_();
	if (sd_dir) {
		closedir(sd_dir);
		sd_dir = NULL;
	}
	exit_();
}

//--------------
bool CSSysDir::next()
{
	int				err;
	struct dirent	*result;

	enter_();
	for (;;) {
		err = readdir_r(sd_dir, &sd_entry, &result);
		self->interrupted();
		if (err)
			CSException::throwFileError(CS_CONTEXT, sd_path->getCString(), err);
		if (!result)
			break;
		/* Filter out '.' and '..': */
		if (sd_entry.d_name[0] == '.') {
			if (sd_entry.d_name[1] == '.') {
				if (sd_entry.d_name[2] == '\0')
					continue;
			}
			else {
				if (sd_entry.d_name[1] == '\0')
					continue;
			}
		}
		break;
	}
	return_(result ? true : false);
}


//--------------
void CSSysDir::getEntryPath(char *path, size_t size)
{
	cs_strcpy(size, path, sd_path->getCString());
	cs_add_dir_char(size, path);
	cs_strcat(size, path, sd_entry.d_name);
}

//--------------
const char *CSSysDir::entryName()
{
	return sd_entry.d_name;
}

//--------------
bool CSSysDir::entryIsFile()
{
	if (sd_entry.d_type & DT_DIR)
		return false;
	return true;
}

//--------------
extern void unix_close(int h);
void unix_close(int h) {close(h);}

