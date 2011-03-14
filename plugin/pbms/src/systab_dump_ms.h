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
 * System dump table.
 *
 */

#pragma once
#ifndef __SYSTAB_DUMP_H__
#define __SYSTAB_DUMP_H__

#include "cslib/CSMutex.h"

#include "defs_ms.h"
#include "system_table_ms.h"

class MSDumpTable : public MSRepositoryTable {
public:
	MSDumpTable(MSSystemTableShare *share, TABLE *table);
	~MSDumpTable();
	
	void seqScanInit();
	bool seqScanNext(char *buf);
	void insertRow(char *buf);
	void use();
	void unuse();
	
private:
	uint32_t	dt_cloudbackupDBID;
	bool dt_hasCompleted;
	bool dt_hasInfo;
	bool dt_haveCloudInfo;
	uint16_t	dt_headerSize;
	
	virtual bool returnRow(MSBlobHeadPtr blob, char *buf);
	bool returnDumpRow(char *record, uint64_t record_size, char *buf);
	bool returnInfoRow(char *buf);
	
	void setUpRepository(const char *repoInfo, uint32_t length);
	void insertRepoRow(MSBlobHeadPtr blob, uint32_t length);
};

extern DT_FIELD_INFO pbms_dump_info[];
extern DT_KEY_INFO pbms_dump_keys[];

#endif // __SYSTAB_DUMP_H__
