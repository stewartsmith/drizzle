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
 * System variables table.
 *
 */
#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/session.h>
#include <drizzled/sql_lex.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/varstring.h>
#endif

#include "cslib/CSConfig.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>

//#include "mysql_priv.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSLog.h"
#include "cslib/CSPath.h"

#include "ha_pbms.h"
//#include <plugin.h>

#include "mysql_ms.h"
#include "database_ms.h"
#include "open_table_ms.h"
#include "discover_ms.h"
#include "systab_util_ms.h"
#include "pbmslib.h"

#include "systab_httpheader_ms.h"

#define METADATA_HEADER_FILE	"http-meta-data-headers"
#define MIN_METADATA_HEADER_FILE_SIZE	3

SysTabRec	*MSHTTPHeaderTable::gDefaultMetaDataHeaders;

DT_FIELD_INFO pbms_metadata_headers_info[]=
{
	{"Name", 32,	NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	NOT_NULL_FLAG,	"HTTP field name"},
	{NULL,	NOVAL,				NULL, MYSQL_TYPE_STRING,	NULL, 0, NULL}
};

DT_KEY_INFO pbms_metadata_headers_keys[]=
{
	{NULL, 0, {NULL}}
};

/*
 * -------------------------------------------------------------------------
 * HTTP HEADER TABLE
 */
void MSHTTPHeaderTable::releaseDefaultMetaDataHeaders()
{
	if (gDefaultMetaDataHeaders) 
		gDefaultMetaDataHeaders->release();
	gDefaultMetaDataHeaders = NULL;
}

void MSHTTPHeaderTable::setDefaultMetaDataHeaders(const char *defaults)
{
	const char *ptr;
	enter_();
	
	if (!gDefaultMetaDataHeaders) 
		new_(gDefaultMetaDataHeaders, SysTabRec("", METADATA_HEADER_FILE".dat", METADATA_HEADER_NAME));

	gDefaultMetaDataHeaders->clear();
	if (defaults) {
		while (*defaults) {
			for (ptr = defaults; *ptr && (*ptr != ':'); ptr++){}
			if (ptr != defaults) {
				gDefaultMetaDataHeaders->beginRecord();
				gDefaultMetaDataHeaders->setStringField(defaults, ptr - defaults);
				gDefaultMetaDataHeaders->endRecord();
			}
			if (!*ptr)
				break;
				
			defaults = ptr +1;
		}
	}
	exit_();
}

//----------------------------
void MSHTTPHeaderTable::loadTable(MSDatabase *db)
{
	CSPath		*path;
	SysTabRec	*headerData = NULL;

	enter_();
	
	push_(db);
	path = getSysFile(RETAIN(db->myDatabasePath), METADATA_HEADER_FILE, MIN_METADATA_HEADER_FILE_SIZE);
	push_(path);

	if (path->exists()) {
		CSFile		*file;
		size_t		size;
		
		new_(headerData, SysTabRec(db->myDatabaseName->getCString(), METADATA_HEADER_FILE".dat", METADATA_HEADER_NAME));
		push_(headerData);

		file = path->openFile(CSFile::READONLY);
		push_(file);
		size = file->getEOF();
		headerData->setLength(size);
		file->read(headerData->getBuffer(0), 0, size, size);
		release_(file);
		
		
	} else if (gDefaultMetaDataHeaders) { // Load the defaults if they exist.
		headerData = gDefaultMetaDataHeaders;
	}

	if (headerData) {
		while (headerData->nextRecord()) {
			const char	*header = headerData->getStringField();
			
			if (headerData->isValidRecord()) {
				db->iHTTPMetaDataHeaders.add(CSString::newString(header));
			}
		}
		
		if (headerData == gDefaultMetaDataHeaders)
			gDefaultMetaDataHeaders->resetRecord();
		else {
			release_(headerData); 
		}
	}
	
	release_(path);	
	release_(db);

	exit_();
}

//----------------------------
void MSHTTPHeaderTable::saveTable(MSDatabase *db)
{
	CSString *str;
	SysTabRec	*headerData;
	enter_();
	
	push_(db);
	
	new_(headerData, SysTabRec(db->myDatabaseName->getCString(), METADATA_HEADER_FILE".dat", METADATA_HEADER_NAME));
	push_(headerData);
	
	// Build the table records
	headerData->clear();
	lock_(&db->iHTTPMetaDataHeaders);
	
	// Note: the object returned by itemAt() is not returnd referenced.
	for (uint32_t i =0; (str = (CSString*) db->iHTTPMetaDataHeaders.itemAt(i)); i++) {
		headerData->beginRecord();
		headerData->setStringField(str->getCString());
		headerData->endRecord();
	}
	unlock_(&db->iHTTPMetaDataHeaders);
	
	restoreTable(RETAIN(db), headerData->getBuffer(0), headerData->length(), false);
	
	release_(headerData);
	release_(db);
	exit_();
}

//--------------------
CSStringBuffer *MSHTTPHeaderTable::dumpTable(MSDatabase *db)
{

	CSPath			*path;
	CSStringBuffer	*dump;

	enter_();
	
	push_(db);
	path = getSysFile(RETAIN(db->myDatabasePath), METADATA_HEADER_FILE, MIN_METADATA_HEADER_FILE_SIZE);
	release_(db);
	
	push_(path);
	new_(dump, CSStringBuffer(20));
	push_(dump);

	if (path->exists()) {
		CSFile	*file;
		size_t	size;
		
		file = path->openFile(CSFile::READONLY);
		push_(file);
		
		size = file->getEOF();
		dump->setLength(size);
		file->read(dump->getBuffer(0), 0, size, size);
		release_(file);
	}
	
	pop_(dump);
	release_(path);
	return_(dump);
}

//--------------------
void MSHTTPHeaderTable::restoreTable(MSDatabase *db, const char *data, size_t size, bool reload)
{
	CSPath	*path;
	CSFile	*file;

	enter_();
	
	push_(db);
	path = getSysFile(RETAIN(db->myDatabasePath), METADATA_HEADER_FILE, MIN_METADATA_HEADER_FILE_SIZE);
	push_(path);
	
	file = path->openFile(CSFile::CREATE | CSFile::TRUNCATE);
	push_(file);
	
	file->write(data, 0, size);
	file->close();
	release_(file);
	
	release_(path);
	
	pop_(db);
	if (reload)
		loadTable(db);
	else
		db->release();
	exit_();
}

//--------------------
void MSHTTPHeaderTable::transferTable(MSDatabase *dst_db, MSDatabase *src_db)
{
	CSPath	*path;
	enter_();
	
	push_(src_db);
	push_(dst_db);
	
	path = CSPath::newPath(RETAIN(src_db->myDatabasePath), METADATA_HEADER_FILE".dat");
	push_(path);
	if (path->exists()) {
		CSPath	*bu_path;
		bu_path = CSPath::newPath(RETAIN(dst_db->myDatabasePath), METADATA_HEADER_FILE".dat");
		path->copyTo(bu_path, true);
	}
	
	release_(path);
	release_(dst_db);
	release_(src_db);
	
	exit_();
}

void MSHTTPHeaderTable::removeTable(CSString *db_path)
{
	CSPath	*path;
	enter_();
	
	path = getSysFile(db_path, METADATA_HEADER_FILE, 0);
	push_(path);
	
	path->removeFile();
	release_(path);
	exit_();
}

MSHTTPHeaderTable::MSHTTPHeaderTable(MSSystemTableShare *share, TABLE *table):
MSOpenSystemTable(share, table),
iHeaderIndex(0),
iDirty(false)
{
}

MSHTTPHeaderTable::~MSHTTPHeaderTable()
{
	//unuse();
}

void MSHTTPHeaderTable::use()
{
	myShare->mySysDatabase->iHTTPMetaDataHeaders.lock();
	iDirty = false;
}

void MSHTTPHeaderTable::unuse()
{
	if (iDirty) {
		saveTable(RETAIN(myShare->mySysDatabase));
		iDirty = false;	
	}
	myShare->mySysDatabase->iHTTPMetaDataHeaders.unlock();
	
}


void MSHTTPHeaderTable::seqScanInit()
{
	iHeaderIndex = 0;
}

bool MSHTTPHeaderTable::seqScanNext(char *buf)
{
	TABLE		*table = mySQLTable;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;
	CSString	*header;
	const char	*name;
	
	enter_();
	
	header = (CSString*) (myShare->mySysDatabase->iHTTPMetaDataHeaders.itemAt(iHeaderIndex));
	if (!header)
		return_(false);
		
	iHeaderIndex++;
	name = header->getCString();
	
	save_write_set = table->write_set;
	table->write_set = NULL;

#ifdef DRIZZLED
	memset(buf, 0xFF, table->getNullBytes());
#else
	memset(buf, 0xFF, table->s->null_bytes);
#endif
 	for (Field **field=GET_TABLE_FIELDS(table); *field ; field++) {
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
					curr_field->store(name, strlen(name), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				break;

		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return_(true);
}

void MSHTTPHeaderTable::seqScanPos(unsigned char *pos)
{
	int32_t index = iHeaderIndex -1;
	if (index < 0)
		index = 0; // This is probably an error condition.
		
	mi_int4store(pos, index);
}

void MSHTTPHeaderTable::seqScanRead(unsigned char *pos, char *buf)
{
	iHeaderIndex = mi_uint4korr(pos);
	seqScanNext(buf);
}

void MSHTTPHeaderTable::insertRow(char *data) 
{
	CSString *header;
	String name;	
	enter_();
	
	getFieldValue(data, 0, &name);

	header = CSString::newString(name.c_ptr_safe());
	myShare->mySysDatabase->iHTTPMetaDataHeaders.add(header);
	iDirty = true;
	
	exit_();
}

void MSHTTPHeaderTable::deleteRow(char *data) 
{
	CSString *header;
	String name;	
	enter_();
	
	getFieldValue(data, 0, &name);
	
	header = CSString::newString(name.c_ptr_safe());
	push_(header);
	myShare->mySysDatabase->iHTTPMetaDataHeaders.remove(header);
	release_(header);
	iDirty = true;
		
	exit_();
}

void MSHTTPHeaderTable::updateRow(char *old_data, char *new_data) 
{
	enter_();
	insertRow(new_data);
	try_(a) {
		deleteRow(old_data);
	}
	catch_(a) {
		deleteRow(new_data);
		throw_();
	}
	cont_(a);
	exit_();
}

