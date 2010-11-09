/* Copyright (c) 2009 PrimeBase Technologies GmbH, Germany
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
 * 2009-10-21
 *
 * System cloud starage info table.
 *
 */

#ifndef __SYSTAB_CLOUD_H__
#define __SYSTAB_CLOUD_H__

#include "cslib/CSMutex.h"

#include "defs_ms.h"
#include "system_table_ms.h"
#include "discover_ms.h"

class MSCloudTable : public MSOpenSystemTable {
public:
	MSCloudTable(MSSystemTableShare *share, TABLE *table);
	~MSCloudTable();

	void use();
	void unuse();
	void seqScanInit();
	bool seqScanNext(char *buf);
	int	getRefLen() { return sizeof(iCloudIndex);}
	void seqScanPos(unsigned char *pos );
	void seqScanRead(unsigned char *pos , char *buf);

	void updateRow(char *old_data, char *new_data);
	void insertRow(char *data);
	void deleteRow(char *data);

	static void startUp();
	static void shutDown();

	static void transferTable(MSDatabase *from_db, MSDatabase *to_db);
	static CSStringBuffer *dumpTable(MSDatabase *db);
	static void restoreTable(MSDatabase *db, const char *data, size_t size, bool reload = true);
	static void removeTable(CSString *db_path);
	
	static void saveTable(MSDatabase *db);
	static void loadTable(MSDatabase *db);
	static const uint8_t tableID = 0X01; // A unique id for the MSCloudTable dump data
	static const uint16_t tableVersion = 0X0001; // The current version of the table
	

private:
	uint32_t			iCloudIndex;
};

#define CLOUD_TABLE_NAME "pbms_cloud"
extern DT_FIELD_INFO pbms_cloud_info[];
extern DT_KEY_INFO pbms_cloud_keys[];

#endif // __SYSTAB_CLOUD_H__
