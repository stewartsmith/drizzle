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
#ifndef DRIZZLED

#include "cslib/CSConfig.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>

//#include "mysql_priv.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"

#include "ha_pbms.h"
//#include <plugin.h>

#include "mysql_ms.h"
#include "repository_ms.h"
#include "database_ms.h"
#include "compactor_ms.h"
#include "open_table_ms.h"
#include "discover_ms.h"
#include "transaction_ms.h"
#include "systab_variable_ms.h"
#include "backup_ms.h"


#include "systab_enabled_ms.h"


DT_FIELD_INFO pbms_enabled_info[]=
{
	{"Name",			32,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	NOT_NULL_FLAG,	"PBMS enabled engine name"},
	{"IsServer",		3,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,					NOT_NULL_FLAG,	"Enabled at server level."},
	{"Transactional",	5,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,					NOT_NULL_FLAG,	"Does the engine support transactions."},
	{"API-Version",		NOVAL,	NULL, MYSQL_TYPE_LONG,		NULL,							NOT_NULL_FLAG,	"The PBMS enabled api version used."},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

DT_KEY_INFO pbms_enabled_keys[]=
{
	{"pbms_enabled_pk", PRI_KEY_FLAG, {"Name", NULL}},
	{NULL, 0, {NULL}}
};


/*
 * -------------------------------------------------------------------------
 * DUMP TABLE
 */
//-----------------------
MSEnabledTable::MSEnabledTable(MSSystemTableShare *share, TABLE *table):
MSOpenSystemTable(share, table),
iEnabledIndex(0)
{
}

//-----------------------
MSEnabledTable::~MSEnabledTable()
{
}

//-----------------------
void MSEnabledTable::seqScanInit()
{
	iEnabledIndex = 0;
}
//-----------------------
bool MSEnabledTable::seqScanNext(char *buf)
{
	TABLE		*table = mySQLTable;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;
	const char *yesno;
	const PBMSEngineRec *eng;
	
	enter_();
	
	eng = MSEngine::getEngineInfoAt(iEnabledIndex++);
	if (!eng)
		return_(false);
	
	save_write_set = table->write_set;
	table->write_set = NULL;

#ifdef DRIZZLED
	memset(buf, 0xFF, table->getNullBytes());
#else
	memset(buf, 0xFF, table->s->null_bytes);
#endif

 	for (Field **field=GET_TABLE_FIELDS(table) ; *field ; field++) {
 		curr_field = *field;
		save = curr_field->ptr;
#if MYSQL_VERSION_ID < 50114
		curr_field->ptr = (byte *) buf + curr_field->offset();
#else
#ifdef DRIZZLED
		curr_field->ptr = (byte *) buf + curr_field->offset(curr_field->getTable()->getInsertRecord());
#else
		curr_field->ptr = (byte *) buf + curr_field->offset(curr_field->table->record[0]);
#endif
#endif

		switch (curr_field->field_name[0]) {
			case 'N':
				ASSERT(strcmp(curr_field->field_name, "Name") == 0);
				curr_field->store(eng->ms_engine_name, strlen(eng->ms_engine_name), &UTF8_CHARSET);
				setNotNullInRecord(curr_field, buf);
				break;

			case 'I':
				ASSERT(strcmp(curr_field->field_name, "IsServer") == 0);
				if (eng->ms_internal)
					yesno = "Yes";
				else
					yesno = "No";
					
				curr_field->store(yesno, strlen(yesno), &UTF8_CHARSET);
				setNotNullInRecord(curr_field, buf);
				break;

			case 'T': 
				ASSERT(strcmp(curr_field->field_name, "Transactional") == 0);
				if (eng->ms_internal || eng->ms_version < 2 )
					yesno = "Maybe";
				else if (eng->ms_has_transactions)
					yesno = "Yes";
				else
					yesno = "No";
					
				curr_field->store(yesno, strlen(yesno), &UTF8_CHARSET);
				setNotNullInRecord(curr_field, buf);
				break;

			case 'A':
				ASSERT(strcmp(curr_field->field_name, "API-Version") == 0);
				curr_field->store(eng->ms_version, true);
				break;

		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return_(true);
}

//-----------------------
void MSEnabledTable::seqScanPos(unsigned char *pos )
{
	int32_t index = iEnabledIndex -1;
	if (index < 0)
		index = 0; // This is probably an error condition.
		
	mi_int4store(pos, index);
}

//-----------------------
void MSEnabledTable::seqScanRead(unsigned char *pos , char *buf)
{
	iEnabledIndex = mi_uint4korr(pos);
	seqScanNext(buf);
}

#endif // DRIZZLED


