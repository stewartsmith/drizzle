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
 * System HTTP header storage table.
 *
 */

#pragma once
#ifndef __SYSTAB_HTTPHEADER_H__
#define __SYSTAB_HTTPHEADER_H__

#include "cslib/CSMutex.h"

#include "defs_ms.h"
#include "system_table_ms.h"
#include "discover_ms.h"

class SysTabRec;

class MSHTTPHeaderTable : public MSOpenSystemTable {
public:
	MSHTTPHeaderTable(MSSystemTableShare *share, TABLE *table);
	~MSHTTPHeaderTable();

	void use();
	void unuse();
	void seqScanInit();
	bool seqScanNext(char *buf);
	int	getRefLen() { return sizeof(iHeaderIndex);}
	void seqScanPos(unsigned char *pos);
	void seqScanRead(unsigned char *pos, char *buf);

	void	insertRow(char *buf);
	void	deleteRow(char *buf);
	void	updateRow(char *old_data, char *new_data);
	
	static void transferTable(MSDatabase *dst_db, MSDatabase *src_db);
	static void removeTable(CSString *db_path);
	static void saveTable(MSDatabase *db);
	static void loadTable(MSDatabase *db);
	
	static CSStringBuffer *dumpTable(MSDatabase *db);
	static void restoreTable(MSDatabase *db, const char *data, size_t size, bool reload = true);
	
	static void setDefaultMetaDataHeaders(const char *defaults);
	static void releaseDefaultMetaDataHeaders();
	
	static const uint8_t tableID = 0X02; // A unique id for the MSHTTPHeaderTable dump data
	static const uint16_t tableVersion = 0X0001; // The current version of the table

private:
	static			SysTabRec	*gDefaultMetaDataHeaders;
	uint32_t		iHeaderIndex;
	bool			iDirty;
};

#define METADATA_HEADER_NAME "pbms_metadata_header"

extern DT_FIELD_INFO pbms_metadata_headers_info[];
extern DT_KEY_INFO pbms_metadata_headers_keys[];


#endif // __SYSTAB_HTTPHEADER_H__
