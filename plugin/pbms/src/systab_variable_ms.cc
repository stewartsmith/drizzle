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

#include "ha_pbms.h"
//#include <plugin.h>

#include "mysql_ms.h"
#include "repository_ms.h"
#include "database_ms.h"
#include "compactor_ms.h"
#include "open_table_ms.h"
#include "discover_ms.h"



#include "systab_variable_ms.h"

#define MS_REPOSITORY_STORAGE_TYPE	"REPOSITORY"
#define MS_CLOUD_STORAGE_TYPE		"CLOUD"

DT_FIELD_INFO pbms_variable_info[]=
{
	{"Id",			NOVAL,	NULL, MYSQL_TYPE_LONG,		NULL,							NOT_NULL_FLAG,	"The variable ID"},
	{"Name",		32,		NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	NOT_NULL_FLAG,	"PBMS variable name"},
	{"Value",	1024,	NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	0,	"PBMS variable value."},
	{"Description",	124,	NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	NOT_NULL_FLAG,	"PBMS variable description."},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

DT_KEY_INFO pbms_variable_keys[]=
{
	{"pbms_variable_pk", PRI_KEY_FLAG, {"Id", NULL}},
	{NULL, 0, {NULL}}
};

typedef const char *(*PBMSVarGetFunc)(MSDatabase *db, const char *dflt_value);
typedef const char *(*PBMSVarCheckFunc)(char *value, bool *ok);
typedef void (*PBMSVarActionFunc)(MSDatabase *db, const char *value);

typedef struct {
	bool hidden;			// true if the value is not to be displayed
	const char *name;
	const char *value;
	const char *info;
	bool save;			// true if the value is to be saved to disk on update
	PBMSVarGetFunc get;
	PBMSVarCheckFunc check;
	PBMSVarActionFunc action;
} PBMSVariableRec, *PBMSVariablePtr;

CSLock MSVariableTable::gVarLock;

//---------------------------
static char *cleanupVariable(char *value, int *len)
{
	char *ptr;
	
	while (*value && isspace(*value)) value++;
	ptr = value + strlen(value) -1;
	while ((ptr > value) && isspace(*ptr)) ptr--;
	ptr++;
	*ptr = 0;
	
	ptr = value;
	while (*ptr) {
		*ptr = toupper(*ptr);
		ptr++;
	}
	
	*len = ptr - value;
	return value;
}

//---------------------------
static const char *get_DumpRestore(MSDatabase *db, const char *)
{
	const char *value;
	enter_();
	push_(db);
	if (db->isRecovering())
		value = "TRUE";
	else
		value = "FALSE";
	release_(db);
	return_(value);
}

//---------------------------
static const char *readOnlyCheck(char *, bool *ok)
{
	*ok = false;
	return "Value is Read Only.";
}
//---------------------------
static const char *boolCheck(char *value, bool *ok)
{
	const char *val = "Invalid boolean variable, try 'true' or 'false'";
	int len;
	
	value = cleanupVariable(value, &len);
	
	*ok = false;
	switch (*value) {
		case '0':
			if (len == 1) {
				*ok = true;
				val = "FALSE";
			}
			break;
			
		case '1':
			if (len == 1) {
				*ok = true;
				val = "TRUE";
			}
			break;
			
		case 'T':
			if (!strcmp(value, "TRUE")) {
				*ok = true;
				val = "TRUE";
			}
			break;
			
		case 'F':
			if (!strcmp(value, "FALSE")) {
				*ok = true;
				val = "FALSE";
			}
			break;
			
				
	}
	
	return val;
}

//---------------------------
static const char *storageTypeCheck(char *value, bool *ok)
{
	const char *val = "Invalid storage type, try '"MS_REPOSITORY_STORAGE_TYPE"' or '"MS_CLOUD_STORAGE_TYPE"'";
	int len;
	
	value = cleanupVariable(value, &len);
	*ok = false;
	
	if (!strcmp(value, MS_REPOSITORY_STORAGE_TYPE)) {
		*ok = true;
		val = MS_REPOSITORY_STORAGE_TYPE;
	} else if (!strcmp(value, MS_CLOUD_STORAGE_TYPE)) {
		*ok = true;
		val = MS_CLOUD_STORAGE_TYPE;
	}
	
	return val;
}

//---------------------------
static void set_DumpRestore(MSDatabase *db, const char *value)
{
	enter_();
	push_(db);
	db->setRecovering((strcmp(value, "TRUE") == 0));
	release_(db);
	exit_();
}

//---------------------------
static void set_StorageType(MSDatabase *db, const char *value)
{
	enter_();
	push_(db);

	if (!strcmp(value, MS_REPOSITORY_STORAGE_TYPE))
		db->myBlobType = MS_STANDARD_STORAGE;
	else if (!strcmp(value, MS_CLOUD_STORAGE_TYPE))
		db->myBlobType = MS_CLOUD_STORAGE;

	release_(db);
	exit_();
}

//---------------------------
static const char *get_StorageType(MSDatabase *db, const char *)
{
	const char *value = "Unknown";
	enter_();
	push_(db);

	if (db->myBlobType == MS_STANDARD_STORAGE)
		value = MS_REPOSITORY_STORAGE_TYPE;
	else if (db->myBlobType == MS_CLOUD_STORAGE)
		value = MS_CLOUD_STORAGE_TYPE;

	release_(db);
	return_(value);
}

//---------------------------
static const char *get_S3CloudRefNo(MSDatabase *db, const char *)
{
	static char value[20];
	uint32_t	num;
	enter_();
	push_(db);
	
	num = db->myBlobCloud->cl_getDefaultCloudRef();
	snprintf(value, 20, "%"PRIu32"", num);
	
	release_(db);
	return_(value);
}

//---------------------------
static void set_S3CloudRefNo(MSDatabase *db, const char *value)
{
	enter_();
	push_(db);
	
	db->myBlobCloud->cl_setDefaultCloudRef(atol(value));
	
	release_(db);
	exit_();
}

//---------------------------
static void set_BackupNo(MSDatabase *db, const char *value)
{
	enter_();
	push_(db);
	
	db->myBlobCloud->cl_setRecoveryNumber(value);
	
	release_(db);
	exit_();
}

//---------------------------
static const char *get_BackupNo(MSDatabase *db, const char *)
{
	const char *value;
	enter_();
	push_(db);
	
	value = db->myBlobCloud->cl_getRecoveryNumber();
	
	release_(db);
	return_(value);
}

static PBMSVariableRec variables[] = {
	{false, "Storage-Type", MS_REPOSITORY_STORAGE_TYPE, "How the BLOB data is to be stored.", true, get_StorageType, storageTypeCheck, set_StorageType},
	{false, "S3-Cloud-Ref", NULL, "The S3 cloud reference id from the pbms.pbms_cloud table used for new BLOB storage.", true, get_S3CloudRefNo, NULL, set_S3CloudRefNo},
	{false, RESTORE_DUMP_VAR, "FALSE", "Indicate if the database is being restored from a dump file.", false, get_DumpRestore, boolCheck, set_DumpRestore},
	// Hidden variables should be placed at the end.
	{true, BACKUP_NUMBER_VAR, NULL, "The backup number for cloud blob data after a drag and drop restore.", true, get_BackupNo, readOnlyCheck, set_BackupNo}
};

static const uint32_t num_variables = 4;

//---------------------------
//----------------------------
#define PBMS_VARIABLES_FILE	"pbms_variables"
static  CSPath *getSysVarFile(CSString *db_path)
{
	CSPath			*path;

	enter_();
	
	push_(db_path);

	path = CSPath::newPath(RETAIN(db_path), PBMS_VARIABLES_FILE".dat");
	push_(path);
	if (!path->exists()) {
		CSPath *tmp_path;

		tmp_path = CSPath::newPath(RETAIN(db_path), PBMS_VARIABLES_FILE".tmp");
		push_(tmp_path);
		if (tmp_path->exists())
			tmp_path->rename(PBMS_VARIABLES_FILE".dat");
		release_(tmp_path);
	}
	
	
	pop_(path);
	release_(db_path);
	return_(path);
}

class LoadTableCleanUp : public CSRefObject {
	bool do_cleanup;
	CSThread *myself;
	
	uint32_t ref_id;

	public:
	
	LoadTableCleanUp(): CSRefObject(),
		do_cleanup(false), myself(NULL){}
		
	~LoadTableCleanUp() 
	{
		if (do_cleanup) {
			CSL.log(myself, CSLog::Protocol, "\nRestore failed!\n");
			CSL.flush();
			myself->logException();
		}
	}
	
	void setCleanUp(CSThread *self)
	{
		myself = self;
		do_cleanup = true;
	}
	
	void cancelCleanUp()
	{
		do_cleanup = false;
	}
	
};

void MSVariableTable::loadTable(MSDatabase *db)
{
	CSPath	*path;

	enter_();
	
	push_(db);
	path = getSysVarFile(RETAIN(db->myDatabasePath));
	push_(path);

	if (path->exists()) {
		CSFile			*file;
		CSStringBuffer	*string;
		size_t			size = 0, pos =0;
		char			*name, *value;
		
		new_(string, CSStringBuffer(20));
		push_(string);

		file = path->openFile(CSFile::READONLY);
		push_(file);
		size = file->getEOF();
		string->setLength(size);
		file->read(string->getBuffer(0), 0, size, size);
		release_(file);
				
		while (pos < size) {
			name = string->getBuffer(pos);
			pos += strlen(name) +1;
			if (pos >= size)
				break;
				
			value = string->getBuffer(pos);
			pos += strlen(value) +1;
			if (pos > size)
				break;
			
			for (uint32_t i =0; i < num_variables; i++) {
				if (variables[i].save && variables[i].action && !strcmp(name, variables[i].name)) {
					variables[i].action(RETAIN(db), value);
				}
			}
			
		}
		
		release_(string);

	} else { // Set the default values
		for (uint32_t i =0; i < num_variables; i++) {
			if (variables[i].value && variables[i].action) {
				variables[i].action(RETAIN(db), variables[i].value);
			}
		}
	}
	

	release_(path);
	
	// Check to see if there is cloud storage and if the database is not
	// currently recovering, then try to restore the BLOBs.	
	if ((db->myBlobType == MS_CLOUD_STORAGE) && db->myBlobCloud->cl_mustRecoverBlobs() && !db->isRecovering()) {
		CSL.log(self, CSLog::Protocol, "Restoring Cloud BLOBs for database: ");
		CSL.log(self, CSLog::Protocol, db->myDatabaseName->getCString());
		CSL.log(self, CSLog::Protocol, " ...");
		CSL.flush();
		LoadTableCleanUp *cleanup;
		
		new_(cleanup, LoadTableCleanUp());
		push_(cleanup);
		cleanup->setCleanUp(self);
		
		db->myBlobCloud->cl_restoreDB();
		
		cleanup->cancelCleanUp();
		release_(cleanup);

		CSL.log(self, CSLog::Protocol, "\nRestore done.\n");
		CSL.flush();
		set_BackupNo(RETAIN(db), "0");
		saveTable(RETAIN(db));
	}
	
	release_(db);

	exit_();
}

void MSVariableTable::saveTable(MSDatabase *db)
{
	CSPath			*path;
	CSPath			*old_path;
	CSFile			*file;
	const char		*value;
	size_t			offset = 0, len;
	char			null_char = 0;
	enter_();
	
	push_(db);
	path = CSPath::newPath(RETAIN(db->myDatabasePath), PBMS_VARIABLES_FILE".tmp");
	push_(path);
	file = path->openFile(CSFile::CREATE | CSFile::TRUNCATE);
	push_(file);
	
	for (uint32_t i = 0; i < num_variables; i++) {
		if (! variables[i].save) continue;
		
		len = strlen(variables[i].name)+1;
		file->write(variables[i].name, offset, len);
		offset += len;

		value = variables[i].get(RETAIN(db), variables[i].value);
		if (value) {
			len = strlen(value)+1;
			file->write(value, offset, len);
			offset += len;
		} else {
			file->write(&null_char, offset, 1);
			offset++;
		}		
	}
	file->close();
	release_(file);

	old_path = CSPath::newPath(RETAIN(db->myDatabasePath), PBMS_VARIABLES_FILE".dat");
	push_(old_path);
	if (old_path->exists())
		old_path->remove();
	path->rename(PBMS_VARIABLES_FILE".dat");
	release_(old_path);

	release_(path);
	release_(db);
	exit_();
}


MSVariableTable::MSVariableTable(MSSystemTableShare *share, TABLE *table):
MSOpenSystemTable(share, table),
iVariableIndex(0)
{
}

MSVariableTable::~MSVariableTable()
{
	//unuse();
}

void MSVariableTable::use()
{
	gVarLock.lock();
}

void MSVariableTable::unuse()
{
	gVarLock.unlock();	
}


void MSVariableTable::seqScanInit()
{
	iVariableIndex = 0;
}

bool MSVariableTable::seqScanNext(char *buf)
{
	TABLE		*table = mySQLTable;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;
	PBMSVariablePtr var;
	
	enter_();
	
	do {
		if (iVariableIndex >= num_variables)
			return_(false);
		var = &(variables[iVariableIndex++]);
	
	} while (var->hidden); 
	
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
			case 'I':
				ASSERT(strcmp(curr_field->field_name, "Id") == 0);
				curr_field->store(iVariableIndex, true);
				setNotNullInRecord(curr_field, buf);
				break;

			case 'N':
				ASSERT(strcmp(curr_field->field_name, "Name") == 0);
					curr_field->store(var->name, strlen(var->name), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				break;

			case 'V': {
				ASSERT(strcmp(curr_field->field_name, "Value") == 0);
					const char *value;
					value = var->get(RETAIN(myShare->mySysDatabase), var->value);
					if (value) {
						curr_field->store(value, strlen(value), &UTF8_CHARSET);
						setNotNullInRecord(curr_field, buf);
					}
				}
				break;

			case 'D':
				ASSERT(strcmp(curr_field->field_name, "Description") == 0);
					curr_field->store(var->info, strlen(var->info), &UTF8_CHARSET);
					setNotNullInRecord(curr_field, buf);
				break;

		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return_(true);
}

void MSVariableTable::seqScanPos(unsigned char *pos)
{
	int32_t index = iVariableIndex -1;
	if (index < 0)
		index = 0; // This is probably an error condition.
		
	mi_int4store(pos, index);
}

void MSVariableTable::seqScanRead(unsigned char *pos, char *buf)
{
	iVariableIndex = mi_uint4korr(pos);
	seqScanNext(buf);
}

void MSVariableTable::updateRow(char *old_data, char *new_data) 
{
	uint32_t n_id, o_id;
	String n_var_name, n_var_value;	
	String o_var_name;	
	const char *clean_value;

	enter_();
	
	getFieldValue(old_data, 0, &o_id);
	getFieldValue(old_data, 1, &o_var_name);
	
	getFieldValue(new_data, 0, &n_id);
	getFieldValue(new_data, 1, &n_var_name);
	getFieldValue(new_data, 2, &n_var_value);
	
	// The command names must match.
	if ((n_id != o_id) || my_strcasecmp(&UTF8_CHARSET, o_var_name.c_ptr_safe(), n_var_name.c_ptr_safe()))
		CSException::throwException(CS_CONTEXT, HA_ERR_TABLE_READONLY, "Attempt to update read only fields in the "VARIABLES_TABLE_NAME" table.");
		
	n_id--;
	if (n_id >  num_variables) // Should never happen
		CSException::throwException(CS_CONTEXT, HA_ERR_KEY_NOT_FOUND, "Invalid id");
	
	CSStringBuffer *value;	
	new_(value, CSStringBuffer(0));
	push_(value);
	value->append(n_var_value.c_ptr(), n_var_value.length());
	
	// check the input value converting it to a standard format where aplicable:
	if (variables[n_id].check) {
		bool ok = false;
		clean_value = variables[n_id].check(value->getCString(), &ok);
		if (!ok)
			CSException::throwException(CS_CONTEXT, HA_ERR_GENERIC, clean_value);
	} else
		clean_value = value->getCString();
		
	// Execute the action associated with the variable.
	if (variables[n_id].action) {
		variables[n_id].action(RETAIN(myShare->mySysDatabase), clean_value);
	}
	
	release_(value);
	
	if (variables[n_id].save) {
		saveTable(RETAIN(myShare->mySysDatabase));
	}
	
	exit_();
}

void MSVariableTable::transferTable(MSDatabase *to_db, MSDatabase *from_db)
{
	CSPath	*path;
	enter_();
	
	push_(from_db);
	push_(to_db);
	
	path = CSPath::newPath(RETAIN(from_db->myDatabasePath), PBMS_VARIABLES_FILE".dat");
	push_(path);
	if (path->exists()) {
		CSPath	*bu_path;
		bu_path = CSPath::newPath(RETAIN(to_db->myDatabasePath), PBMS_VARIABLES_FILE".dat");
		path->copyTo(bu_path, true);
	}
	
	release_(path);
	release_(to_db);
	release_(from_db);
	
	exit_();
}

void MSVariableTable::setVariable(MSDatabase *db, const char *name, const char *value)
{
	enter_();
	
	push_(db);
	
	for (uint32_t i =0; db && i < num_variables; i++) {
		if (variables[i].action && !strcmp(name, variables[i].name)) {
			variables[i].action(RETAIN(db), value);
			if (variables[i].save) {
				pop_(db);
				saveTable(db);
			} else
				release_(db);
			db = NULL;
		}
	}

	if (db) {
		release_(db);
		CSException::throwException(CS_CONTEXT, HA_ERR_KEY_NOT_FOUND, name);
	}
	exit_();
}

CSStringBuffer *MSVariableTable::dumpTable(MSDatabase *db)
{

	CSPath			*path;
	CSStringBuffer	*dump;

	enter_();
	
	push_(db);
	path = getSysVarFile(RETAIN(db->myDatabasePath));
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

void MSVariableTable::restoreTable(MSDatabase *db, const char *data, size_t size, bool reload)
{
	CSPath	*path;
	CSFile	*file;

	enter_();
	
	push_(db);
	path = getSysVarFile(RETAIN(db->myDatabasePath));
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

void MSVariableTable::removeTable(CSString *db_path)
{
	CSPath	*path;
	enter_();
	
	path = getSysVarFile(db_path);
	push_(path);
	
	path->removeFile();
	release_(path);
	exit_();
}

