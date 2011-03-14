#pragma once
#ifndef __SYSTAB_UTIL_H__
#define __SYSTAB_UTIL_H__
/* Copyright (C) 2009 PrimeBase Technologies GmbH, Germany
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
 *  Created by Barry Leslie on 3/20/09.
 *
 */

#include "cslib/CSLog.h"


class SysTabRec: public CSRefStringBuffer {	
	void logError(const char *text = NULL);

	bool findRecord();

	bool badRecord;
	
	uint32_t recordLength, start_of_record;
	
	public:
	const char *db_name, *file_name, *table_name;
	char *ptr, *end_of_record, *end_of_data;

	SysTabRec(const char *db_name_arg, const char *file_name_arg, const char *table_name_arg): 
		CSRefStringBuffer(64),
		badRecord(false),
		recordLength(0),
		start_of_record(0),
		db_name(db_name_arg),
		file_name(file_name_arg),
		table_name(table_name_arg),
		ptr(NULL),
		end_of_record(NULL),
		end_of_data(NULL)
	{
	}
	
	// Methods used to read records
	bool firstRecord();
	bool nextRecord();
	void resetRecord();
		
	uint8_t getInt1Field();	
	uint32_t getInt4Field();	
	const char *getStringField();
		
	bool isValidRecord()
	{
		return ((ptr == end_of_record) && !badRecord);
	}
	
	// Methods used to write records
	void clear();
	void beginRecord();
	void endRecord();
	void setInt1Field(uint8_t val);	
	void setInt4Field(uint32_t val);	
	void setStringField(const char *val);
	void setStringField(const char *val, uint32_t len);

	
};

CSString *getPBMSPath(CSString *db_path);
CSPath *getSysFile(CSString *db_path, const char *name, size_t min_size);

#endif // __SYSTAB_UTIL_H__

