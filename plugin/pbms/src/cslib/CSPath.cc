/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
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

#include <string.h>
#include <stdio.h>

#include "CSStrUtil.h"
#include "CSSys.h"
#include "CSFile.h"
#include "CSDirectory.h"
#include "CSPath.h"
#include "CSGlobal.h"

/*
 * ---------------------------------------------------------------
 * CORE SYSTEM PATH
 */
CSLock	CSPath::iRename_lock;

CSPath *CSPath::iCWD = NULL;

CSPath::~CSPath()
{
	if (iPath)
		iPath->release();
}

CSFile *CSPath::try_CreateAndOpen(CSThread *self, int mode, bool retry)
{
	volatile CSFile *fh = NULL;
	
	try_(a) {
		fh = openFile(mode | CSFile::CREATE); // success, do not try again.
	}
	catch_(a) {
		if (retry || !CSFile::isDirNotFound(&self->myException))
			throw_();

		/* Make sure the parent directory exists: */
		CSPath	*dir = CSPath::newPath(RETAIN(this), "..");
		push_(dir);
		dir->makePath();
		release_(dir);

	}
	cont_(a);
	return (CSFile *) fh; 
}

CSFile *CSPath::createFile(int mode)
{
	CSFile	*file = NULL;
	bool	retry = false;

	enter_();
	while (file == NULL) {
		file = try_CreateAndOpen(self, mode, retry);
		retry = true;
	}
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
	push_(path);
	
	path->makePath();
	makeDir();
	
	release_(path);

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
	bool link;
	enter_();

	link = sys_isLink(iPath->getCString());
	
	return_(link);
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
	push_(dir);
	
	if (dir->next())
		result = false;

	release_(dir);
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
	push_(dir);

	while (dir->next()) {
		path = CSPath::newPath(RETAIN(this), dir->name());
		push_(path);
		if (dir->isFile())
			path->remove();
		else
			path->removeDir();
		release_(path);
	}

	release_(dir);
	exit_();
}

void CSPath::emptyPath()
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
	push_(dir);

	while (dir->next()) {
		path = CSPath::newPath(RETAIN(this), dir->name());
		push_(path);
		if (dir->isFile())
			path->remove();
		else
			path->emptyPath();
		release_(path);
	}

	release_(dir);
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
	CSPath	*to_path = NULL;
	bool	is_dir;

	enter_();
	push_(in_to_path);
	
	if (!exists(NULL))
		CSException::throwFileError(CS_CONTEXT, iPath, ENOENT);

	if (in_to_path->exists(&is_dir)) {
		if (is_dir) {
			to_path = CSPath::newPath(RETAIN(in_to_path), getNameCString());
			push_(to_path);
			if (to_path->exists(NULL))
				CSException::throwFileError(CS_CONTEXT, to_path->getCString(), EEXIST);
			pop_(to_path);
		}
		else
			CSException::throwFileError(CS_CONTEXT, to_path->getCString(), ENOTDIR);
	} else
		to_path = RETAIN(in_to_path);
		
	move(to_path);

	release_(in_to_path);
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

off64_t CSPath::getSize(const char *path)
{
	off64_t size;

	info(path, NULL, &size, NULL);
	return size;
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

bool CSPath::exists(bool *is_dir)
{
	if (!sys_exists(iPath->getCString()))
		return false;
		
	if (is_dir)
		*is_dir = isDir();
	return true;
}

void CSPath::info(const char *path, bool *is_dir, off64_t *size, CSTime *mod_time)
{
	sys_stat(path, is_dir, size, mod_time);
	
}

void CSPath::info(bool *is_dir, off64_t *size, CSTime *mod_time)
{
	info(iPath->getCString(), is_dir, size, mod_time);
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
	sys_removeFile(iPath->getCString());
}

void CSPath::makeDir()
{
	char path[PATH_MAX];

	cs_strcpy(PATH_MAX, path, iPath->getCString());
	cs_remove_dir_char(path);

	sys_makeDir(path);
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
	sys_removeDir(iPath->getCString());
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
	
	lock_(&iRename_lock); // protect against race condition when testing if the new name exists yet or not.	
	if (sys_exists(new_path))
		CSException::throwFileError(CS_CONTEXT, new_path, EEXIST);
	
	tmp_path = CSString::newString(new_path);
	push_(tmp_path);
	
	sys_rename(iPath->getCString(), new_path);
		
	pop_(tmp_path);
	unlock_(&iRename_lock);
	
	old_path = iPath;
	iPath = tmp_path;
	
	old_path->release();
	exit_();
}

void CSPath::move(CSPath *to_path)
{
	enter_();
	push_(to_path);
	lock_(&iRename_lock); // protect against race condition when testing if the new name exists yet or not.	
	if (to_path->exists())
		CSException::throwFileError(CS_CONTEXT, to_path->getCString(), EEXIST);
		
	/* Cannot move from TD to non-TD: */
	sys_rename(iPath->getCString(), to_path->getCString());
	unlock_(&iRename_lock);
	release_(to_path);
	exit_();
}

CSPath *CSPath::getCWD()
{
	char path[PATH_MAX];

	sys_getcwd(path, PATH_MAX);
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

