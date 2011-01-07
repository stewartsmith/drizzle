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
 * Barry Leslie
 *
 * System table utility functions.
 *
 */
#include "cslib/CSConfig.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSPath.h"

#include "systab_util_ms.h"

//======================
void SysTabRec::logError(const char *text)
{
	char msg[80];
	enter_();
	
	snprintf(msg, 80, ", damaged at or near position %"PRIu32" \n", (uint32_t) (ptr - getBuffer(0)));
	CSL.log(self, CSLog::Warning, db_name);
	CSL.log(self, CSLog::Warning, ".");
	CSL.log(self, CSLog::Warning, table_name);
	CSL.log(self, CSLog::Warning, " table file ");
	CSL.log(self, CSLog::Warning, file_name);
	if (text) {
		CSL.log(self, CSLog::Warning, " ");
		CSL.log(self, CSLog::Warning, text);
	}
	CSL.log(self, CSLog::Warning, msg);
	exit_();
}

#define HEAD_MARKER ((uint32_t)0XABABABAB)
#define TAIL_MARKER ((uint32_t)0XCDCDCDCD)
//---------------------
bool SysTabRec::findRecord()
{
	uint32_t len, marker;
	CSDiskData d;

	badRecord = true;
	
	while (badRecord) {
		len = end_of_data - ptr;
		if (len < 12)
			return false;
		
		// Look for the record header.
		d.rec_chars = ptr;	ptr +=4;	
		marker = CS_GET_DISK_4(d.int_val->val_4);
		if (marker != HEAD_MARKER) 
			continue;
			
		// Get the record length.
		d.rec_chars = ptr;	
		recordLength = CS_GET_DISK_4(d.int_val->val_4);
		if (len < recordLength)
			continue;
		
		end_of_record = ptr + recordLength;
		
		// Look for the record trailer.
		d.rec_chars = end_of_record;	
		marker = CS_GET_DISK_4(d.int_val->val_4);
		if (marker != TAIL_MARKER) 
			continue;
			
		ptr +=4; // Skip the record length.
		badRecord = false;		
	}
	
	return true;
}

//---------------------
bool SysTabRec::firstRecord()
{
	ptr = getBuffer(0);
	end_of_data = ptr + length();
	
	if (!findRecord()) {
		logError("Missing record terminator, file being ignored");
		return false;
	}
	
	return true;
}

//---------------------
bool SysTabRec::nextRecord()
{
	if (!ptr)
		return firstRecord();
		
	if (ptr <= end_of_record) 
		ptr = end_of_record + 4;
	
	return findRecord();
}
	
//---------------------
void SysTabRec::resetRecord()
{
	ptr= 0;
}

//---------------------
uint8_t SysTabRec::getInt1Field()
{
	uint8_t val = 0;
	CSDiskData d;
	
	if (badRecord)
		return val;
		
	if (ptr > (end_of_record -1)) {
		logError("Missing 1 byte int field");
		ptr = end_of_record;
		badRecord = true;
		return val;
	}
	
	d.rec_chars = ptr;
	val = CS_GET_DISK_1(d.int_val->val_1);
	ptr += 1;		
	return val;
}

//---------------------
uint32_t SysTabRec::getInt4Field()
{
	uint32_t val = 0;
	CSDiskData d;
	
	if (badRecord)
		return val;
		
	if (ptr > (end_of_record -4)) {
		logError("Missing 4 byte int field");
		ptr = end_of_record;
		badRecord = true;
		return val;
	}
	
	d.rec_chars = ptr;
	val = CS_GET_DISK_4(d.int_val->val_4);
	ptr += 4;		
	return val;
}

//---------------------
const char *SysTabRec::getStringField()
{
	const char *val = "";
	
	if (badRecord)
		return val;
		
	if (ptr > (end_of_record -1)) {
		logError("Missing string field");
		badRecord = true;
		ptr = end_of_record;
	} else {
		val = ptr;
		while (*ptr && ptr < end_of_record) ptr++;
		if (ptr == end_of_record) {
			logError("Unterminated string field");
			badRecord = true;
			val = "";
		} else
			ptr++;
	}
	
	return val;
}

//---------------------
void SysTabRec::clear()
{
	setLength(0);
}

//---------------------
void SysTabRec::beginRecord()
{
	CSDiskData d;
	uint32_t len = length();
	
	setLength(len + 8); // Room for header marker and record length.

	d.rec_chars = getBuffer(len);
	CS_SET_DISK_4(d.int_val->val_4, HEAD_MARKER);	

	start_of_record = len + 4;
}

//---------------------
void SysTabRec::endRecord()
{
	CSDiskData d;
	uint32_t len = length();
	
	// Write the record length to the head of the record
	d.rec_chars = getBuffer(start_of_record);
	CS_SET_DISK_4(d.int_val->val_4, len - start_of_record);	

	// Write the record trailer
	setLength(len + 4); 
	d.rec_chars = getBuffer(len);
	CS_SET_DISK_4(d.int_val->val_4, TAIL_MARKER);		
}

//---------------------
void SysTabRec::setInt1Field(uint8_t val)
{
	CSDiskData d;

	uint32_t len = length();
	
	setLength(len +1); // Important: set the length before getting the buffer pointer
	d.rec_chars = getBuffer(len);
	CS_SET_DISK_1(d.int_val->val_1, val);	
}

//---------------------
void SysTabRec::setInt4Field(uint32_t val)
{
	CSDiskData d;

	uint32_t len = length();
	
	setLength(len +4); // Important: set the length before getting the buffer pointer
	d.rec_chars = getBuffer(len);
	CS_SET_DISK_4(d.int_val->val_4, val);	
}

//---------------------
void SysTabRec::setStringField(const char *val)
{
	if (!val) val = "";
	append(val, strlen(val) +1);
}

//---------------------
void SysTabRec::setStringField(const char *val, uint32_t len)
{
	if (val)
		append(val, len);
	append("", 1);
}


//======================
CSString *getPBMSPath(CSString *db_path)
{
	char pbms_path[PATH_MAX];
	enter_();
	
	push_(db_path);	
	cs_strcpy(PATH_MAX, pbms_path, db_path->getCString());
	release_(db_path);
	
	cs_remove_last_name_of_path(pbms_path);

	return_(CSString::newString(pbms_path));
}


//----------------------------
CSPath *getSysFile(CSString *db_path, const char *name_arg, size_t min_size)
{
	CSPath			*path;
	CSStringBuffer	*name;
	char			*ptr;

	enter_();
	
	push_(db_path);
	new_(name, CSStringBuffer());
	push_(name);
	name->append(name_arg);
	name->append(".dat");
	
	ptr = name->getBuffer(strlen(name_arg));

try_again:
	path = CSPath::newPath(RETAIN(db_path), name->getCString());
	push_(path);
	if (!path->exists()) {
		CSPath *tmp_path;

		strcpy(ptr, ".tmp");
		tmp_path = CSPath::newPath(RETAIN(db_path), name->getCString());
		push_(tmp_path);
		if (tmp_path->exists()) {
			strcpy(ptr, ".dat");
			tmp_path->rename(name->getCString());
		}
		release_(tmp_path);
	}
	
	// If the file if too small assume it is garbage.
	if (path->exists() && (path->getSize() < min_size)) {
		path->removeFile();
		release_(path);
		goto try_again;
	}
	
	pop_(path);
	release_(name);
	release_(db_path);
	return_(path);
}

