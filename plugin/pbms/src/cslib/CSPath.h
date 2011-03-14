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

#pragma once
#ifndef __CSPATH_H__
#define __CSPATH_H__

#ifdef OS_UNIX
#include <limits.h>
#endif

#include "CSDefs.h"
#include "CSTime.h"
#include "CSDefs.h"
#include "CSString.h"
#include "CSSys.h"

class CSFile;
class CSDirectory;

class CSPath : public CSRefObject, public CSSys {
public:
	virtual CSFile *createFile(int mode);

	virtual void copyFile(CSPath *to_file, bool overwrite);

	/*
	 * Recursively creates as many directories as required.
	 */
	virtual void makePath();

	virtual void copyDir(CSPath *to_dir, bool overwrite);

	/* Return true of the directory is a symbolic link. */
	virtual bool isLink();

	/* Return true of the directory is empty. */
	virtual bool isEmpty();

	/* Delete the contents of a directory */
	virtual void emptyDir();
	
	/* Recursively delete the contents of a directory */
	virtual void emptyPath();
	
	/* Copy a file or directory to the specified location. */
	virtual void copyTo(CSPath *to_path, bool overwrite);

	virtual void moveTo(CSPath *to_path);

	/*
	 * Remove a file or directory (even if not empty).
	 */
	virtual void remove();

	/* Create an empty file. */
	virtual void touch(bool create_path = false);

	virtual CSString *getString();

	virtual const char *getCString();

	virtual const char *getNameCString();

	//virtual CSPath *clone() const;

	friend class TDPath;
	
	virtual bool exists(bool *is_dir);

	virtual bool exists() { return exists(NULL); }

	static void info(const char *path, bool *is_dir, off64_t *size, CSTime *mod_time);

	virtual void info(bool *is_dir, off64_t *size, CSTime *mod_time);

	static off64_t getSize(const char *path);
	
 	virtual off64_t getSize();

	virtual bool isDir();

	virtual CSFile *openFile(int mode);

	/*
	 * Remove a file.
	 */
	virtual void removeFile();

	/*
	 * Creates a directory assuming the directory path exists.
	 */
	virtual void makeDir();

	virtual CSDirectory *openDirectory();

	virtual void removeDir();

	virtual void rename(const char *name);

	/*
	 * Renames one path to another.
	 * The destination path may not exist.
	 */
	virtual void move(CSPath *to_path);
	
	CSPath *getCWD();

	static CSPath *getSystemCWD();

	/* Create a new path given an absolute and a relative path: */
	static CSPath *newPath(const char *path);
	static CSPath *newPath(CSString *path);

	/* Create a path relative to the given 'cwd' */
	static CSPath *newPath(CSPath *cwd, const char *path);
	static CSPath *newPath(CSString *cwd, const char *path);
	static CSPath *newPath(const char *cwd, CSString *path);
	static CSPath *newPath(const char *cwd, const char *path);

private:
	CSFile *try_CreateAndOpen(CSThread *self, int mode, bool retry);
	static CSLock iRename_lock;
	CSPath():iPath(NULL) { }

	virtual ~CSPath();

	CSString *iPath;

	static CSPath *iCWD;
};

#endif
