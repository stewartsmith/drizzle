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

#ifndef __CSDIRECTORY_H__
#define __CSDIRECTORY_H__

#include <dirent.h>

#include "CSDefs.h"
#include "CSPath.h"
#include "CSTime.h"
#include "CSObject.h"
#include "CSStream.h"

using namespace std;

class CSDirectory : public CSObject {
public:
	CSDirectory(): iPath(NULL), iDir(NULL) { }

	virtual ~CSDirectory();

	virtual void open();

	virtual void close();

	virtual bool next();

	virtual const char *name();

	virtual bool isFile() ;
	
	virtual void deleteEntry() ;

	virtual void info(bool *is_dir, off_t *size, CSTime *mod_time);

	virtual void print(CSOutputStream *out);

	friend class TDDirectory;

	static CSDirectory *newDirectory(CSPath *);
	static CSDirectory *newDirectory(CSString *);

private:
	struct dirent iEntry;
	DIR *iDir;
	CSString *iPath;
};


#endif
