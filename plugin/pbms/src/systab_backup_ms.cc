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
 * 2009-10-27
 *
 * System backup info table for repository backups.
 */
#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/session.h>
#include <drizzled/sql_lex.h>
#endif

#include "cslib/CSConfig.h"
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>

//#include "mysql_priv.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSLog.h"
#include "cslib/CSPath.h"
#include "cslib/CSDirectory.h"

#include "ha_pbms.h"
//#include <plugin.h>

#include "mysql_ms.h"
#include "database_ms.h"
#include "open_table_ms.h"
#include "discover_ms.h"
#include "systab_util_ms.h"
#include "backup_ms.h"

#include "systab_backup_ms.h"


DT_FIELD_INFO pbms_backup_info[]=
{
	{"Id",				NOVAL,	NULL, MYSQL_TYPE_LONG,		NULL,			NOT_NULL_FLAG,	"The backup reference ID"},
	{"Database_Name",	64,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	NOT_NULL_FLAG,	"The database name"},
	{"Database_Id",		NOVAL,	NULL, MYSQL_TYPE_LONG,		NULL,			0,	"The database ID"},
	{"Started",			32,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	0,	"The start time"},
	{"Completed",		32,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	0,	"The completion time"},
	{"IsRunning",		3,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	0,	"Is the backup still running"},
	{"IsDump",			3,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	0,	"Is the backup the result of a dump"},
	{"Location",		1024,	NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	0,	"The backup location"},
	{"Cloud_Ref",		NOVAL,	NULL, MYSQL_TYPE_LONG,		NULL,			0,	"The S3 cloud reference number refering to the pbms.pbms_cloud table."},
	{"Cloud_Backup_No",	NOVAL,	NULL, MYSQL_TYPE_LONG,		NULL,			0,	"The cloud backup number"},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

DT_KEY_INFO pbms_backup_keys[]=
{
	{"pbms_backup_pk", PRI_KEY_FLAG, {"Id", NULL}},
	{NULL, 0, {NULL}}
};

#define MIN_BACKUP_TABLE_FILE_SIZE 4


//----------------------------
void MSBackupTable::startUp()
{
	MSBackupInfo::startUp();
}

//----------------------------
void MSBackupTable::shutDown()
{
	MSBackupInfo::shutDown();
}

//----------------------------
void MSBackupTable::loadTable(MSDatabase *db)
{

	enter_();
	
	push_(db);
	lock_(MSBackupInfo::gBackupInfo);
	
	if (MSBackupInfo::gMaxInfoRef == 0) {
		CSPath	*path;
		path = getSysFile(getPBMSPath(RETAIN(db->myDatabasePath)), BACKUP_TABLE_NAME, MIN_BACKUP_TABLE_FILE_SIZE);
		push_(path);

		if (path->exists()) {
			CSFile		*file;
			SysTabRec	*backupData;
			const char	*name, *location;
			uint32_t		info_id, db_id, start, end, cloud_ref, cloud_backup_no;
			bool		isDump;
			MSBackupInfo	*info;
			size_t		size;
			
			new_(backupData, SysTabRec("pbms", BACKUP_TABLE_NAME".dat", BACKUP_TABLE_NAME));
			push_(backupData);

			file = path->openFile(CSFile::READONLY);
			push_(file);
			size = file->getEOF();
			backupData->setLength(size);
			file->read(backupData->getBuffer(0), 0, size, size);
			release_(file);
			
			backupData->firstRecord();
			MSBackupInfo::gMaxInfoRef = backupData->getInt4Field();
			
			if (! backupData->isValidRecord()) 
				MSBackupInfo::gMaxInfoRef = 1;
			
			while (backupData->nextRecord()) {
				info_id = backupData->getInt4Field();
				name = backupData->getStringField();
				db_id = backupData->getInt4Field();
				start = backupData->getInt4Field();
				end = backupData->getInt4Field();
				isDump = backupData->getInt1Field();
				location = backupData->getStringField();
				cloud_ref = backupData->getInt4Field();
				cloud_backup_no = backupData->getInt4Field();
				
				if (backupData->isValidRecord()) {
					if (info_id > MSBackupInfo::gMaxInfoRef) {
						char msg[80];
						snprintf(msg, 80, "backup info id (%"PRIu32") larger than expected (%"PRIu32")\n", info_id, MSBackupInfo::gMaxInfoRef);
						CSL.log(self, CSLog::Warning, "pbms "BACKUP_TABLE_NAME".dat :possible damaged file or record. ");
						CSL.log(self, CSLog::Warning, msg);
						MSBackupInfo::gMaxInfoRef = info_id +1;
					}
					if ( MSBackupInfo::gBackupInfo->get(info_id)) {
						char msg[80];
						snprintf(msg, 80, "Duplicate Backup info id (%"PRIu32") being ignored\n", info_id);
						CSL.log(self, CSLog::Warning, "pbms "BACKUP_TABLE_NAME".dat :possible damaged file or record. ");
						CSL.log(self, CSLog::Warning, msg);
					} else {
						new_(info, MSBackupInfo(info_id, name, db_id, start, end, isDump, location, cloud_ref, cloud_backup_no));
						MSBackupInfo::gBackupInfo->set(info_id, info);
					}
				}
			}
			release_(backupData); backupData = NULL;
			
		} else
			MSBackupInfo::gMaxInfoRef = 1;
		
		release_(path);
		
	}
	unlock_(MSBackupInfo::gBackupInfo);

	release_(db);

	exit_();
}

void MSBackupTable::saveTable(MSDatabase *db)
{
	SysTabRec		*backupData;
	MSBackupInfo		*info;
	enter_();
	
	push_(db);
	
	new_(backupData, SysTabRec("pbms", BACKUP_TABLE_NAME".dat", BACKUP_TABLE_NAME));
	push_(backupData);
	
	// Build the table records
	backupData->clear();
	lock_(MSBackupInfo::gBackupInfo);
	
	backupData->beginRecord();	
	backupData->setInt4Field(MSBackupInfo::gMaxInfoRef);
	backupData->endRecord();	
	for  (int i = 0;(info = (MSBackupInfo*) MSBackupInfo::gBackupInfo->itemAt(i)); i++) { // info is not referenced.
		
		backupData->beginRecord();	
		backupData->setInt4Field(info->getBackupRefId());
		
		backupData->setStringField(info->getName());
		backupData->setInt4Field(info->getDatabaseId());
		backupData->setInt4Field(info->getStart());
		backupData->setInt4Field(info->getEnd());
		backupData->setInt1Field(info->isDump());
		backupData->setStringField(info->getLocation());
		backupData->setInt4Field(info->getcloudRef());
		backupData->setInt4Field(info->getcloudBackupNo());
		backupData->endRecord();			
	}
	unlock_(MSBackupInfo::gBackupInfo);

	restoreTable(RETAIN(db), backupData->getBuffer(0), backupData->length(), false);
	
	release_(backupData);
	release_(db);
	exit_();
}


MSBackupTable::MSBackupTable(MSSystemTableShare *share, TABLE *table):
MSOpenSystemTable(share, table),
iBackupIndex(0)
{
}

MSBackupTable::~MSBackupTable()
{
	//unuse();
}

void MSBackupTable::use()
{
	MSBackupInfo::gBackupInfo->lock();
}

void MSBackupTable::unuse()
{
	MSBackupInfo::gBackupInfo->unlock();
	
}


void MSBackupTable::seqScanInit()
{
	iBackupIndex = 0;
}

bool MSBackupTable::seqScanNext(char *buf)
{
	TABLE		*table = mySQLTable;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;
	MSBackupInfo	*info;
	CSTime		*timeVal;
	const char	*val;
	
	enter_();
	
	info = (MSBackupInfo	*) MSBackupInfo::gBackupInfo->itemAt(iBackupIndex++); // Object is not referenced.
	if (!info)
		return_(false);
	
	save_write_set = table->write_set;
	table->write_set = NULL;

	new_(timeVal, CSTime());
	push_(timeVal);
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
			case 'I':
				if (curr_field->field_name[1] == 'd') {
					ASSERT(strcmp(curr_field->field_name, "Id") == 0);
					curr_field->store(info->getBackupRefId(), true);
					setNotNullInRecord(curr_field, buf);
				} else if (curr_field->field_name[2] == 'D') {
					ASSERT(strcmp(curr_field->field_name, "IsDump") == 0);
					val = (info->isDump())? "Yes": "No";
					curr_field->store(val, strlen(val), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				} else {
					ASSERT(strcmp(curr_field->field_name, "IsRunning") == 0);
					val = (info->isBackupRunning())? "Yes": "No";
					curr_field->store(val, strlen(val), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				} 
				break;

			case 'D':
				if (curr_field->field_name[9] == 'I') {
					ASSERT(strcmp(curr_field->field_name, "Database_Id") == 0);
					curr_field->store(info->getDatabaseId(), true);
					setNotNullInRecord(curr_field, buf);
				} else {
					ASSERT(strcmp(curr_field->field_name, "Database_Name") == 0);
					val = info->getName();
					curr_field->store(val, strlen(val), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				}
				
				break;

			case 'S': 
				ASSERT(strcmp(curr_field->field_name, "Started") == 0);
				if (info->getStart()) {
					timeVal->setUTC1970(info->getStart(), 0);
					val = timeVal->getCString();
					curr_field->store(val, strlen(val), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				}
				break;

			case 'L':
				ASSERT(strcmp(curr_field->field_name, "Location") == 0);
				val = info->getLocation();
				if (val) {
					curr_field->store(val, strlen(val), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				}
				break;

			case 'C': 
				if (curr_field->field_name[1] == 'o') {
					ASSERT(strcmp(curr_field->field_name, "Completed") == 0);
					if (info->getEnd()) {
						timeVal->setUTC1970(info->getEnd(), 0);
						val = timeVal->getCString();
						curr_field->store(val, strlen(val), &UTF8_CHARSET);
						setNotNullInRecord(curr_field, buf);
					}
				} else if (curr_field->field_name[6] == 'R') {
					ASSERT(strcmp(curr_field->field_name, "Cloud_Ref") == 0);
					curr_field->store(info->getcloudRef(), true);
					setNotNullInRecord(curr_field, buf);
				} else if (curr_field->field_name[6] == 'B') {
					ASSERT(strcmp(curr_field->field_name, "Cloud_Backup_No") == 0);
					curr_field->store(info->getcloudBackupNo(), true);
					setNotNullInRecord(curr_field, buf);
				} else {
					ASSERT(false);
					break;
				}
				break;
				
			default:
				ASSERT(false);
		}
		curr_field->ptr = save;
	}

	release_(timeVal);
	table->write_set = save_write_set;
	
	return_(true);
}

void MSBackupTable::seqScanPos(unsigned char *pos)
{
	int32_t index = iBackupIndex -1;
	if (index < 0)
		index = 0; // This is probably an error condition.
		
	mi_int4store(pos, index);
}

void MSBackupTable::seqScanRead(unsigned char *pos, char *buf)
{
	iBackupIndex = mi_uint4korr(pos);
	seqScanNext(buf);
}

void MSBackupTable::updateRow(char *old_data, char *new_data) 
{
	uint32_t n_id, db_id, cloud_ref, cloud_backup_no, n_indx;
	uint32_t o_id, o_db_id, o_cloud_ref, o_cloud_backup_no, o_indx;
	String name, start, end, isRunning, isDump, location;
	String o_name, o_start, o_end, o_isRunning, o_isDump, o_location;
	MSBackupInfo *info, *old_info;

	enter_();
	
	getFieldValue(new_data, 0, &n_id);
	getFieldValue(new_data, 1, &name);
	getFieldValue(new_data, 2, &db_id);
	getFieldValue(new_data, 3, &start);
	getFieldValue(new_data, 4, &end);
	getFieldValue(new_data, 5, &isRunning);
	getFieldValue(new_data, 6, &isDump);
	getFieldValue(new_data, 7, &location);
	getFieldValue(new_data, 8, &cloud_ref);
	getFieldValue(new_data, 9, &cloud_backup_no);

	getFieldValue(old_data, 0, &o_id);
	getFieldValue(old_data, 1, &o_name);
	getFieldValue(old_data, 2, &o_db_id);
	getFieldValue(old_data, 3, &o_start);
	getFieldValue(old_data, 4, &o_end);
	getFieldValue(old_data, 5, &o_isRunning);
	getFieldValue(old_data, 6, &o_isDump);
	getFieldValue(old_data, 7, &o_location);
	getFieldValue(old_data, 8, &o_cloud_ref);
	getFieldValue(old_data, 9, &o_cloud_backup_no);

	// The only fields that are allowed to be updated are 'Location' and 'Cloud_Ref'.
	// It makes no scence to update any of the other fields.
	if (n_id != o_id )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (Id) in the "BACKUP_TABLE_NAME" table.");
	
	if (strcmp(name.c_ptr(), o_name.c_ptr()) == 0 )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (Database_Name) in the "BACKUP_TABLE_NAME" table.");
	
	if (db_id != o_db_id )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (Database_Id) in the "BACKUP_TABLE_NAME" table.");
	
	if (strcmp(start.c_ptr(), o_start.c_ptr()) == 0 )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (Started) in the "BACKUP_TABLE_NAME" table.");
	
	if (strcmp(end.c_ptr(), o_end.c_ptr()) == 0 )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (Completed) in the "BACKUP_TABLE_NAME" table.");
	
	if (strcmp(isRunning.c_ptr(), o_isRunning.c_ptr()) == 0 )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (isRunning) in the "BACKUP_TABLE_NAME" table.");
	
	if (strcmp(isDump.c_ptr(), o_isDump.c_ptr()) == 0 )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (IsDump) in the "BACKUP_TABLE_NAME" table.");
	
	if (cloud_backup_no != o_cloud_backup_no )
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only field (Cloud_Backup_No) in the "BACKUP_TABLE_NAME" table.");

	old_info = (MSBackupInfo*)  MSBackupInfo::gBackupInfo->get(o_id); // A non referenced object.
	
	new_(info, MSBackupInfo(n_id, old_info->getName(), db_id, old_info->getStart(), old_info->getEnd(), old_info->isDump(), location.c_ptr(), cloud_ref, cloud_backup_no));
	push_(info);
	
	o_indx = MSBackupInfo::gBackupInfo->getIndex(o_id);

	MSBackupInfo::gBackupInfo->remove(o_id);
	pop_(info);
	MSBackupInfo::gBackupInfo->set(n_id, info);
	
	// Adjust the current position in the array if required.
	n_indx = MSBackupInfo::gBackupInfo->getIndex(n_id);
	if (o_indx < n_indx )
		iBackupIndex--;

	saveTable(RETAIN(myShare->mySysDatabase));

	exit_();
}

class InsertRowCleanUp : public CSRefObject {
	bool do_cleanup;
	CSThread *myself;
	
	uint32_t ref_id;

	public:
	
	InsertRowCleanUp(CSThread *self): CSRefObject(),
		do_cleanup(true), myself(self){}
		
	~InsertRowCleanUp() 
	{
		if (do_cleanup) {
			myself->logException();
			if (ref_id)
				MSBackupInfo::gBackupInfo->remove(ref_id);

		}
	}
	
	void setCleanUp(uint32_t id)
	{
		ref_id = id;
	}
	
	void cancelCleanUp()
	{
		do_cleanup = false;
	}
	
};

void MSBackupTable::insertRow(char *data) 
{
	uint32_t ref_id = 0, db_id, cloud_ref, cloud_backup_no;
	String name, start, end, isRunning, isDump, location;
	MSBackupInfo *info = NULL;
	const char *db_name;
	InsertRowCleanUp *cleanup;

	enter_();

	new_(cleanup, InsertRowCleanUp(self));
	push_(cleanup);
	
	getFieldValue(data, 0, &ref_id);
		
	// The id must be unique.
	if (ref_id && MSBackupInfo::gBackupInfo->get(ref_id)) {
		CSException::throwException(CS_CONTEXT, MS_ERR_DUPLICATE, "Attempt to insert a row with a duplicate key in the "BACKUP_TABLE_NAME" table.");
	}
	
	// The 'Database_Id', 'Start', 'Completion' and "IsDump" fields are ignored.
	// I still need to get the fields though to advance the field position pointer.
	getFieldValue(data, 1, &name);
	getFieldValue(data, 2, &db_id);
	getFieldValue(data, 3, &start);
	getFieldValue(data, 4, &end);
	getFieldValue(data, 5, &isRunning);
	getFieldValue(data, 6, &isDump);
	getFieldValue(data, 7, &location);
	getFieldValue(data, 8, &cloud_ref);
	getFieldValue(data, 9, &cloud_backup_no);
	
	if (ref_id == 0)
		ref_id = MSBackupInfo::gMaxInfoRef++;
	else if (ref_id >= MSBackupInfo::gMaxInfoRef)
		MSBackupInfo::gMaxInfoRef = ref_id +1;
	
	db_name	= name.c_ptr();
	db_id = MSDatabase::getDatabaseID(db_name, false);
	
	cleanup->setCleanUp(ref_id);
	new_(info, MSBackupInfo(ref_id, db_name, db_id, 0, 0, false, location.c_ptr(), cloud_ref, cloud_backup_no));
	MSBackupInfo::gBackupInfo->set(ref_id, info);
	
	// There is no need to call this now, startBackup() will call it
	// after the backup is started.
	// saveTable(RETAIN(myShare->mySysDatabase)); 
	info->startBackup(RETAIN(myShare->mySysDatabase));

	cleanup->cancelCleanUp();
	release_(cleanup);
	
	exit_();
}

void MSBackupTable::deleteRow(char *data) 
{
	uint32_t ref_id, indx;

	enter_();
	
	getFieldValue(data, 0, &ref_id);
	
	// Adjust the current position in the array if required.
	indx = MSBackupInfo::gBackupInfo->getIndex(ref_id);
	if (indx <= iBackupIndex)
		iBackupIndex--;
	
	MSBackupInfo::gBackupInfo->remove(ref_id);
	saveTable(RETAIN(myShare->mySysDatabase));
	exit_();
}

void MSBackupTable::transferTable(MSDatabase *to_db, MSDatabase *from_db)
{
	CSPath	*path;
	enter_();
	
	push_(from_db);
	push_(to_db);
	
	path = CSPath::newPath(getPBMSPath(RETAIN(from_db->myDatabasePath)), BACKUP_TABLE_NAME".dat");
	push_(path);
	if (path->exists()) {
		CSPath	*bu_path;
		bu_path = CSPath::newPath(getPBMSPath(RETAIN(to_db->myDatabasePath)), BACKUP_TABLE_NAME".dat");
		path->copyTo(bu_path, true);
	}
	
	release_(path);
	release_(to_db);
	release_(from_db);
	
	exit_();
}

CSStringBuffer *MSBackupTable::dumpTable(MSDatabase *db)
{

	CSPath			*path;
	CSStringBuffer	*dump;

	enter_();
	
	push_(db);
	path = getSysFile(getPBMSPath(RETAIN(db->myDatabasePath)), BACKUP_TABLE_NAME, MIN_BACKUP_TABLE_FILE_SIZE);
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

void MSBackupTable::restoreTable(MSDatabase *db, const char *data, size_t size, bool reload)
{
	CSPath	*path;
	CSFile	*file;

	enter_();
	
	push_(db);
	path = getSysFile(getPBMSPath(RETAIN(db->myDatabasePath)), BACKUP_TABLE_NAME, MIN_BACKUP_TABLE_FILE_SIZE);
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

// The cloud info table is only removed from the pbms database
// if there are no more databases.
void MSBackupTable::removeTable(CSString *db_path)
{
	CSPath	*path;
	char pbms_path[PATH_MAX];
	
	enter_();
	
	push_(db_path);	
	cs_strcpy(PATH_MAX, pbms_path, db_path->getCString());
	release_(db_path);
	
	if (strcmp(cs_last_name_of_path(pbms_path), "pbms")  != 0)
		exit_();
		
	cs_remove_last_name_of_path(pbms_path);

	path = getSysFile(CSString::newString(pbms_path), BACKUP_TABLE_NAME, MIN_BACKUP_TABLE_FILE_SIZE);
	push_(path);
	
	if (path->exists())
		path->removeFile();
	release_(path);
	
	exit_();
}

