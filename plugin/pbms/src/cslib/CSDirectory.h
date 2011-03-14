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

#pragma once
#ifndef __CSDIRECTORY_H__
#define __CSDIRECTORY_H__

#include "CSDefs.h"
#include "CSSys.h"
#include "CSPath.h"
#include "CSTime.h"
#include "CSObject.h"
#include "CSStream.h"

class CSDirectory : public CSSysDir, public CSObject  {
public:
	const char *name();

	bool isFile();
	
	off64_t getSize();
	
	void deleteEntry();

	void info(bool *is_dir, off64_t *size, CSTime *mod_time);

	bool exists();

	void print(CSOutputStream *out);

	friend class TDDirectory;

	static CSDirectory *newDirectory(CSPath *path);
	static CSDirectory *newDirectory(CSString *path);
	static CSDirectory *newDirectory(const char *path);

};


#endif
