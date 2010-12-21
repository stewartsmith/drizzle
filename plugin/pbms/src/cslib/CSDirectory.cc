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

#include "CSStrUtil.h"
#include "CSPath.h"
#include "CSDirectory.h"
#include "CSGlobal.h"

/*
 * ---------------------------------------------------------------
 * CORE SYSTEM DIRECTORY
 */

void CSDirectory::print(CSOutputStream *out)
{
	char	buffer[500];
	char	number[50];
	bool	is_dir;
	off64_t	size;
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

void CSDirectory::deleteEntry()
{
	char path[PATH_MAX];

	enter_();
	
	getEntryPath(path, PATH_MAX);
	
	CSPath *cs_path = CSPath::newPath(path);
	push_(cs_path);
	cs_path->removeFile();
	release_(cs_path);

	exit_();
}

const char *CSDirectory::name()
{
	return entryName();
}

bool CSDirectory::isFile()
{
	return entryIsFile();
}

off64_t CSDirectory::getSize()
{
	char path[PATH_MAX];

	getEntryPath(path, PATH_MAX);
	
	return CSPath::getSize(path);
}

void CSDirectory::info(bool *is_dir, off64_t *size, CSTime *mod_time)
{
	char path[PATH_MAX];

	getEntryPath(path, PATH_MAX);
	
	CSPath::info(path, is_dir, size, mod_time);
}

bool CSDirectory::exists()
{
	CSPath *path;
	bool yup;

	enter_();
	path = CSPath::newPath(RETAIN(sd_path));
	push_(path);
	yup = path->exists();
	release_(path);
	return_(yup);
}

CSDirectory *CSDirectory::newDirectory(CSString *path)
{
	CSDirectory *dir;

	if (!(dir = new CSDirectory())) {
		path->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	dir->sd_path = path;
	return dir;
}

CSDirectory *CSDirectory::newDirectory(CSPath *path)
{
	CSDirectory *dir;
	enter_();
	
	push_(path);
	dir = newDirectory(RETAIN(path->getString()));
	release_(path);
	return_(dir);
}

CSDirectory *CSDirectory::newDirectory(const char *path)
{
	return newDirectory(CSString::newString(path));
}


