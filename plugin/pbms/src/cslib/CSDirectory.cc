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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-06-07
 *
 * CORE SYSTEM:
 * A basic directory.
 *
 */

#include "CSConfig.h"

#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>

#include "CSStrUtil.h"
#include "CSPath.h"
#include "CSDirectory.h"
#include "CSGlobal.h"

/*
 * ---------------------------------------------------------------
 * CORE SYSTEM DIRECTORY
 */

CSDirectory::~CSDirectory()
{
	enter_();
	close();
	if (iPath)
		iPath->release();
	exit_();
}

void CSDirectory::print(CSOutputStream *out)
{
	char	buffer[500];
	char	number[50];
	bool	is_dir;
	off_t	size;
	CSTime	mod_time;
	char	*str_time;

	while (next()) {
		info(&is_dir, &size, &mod_time); 
		if (is_dir)
			cs_strcpy(500, buffer, "D");
		else
			cs_strcpy(500, buffer, "f");
		snprintf(number, 50, "%8llu ", (unsigned long long) size);
		cs_strcat(500, buffer, number);
		str_time = mod_time.getCString();
		cs_strcat(500, buffer, str_time);
		cs_strcat(500, buffer, " ");
		cs_strcat(500, buffer, name());
		out->printLine(buffer);
	}
}

/*
 * ---------------------------------------------------------------
 * UNIX DIRECTORY
 */

/*
 * The filter may contain one '*' as wildcard.
 */
void CSDirectory::open()
{
	enter_();
	if (!(iDir = opendir(iPath->getCString())))
		CSException::throwFileError(CS_CONTEXT, iPath->getCString(), errno);
	exit_();
}

void CSDirectory::close()
{
	enter_();
	if (iDir) {
		closedir(iDir);
		iDir = NULL;
	}
	exit_();
}

bool CSDirectory::next()
{
	int				err;
	struct dirent	*result;

	enter_();
	for (;;) {
		err = readdir_r(iDir, &iEntry, &result);
		self->interrupted();
		if (err)
			CSException::throwFileError(CS_CONTEXT, iPath->getCString(), err);
		if (!result)
			break;
		/* Filter out '.' and '..': */
		if (iEntry.d_name[0] == '.') {
			if (iEntry.d_name[1] == '.') {
				if (iEntry.d_name[2] == '\0')
					continue;
			}
			else {
				if (iEntry.d_name[1] == '\0')
					continue;
			}
		}
		break;
	}
	return_(result ? true : false);
}

void CSDirectory::deleteEntry()
{
	char path[PATH_MAX];

	enter_();
	cs_strcpy(PATH_MAX, path, iPath->getCString());
	cs_add_dir_char(PATH_MAX, path);
	cs_strcat(PATH_MAX, path, iEntry.d_name);

	CSPath *cs_path = CSPath::newPath(path);
	push_(cs_path);
	cs_path->removeFile();
	release_(cs_path);

	exit_();
}

const char *CSDirectory::name()
{
	return iEntry.d_name;
}

bool CSDirectory::isFile()
{
	if (iEntry.d_type & DT_DIR)
		return false;
	return true;
}

void CSDirectory::info(bool *is_dir, off_t *size, CSTime *mod_time)
{
	char path[PATH_MAX];

	enter_();
	cs_strcpy(PATH_MAX, path, iPath->getCString());
	cs_add_dir_char(PATH_MAX, path);
	cs_strcat(PATH_MAX, path, iEntry.d_name);

	CSPath *cs_path = CSPath::newPath(path);
	push_(cs_path);
	cs_path->info(is_dir, size, mod_time);
	release_(cs_path);

	exit_();
}

CSDirectory *CSDirectory::newDirectory(CSPath *path)
{
	CSDirectory *dir;

	if (!(dir = new CSDirectory())) {
		path->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	dir->iPath = path->getString();
	dir->iPath->retain();
	path->release();
	return dir;
}

CSDirectory *CSDirectory::newDirectory(CSString *path)
{
	CSDirectory *dir;

	if (!(dir = new CSDirectory())) {
		path->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	dir->iPath = path;
	return dir;
}


