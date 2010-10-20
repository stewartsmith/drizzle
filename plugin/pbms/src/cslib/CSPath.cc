/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-06-07
 *
 * CORE SYSTEM:
 * Basic file system path.
 *
 */

#include "CSConfig.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include <stdio.h>

static int unix_file_rename(const char *from, const char *to)
{
	return rename(from, to);
}

#include "CSStrUtil.h"
#include "CSFile.h"
#include "CSDirectory.h"
#include "CSPath.h"
#include "CSGlobal.h"

/*
 * ---------------------------------------------------------------
 * CORE SYSTEM PATH
 */

CSPath *CSPath::iCWD = NULL;

CSPath::~CSPath()
{
	if (iPath)
		iPath->release();
}

CSFile *CSPath::createFile(int mode)
{
	CSFile	*file;
	CSPath	*dir;
	bool	retry = false;

	CLOBBER_PROTECT(retry);

	enter_();
	/* Create and open the file: */
	do {
		try_(a) {
			file = openFile(mode | CSFile::CREATE);
			retry = false;
		}
		catch_(a) {
			if (retry || !CSFile::isDirNotFound(&self->myException))
				throw_();

			/* Make sure the parent directory exists: */
			dir = CSPath::newPath(RETAIN(this), "..");
			try_(b) {
				dir->makePath();
			}
			finally_(b) {
				dir->release();
			}
			finally_end_block(b);

			retry = true;
		}
		cont_(a);
	}
	while (retry);
	return_(file);
}

/* Copy the contents of one file to another.
 * The destination need not exist.
 * If the destination exists, it will be
 * overwritten if 'overwrite' is true.
 */
void CSPath::copyFile(CSPath *in_to_file, bool overwrite)
{
	CSPath			*to_file = in_to_file;
	bool			is_dir;
	CSFile			*infile, *outfile;

	enter_();
	push_(in_to_file);
	
	if (to_file->exists(&is_dir)) {
		if (is_dir) {
			to_file = CSPath::newPath(RETAIN(in_to_file), getNameCString());
			push_(to_file);
			
			if (to_file->exists(&is_dir)) {	
				if (!overwrite)
					CSException::throwFileError(CS_CONTEXT, to_file->getCString(), EEXIST);
				to_file->remove();
			}
		}
		else if (!overwrite)
			CSException::throwFileError(CS_CONTEXT, to_file->getCString(), EEXIST);
	}

	infile = openFile(CSFile::READONLY);
	push_(infile);

	outfile = to_file->createFile(CSFile::TRUNCATE);
	push_(outfile);
	
	CSStream::pipe(outfile->getOutputStream(), infile->getInputStream());
	release_(outfile);
	release_(infile);
	
	if (to_file != in_to_file)
		release_(to_file);
		
	release_(in_to_file);
	exit_();
}

void CSPath::makePath()
{
	CSPath	*path;
	bool	is_dir;

	enter_();
	if (iPath->length() <= 1)
		exit_();

	if (exists(&is_dir)) {
		if (!is_dir)
			CSException::throwFileError(CS_CONTEXT, iPath, EEXIST);
		exit_();
	}

	path = CSPath::newPath(RETAIN(this), "..");
	try_(a) {
		path->makePath();
		makeDir();
	}
	finally_(a) {
		path->release();
	}
	finally_end_block(a);
	exit_();
}

/* Copy the contents of one directory to another.
 * The destination must be a directory.
 * The new source directory will be copied to
 * a directory of the same name in the destination.
 */
void CSPath::copyDir(CSPath *in_to_dir, bool overwrite)
{
	CSPath		*to_dir = in_to_dir;
	bool		is_dir;
	CSDirectory *dir = NULL;
	CSPath		*path = NULL;

	enter_();
	push_(in_to_dir);
	
	if (to_dir->exists(&is_dir)) {
		if (!is_dir)
			CSException::throwFileError(CS_CONTEXT, to_dir->getCString(), ENOTDIR);
		/* Add the source directory name to the destination: */
		to_dir = CSPath::newPath(RETAIN(in_to_dir), getNameCString());
		push_(to_dir);
		
		if (to_dir->exists(&is_dir)) {
			if (!overwrite)
				CSException::throwFileError(CS_CONTEXT, to_dir->getCString(), EEXIST);
			to_dir->remove();
		}
	}
 
	/* Make the directory and copy the contents of the source
	 * into it:
	 */
	to_dir->makePath();

	dir = openDirectory();
	push_(dir);
	while (dir->next()) {
		self->interrupted();
		path = CSPath::newPath(RETAIN(this), dir->name());
		push_(path);
		if (dir->isFile())
			path->copyFile(RETAIN(to_dir), overwrite);
		else
			path->copyDir(RETAIN(to_dir), overwrite);
		release_(path);
	}
	release_(dir);
	
	if (to_dir != in_to_dir)
		release_(to_dir);
		
	release_(in_to_dir);
	
	exit_();
}


bool CSPath::isLink()
{
	struct stat sb;
	enter_();

	if (lstat(iPath->getCString(), &sb) == -1)
		CSException::throwFileError(CS_CONTEXT, iPath, errno);
		
	return_(S_ISLNK(sb.st_mode));
}

bool CSPath::isEmpty()
{
	CSDirectory *dir = NULL;
	bool		is_dir, result = true;

	enter_();
	if (!exists(&is_dir))
		return_(true);

	if (!is_dir)
		return_(false);

	dir = openDirectory();
	try_(a) {
		if (dir->next())
			result = false;
	}
	finally_(a) {
		dir->release();	
	}
	finally_end_block(a);
	return_(result);
}

void CSPath::emptyDir()
{
	CSDirectory *dir;
	CSPath		*path = NULL;
	bool		is_dir;

	enter_();
	if (!exists(&is_dir))
		exit_();

	if (!is_dir)
		CSException::throwFileError(CS_CONTEXT, iPath, ENOTDIR);

	dir = openDirectory();
	try_(a) {
		while (dir->next()) {
			path = CSPath::newPath(RETAIN(this), dir->name());
			if (dir->isFile())
				path->remove();
			else
				path->removeDir();
			path->release();
			path = NULL;
		}
	}
	finally_(a) {
		if (path)
			path->release();
		dir->release();	
	}
	finally_end_block(a);
	exit_();
}

void CSPath::copyTo(CSPath *to_path, bool overwrite)
{
	bool is_dir;

	enter_();
	push_(to_path);
	if (!exists(&is_dir))
		CSException::throwFileError(CS_CONTEXT, iPath, ENOENT);

	pop_(to_path);
	if (is_dir)
		/* The source is a directory. */
		copyDir(to_path, overwrite);
	else {
		/* The source is not a directory: */
		copyFile(to_path, overwrite);
	}

	exit_();
}

void CSPath::moveTo(CSPath *in_to_path)
{
	CSPath	*to_path = in_to_path;
	bool	is_dir;

	enter_();
	if (!exists(NULL))
		CSException::throwFileError(CS_CONTEXT, iPath, ENOENT);

	try_(a) {
		if (to_path->exists(&is_dir)) {
			if (is_dir) {
			 	to_path = CSPath::newPath(RETAIN(in_to_path), getNameCString());
			 	if (to_path->exists(NULL))
					CSException::throwFileError(CS_CONTEXT, to_path->getCString(), EEXIST);
			}
			else
				CSException::throwFileError(CS_CONTEXT, to_path->getCString(), ENOTDIR);
		}
		move(to_path);
	}
	finally_(a) {
		if (to_path != in_to_path)
			to_path->release();
	}
	finally_end_block(a);

	exit_();
}

void CSPath::remove()
{
	bool is_dir;

	if (exists(&is_dir)) {
		if (is_dir) {
			emptyDir();
			removeDir();
		}
		else
			removeFile();
	}
}


void CSPath::touch(bool create_path)
{
	CSFile *file;

	enter_();
	if (create_path)
		file = createFile(CSFile::READONLY);
	else
		file = openFile(CSFile::READONLY | CSFile::CREATE);
	file->release();
	exit_();
}

CSString *CSPath::getString()
{ 
	return iPath;
}

const char *CSPath::getCString()
{
	return iPath->getCString();
}

const char *CSPath::getNameCString()
{
	const char *str = getCString();
	
	return cs_last_name_of_path(str);
}

off64_t CSPath::getSize()
{
	off64_t size;

	info((bool *) NULL, &size, (CSTime *) NULL);
	return size;
}

bool CSPath::isDir()
{
	bool	is_dir;

	info(&is_dir, (off64_t *) NULL, (CSTime *) NULL);
	return is_dir;
}

/*
 * ---------------------------------------------------------------
 * A UNIX PATH
 */

bool CSPath::exists(bool *is_dir)
{
	int err;

	err = access(iPath->getCString(), F_OK);
	if (err == -1)
		return false;
	if (is_dir)
		*is_dir = isDir();
	return true;
}

void CSPath::info(bool *is_dir, off64_t *size, CSTime *mod_time)
{
	struct stat sb;

	if (stat(iPath->getCString(), &sb) == -1)
		CSException::throwFileError(CS_CONTEXT, iPath, errno);
	if (is_dir)
		*is_dir = sb.st_mode & S_IFDIR;
	if (size)
		*size = sb.st_size;
	if (mod_time)
#ifdef __USE_MISC
		/* This is the Linux version: */
		mod_time->setUTC1970(sb.st_mtim.tv_sec, sb.st_mtim.tv_nsec);
#else
		mod_time->setUTC1970(sb.st_mtime, 0);
#endif
}

CSFile *CSPath::openFile(int mode)
{
	CSFile *file;

	enter_();
	file = CSFile::newFile(RETAIN(this));
	push_(file);
	file->open(mode);
	pop_(file);
	return_(file);
}

void CSPath::removeFile()
{
	if (unlink(iPath->getCString()) == -1) {
		int err = errno;

		if (err != ENOENT)
			CSException::throwFileError(CS_CONTEXT, iPath, err);
	}
}

static void cs_set_stats(char *path)
{
	char		super_path[PATH_MAX];
	struct stat	stats;
	char		*ptr;

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

	/*chown(path, stats.st_uid, stats.st_gid);*/
}

void CSPath::makeDir()
{
	char path[PATH_MAX];

	cs_strcpy(PATH_MAX, path, iPath->getCString());
	cs_remove_dir_char(path);

	if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
		CSException::throwFileError(CS_CONTEXT, iPath, errno);

	cs_set_stats(path);
}

CSDirectory *CSPath::openDirectory()
{
	CSDirectory *dir;

	enter_();
	dir = CSDirectory::newDirectory(RETAIN(this));
	push_(dir);
	dir->open();
	pop_(dir);
	return_(dir);
}

void CSPath::removeDir()
{			
	emptyDir();
	if (rmdir(iPath->getCString()) == -1) {
		int err = errno;

		if (err != ENOENT)
			CSException::throwFileError(CS_CONTEXT, iPath, err);
	}
}

void CSPath::rename(const char *name)
{
	char new_path[PATH_MAX];
	CSString *tmp_path, *old_path = iPath;
	
	enter_();
	
	cs_strcpy(PATH_MAX, new_path, iPath->getCString());
	cs_remove_last_name_of_path(new_path);
	cs_add_dir_char(PATH_MAX, new_path);
	cs_strcat(PATH_MAX, new_path, name);
	
	tmp_path = CSString::newString(new_path);
	push_(tmp_path);
	
	if (unix_file_rename(iPath->getCString(), new_path) == -1)
		CSException::throwFileError(CS_CONTEXT, iPath, errno);
		
	pop_(tmp_path);
	old_path = iPath;
	iPath = tmp_path;
	
	old_path->release();
	exit_();
}

void CSPath::move(CSPath *to_path)
{
	/* Cannot move from TD to non-TD: */
	if (unix_file_rename(iPath->getCString(), to_path->getCString()) == -1)
		CSException::throwFileError(CS_CONTEXT, iPath, errno);
}

CSPath *CSPath::getCWD()
{
	char path[MAXPATHLEN];

	if (!getcwd(path, MAXPATHLEN))
		CSException::throwOSError(CS_CONTEXT, errno);

	return newPath(path);
}

CSPath *CSPath::newPath(const char *path)
{
	if (!path)
		CSException::throwAssertion(CS_CONTEXT, "Initial string may not be NULL");
	return newPath(CSString::newString(path));
}

CSPath *CSPath::newPath(CSString *path)
{
	CSPath *p;

	enter_();
	push_(path);
	new_(p, CSPath());

	/* Adjust the path string so that it does not have
	 * a terminating CS_DIR_CHAR:
	 */
	if (path->endsWith(CS_DIR_DELIM) && path->length() > 1) {
		p->iPath = path->left(CS_DIR_DELIM, -1);
		path->release();
	}
	else
		p->iPath = path;
	pop_(path);
	return_(p);
}

CSPath *CSPath::newPath(CSPath *cwd, const char *path)
{
	char abs_path[PATH_MAX];

	enter_();
	cs_make_absolute_path(PATH_MAX, abs_path, path, cwd->getCString());
	cwd->release();
	CSPath *p = newPath(abs_path);
	return_(p);
}

CSPath *CSPath::newPath(CSString *cwd, const char *path)
{
	char abs_path[PATH_MAX];

	enter_();
	cs_make_absolute_path(PATH_MAX, abs_path, path, cwd->getCString());
	cwd->release();
	CSPath *p = newPath(abs_path);
	return_(p);
}

CSPath *CSPath::newPath(const char *cwd, CSString *path)
{
	char abs_path[PATH_MAX];

	enter_();
	cs_make_absolute_path(PATH_MAX, abs_path, path->getCString(), cwd);
	path->release();
	CSPath *p = newPath(abs_path);
	return_(p);
}

CSPath *CSPath::newPath(const char *cwd, const char *path)
{
	char abs_path[PATH_MAX];

	enter_();
	cs_make_absolute_path(PATH_MAX, abs_path, path, cwd);
	CSPath *p = newPath(abs_path);
	return_(p);
}

