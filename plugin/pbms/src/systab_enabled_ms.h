/* Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
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
 * System enabled table.
 *
 */

#pragma once
#ifndef __SYSTAB_ENABLED_H__
#define __SYSTAB_ENABLED_H__

#include "defs_ms.h"
#include "system_table_ms.h"
#include "discover_ms.h"

#define ENABLED_TABLE_NAME "pbms_enabled"

class MSEnabledTable : public MSOpenSystemTable {
public:
	MSEnabledTable(MSSystemTableShare *share, TABLE *table);
	~MSEnabledTable();
	
	void seqScanInit();
	bool seqScanNext(char *buf);
	int	getRefLen() { return sizeof(iEnabledIndex);}
	void seqScanPos(unsigned char *pos );
	void seqScanRead(unsigned char *pos , char *buf);
	
private:
	uint32_t			iEnabledIndex;
};

extern DT_FIELD_INFO pbms_enabled_info[];
extern DT_KEY_INFO pbms_enabled_keys[];

#endif // __SYSTAB_ENABLED_H__
