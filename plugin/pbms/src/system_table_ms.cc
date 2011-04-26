/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
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
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-07-18
 *
 * H&G2JCtL
 *
 * System tables.
 *
 */

#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/session.h>
#include <drizzled/table.h>
#include <drizzled/field.h>
#include <drizzled/field/blob.h>

#include <drizzled/message/table.pb.h>
#include <drizzled/charset.h>
#include <drizzled/table_proto.h>
#endif


#include "cslib/CSConfig.h"
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>

//#include "mysql_priv.h"
//#include <plugin.h>

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "ha_pbms.h"

#include "mysql_ms.h"
#include "engine_ms.h"
#include "system_table_ms.h"
#include "repository_ms.h"
#include "database_ms.h"
#include "compactor_ms.h"
#include "open_table_ms.h"
#include "metadata_ms.h"
#ifdef HAVE_ALIAS_SUPPORT
#include "alias_ms.h"
#endif
#include "cloud_ms.h"
#include "transaction_ms.h"

#include "systab_httpheader_ms.h"
#include "systab_dump_ms.h"
#include "systab_variable_ms.h"
#include "systab_cloud_ms.h"
#include "systab_backup_ms.h"
#ifndef DRIZZLED
#include "systab_enabled_ms.h"
#endif
#include "discover_ms.h"
#include "parameters_ms.h"

///* Note: mysql_priv.h messes with new, which caused a crash. */
//#ifdef new
//#undef new
//#endif

/* Definitions for PBMS table discovery: */
//--------------------------------
static DT_FIELD_INFO pbms_repository_info[]=
{
#ifdef HAVE_ALIAS_SUPPORT
	{"Blob_alias",			BLOB_ALIAS_LENGTH, NULL, MYSQL_TYPE_VARCHAR,		&my_charset_utf8_bin,	0,								"The BLOB alias"},
#endif
	{"Repository_id",		NOVAL, NULL, MYSQL_TYPE_LONG,		NULL,					NOT_NULL_FLAG,					"The repository file number"},
	{"Repo_blob_offset",	NOVAL, NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,					"The offset of the BLOB in the repository file"},
	{"Blob_size",			NOVAL, NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,					"The size of the BLOB in bytes"},
	{"MD5_Checksum",		32,   NULL,	MYSQL_TYPE_VARCHAR,		system_charset_info,	0,								"The MD5 Digest of the BLOB data."},
	{"Head_size",			NOVAL, NULL, MYSQL_TYPE_SHORT,		NULL,					NOT_NULL_FLAG | UNSIGNED_FLAG,	"The size of the BLOB header - proceeds the BLOB data"},
	{"Access_code",			NOVAL, NULL, MYSQL_TYPE_LONG,		NULL,					NOT_NULL_FLAG,					"The 4-byte authorisation code required to access the BLOB - part of the BLOB URL"},
	{"Creation_time",		NOVAL, NULL, MYSQL_TYPE_TIMESTAMP,	NULL,					NOT_NULL_FLAG,					"The time the BLOB was created"},
	{"Last_ref_time",		NOVAL, NULL, MYSQL_TYPE_TIMESTAMP,	NULL,					0,								"The last time the BLOB was referenced"},
	{"Last_access_time",	NOVAL, NULL, MYSQL_TYPE_TIMESTAMP,	NULL,					0,								"The last time the BLOB was accessed (read)"},
	{"Access_count",		NOVAL, NULL, MYSQL_TYPE_LONG,		NULL,					NOT_NULL_FLAG,					"The count of the number of times the BLOB has been read"},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

#ifdef PBMS_HAS_KEYS
static DT_KEY_INFO pbms_repository_keys[]=
{
	{"pbms_repository_pk", PRI_KEY_FLAG, {"Repository_id", "Repo_blob_offset", NULL}},
	{NULL, 0, {NULL}}
};
#endif

static DT_FIELD_INFO pbms_metadata_info[]=
{
	{"Repository_id",		NOVAL,					NULL, MYSQL_TYPE_LONG,		NULL,							NOT_NULL_FLAG,	"The repository file number"},
	{"Repo_blob_offset",	NOVAL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,							NOT_NULL_FLAG,	"The offset of the BLOB in the repository file"},
	{"Name",				MS_META_NAME_SIZE,	NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,	NOT_NULL_FLAG,	"Metadata name"},
	{"Value",				MS_META_VALUE_SIZE,	NULL, MYSQL_TYPE_VARCHAR,	&UTF8_CHARSET,			NOT_NULL_FLAG,	"Metadata value"},
	{NULL,					NOVAL,					NULL, MYSQL_TYPE_STRING,	NULL, 0, NULL}
};

#ifdef PBMS_HAS_KEYS
static DT_KEY_INFO pbms_metadata_keys[]=
{
	{"pbms_metadata_pk", PRI_KEY_FLAG, {"Repository_id", "Repo_blob_offset", NULL}},
	{NULL, 0, {NULL}}
};
#endif


#ifdef HAVE_ALIAS_SUPPORT
static DT_FIELD_INFO pbms_alias_info[]=
{
	{"Repository_id",		NOVAL, NULL, MYSQL_TYPE_LONG,		NULL,				NOT_NULL_FLAG,	"The repository file number"},
	{"Repo_blob_offset",	NOVAL, NULL, MYSQL_TYPE_LONGLONG,	NULL,				NOT_NULL_FLAG,	"The offset of the BLOB in the repository file"},
	{"Blob_alias",			BLOB_ALIAS_LENGTH, NULL, MYSQL_TYPE_VARCHAR,		&my_charset_utf8_bin,	NOT_NULL_FLAG,			"The BLOB alias"},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

static DT_KEY_INFO pbms_alias_keys[]=
{
	{"pbms_alias_pk", PRI_KEY_FLAG, {"Repository_id", "Repo_blob_offset", NULL}},
	{NULL, 0, {NULL}}
};
#endif

static DT_FIELD_INFO pbms_blobs_info[]=
{
	{"Repository_id",		NOVAL, NULL, MYSQL_TYPE_LONG,		NULL,				NOT_NULL_FLAG,	"The repository file number"},
	{"Repo_blob_offset",	NOVAL, NULL, MYSQL_TYPE_LONGLONG,	NULL,				NOT_NULL_FLAG,	"The offset of the BLOB in the repository file"},
	{"Blob_data",			NOVAL, NULL, MYSQL_TYPE_LONG_BLOB,	&my_charset_bin,	NOT_NULL_FLAG,	"The data of this BLOB"},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

#ifdef PBMS_HAS_KEYS
static DT_KEY_INFO pbms_blobs_keys[]=
{
	{"pbms_blobs_pk", PRI_KEY_FLAG, {"Repository_id", "Repo_blob_offset", NULL}},
	{NULL, 0, {NULL}}
};
#endif

static DT_FIELD_INFO pbms_reference_info[]=
{
	{"Table_name",		MS_TABLE_NAME_SIZE,		NULL, MYSQL_TYPE_STRING,	system_charset_info,	0,	"The name of the referencing table"},
	{"Column_ordinal",	NOVAL,					NULL, MYSQL_TYPE_LONG,		NULL,					0,	"The column ordinal of the referencing field"},
	{"Blob_id",			NOVAL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,	"The BLOB reference number - part of the BLOB URL"},
	{"Blob_url",		PBMS_BLOB_URL_SIZE,		NULL, MYSQL_TYPE_VARCHAR,	system_charset_info,	0,	"The BLOB URL for HTTP GET access"},
	{"Repository_id",	NOVAL,					NULL, MYSQL_TYPE_LONG,		NULL,					NOT_NULL_FLAG,	"The repository file number of the BLOB"},
	{"Repo_blob_offset",NOVAL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,	"The offset in the repository file"},
	{"Blob_size",		NOVAL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					NOT_NULL_FLAG,	"The size of the BLOB in bytes"},
	{"Deletion_time",	NOVAL,					NULL, MYSQL_TYPE_TIMESTAMP,	NULL,					0,				"The time the BLOB was deleted"},
	{"Remove_in",		NOVAL,					NULL, MYSQL_TYPE_LONG,		NULL,					0,				"The number of seconds before the reference/BLOB is removed perminently"},
	{"Temp_log_id",		NOVAL,					NULL, MYSQL_TYPE_LONG,		NULL,					0,				"Temporary log number of the referencing deletion entry"},
	{"Temp_log_offset",	NOVAL,					NULL, MYSQL_TYPE_LONGLONG,	NULL,					0,				"Temporary log offset of the referencing deletion entry"},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

#ifdef PBMS_HAS_KEYS
static DT_KEY_INFO pbms_reference_keys[]=
{
	{"pbms_reference_pk", PRI_KEY_FLAG, {"Table_name", "Blob_id", NULL}},
	{"pbms_reference_k", MULTIPLE_KEY_FLAG, {"Repository_id", "Repo_blob_offset", NULL}},
	{NULL, 0, {NULL}}
};
#endif


typedef enum {	SYS_REP = 0, 
				SYS_REF, 
				SYS_BLOB, 
				SYS_DUMP, 
				SYS_META, 
				SYS_HTTP, 
#ifdef HAVE_ALIAS_SUPPORT
				SYS_ALIAS, 
#endif
				SYS_VARIABLE, 
				SYS_CLOUD, 
				SYS_BACKUP, 
#ifndef DRIZZLED
				SYS_ENABLED, 
#endif
				SYS_UNKNOWN} SysTableType;
				
static const char *sysTableNames[] = {
	"pbms_repository",
	"pbms_reference",
	"pbms_blob",
	"pbms_dump",
	"pbms_metadata",
	METADATA_HEADER_NAME,
#ifdef HAVE_ALIAS_SUPPORT
	"pbms_alias",
#endif
	VARIABLES_TABLE_NAME,
	CLOUD_TABLE_NAME,
	BACKUP_TABLE_NAME,
#ifndef DRIZZLED
	ENABLED_TABLE_NAME,
#endif
	NULL
};

static INTERRNAL_TABLE_INFO pbms_internal_tables[]=
{
#ifdef PBMS_HAS_KEYS
	{ false, sysTableNames[SYS_REP],pbms_repository_info, pbms_repository_keys},
	{ false, sysTableNames[SYS_REF], pbms_reference_info, pbms_reference_keys},
	{ false, sysTableNames[SYS_BLOB], pbms_blobs_info, pbms_blobs_keys},
	{ false, sysTableNames[SYS_DUMP], pbms_dump_info, pbms_dump_keys},
	{ false, sysTableNames[SYS_META], pbms_metadata_info, pbms_metadata_keys},
	{ false, sysTableNames[SYS_HTTP], pbms_metadata_headers_info, pbms_metadata_headers_keys},
#ifdef HAVE_ALIAS_SUPPORT
	{ false, sysTableNames[SYS_ALIAS], pbms_alias_info, pbms_alias_keys},
#endif
	{ false, sysTableNames[SYS_VARIABLE], pbms_variable_info, pbms_variable_keys},
	{ true, sysTableNames[SYS_CLOUD], pbms_cloud_info, pbms_cloud_keys},
	{ true, sysTableNames[SYS_BACKUP], pbms_backup_info, pbms_backup_keys},
#ifndef DRIZZLED
	{ true, sysTableNames[SYS_ENABLED], pbms_enabled_info, pbms_enabled_keys},
#endif
#else
	{ false, sysTableNames[SYS_REP], pbms_repository_info, NULL},
	{ false, sysTableNames[SYS_REF], pbms_reference_info, NULL},
	{ false, sysTableNames[SYS_BLOB], pbms_blobs_info, NULL},
	{ false, sysTableNames[SYS_DUMP], pbms_dump_info, NULL},
	{ false, sysTableNames[SYS_META], pbms_metadata_info, NULL},
	{ false, sysTableNames[SYS_HTTP], pbms_metadata_headers_info, NULL},
#ifdef HAVE_ALIAS_SUPPORT
	{ false, sysTableNames[SYS_ALIAS], pbms_alias_info, NULL},
#endif
	{ false, sysTableNames[SYS_VARIABLE], pbms_variable_info, NULL},
	{ true, sysTableNames[SYS_CLOUD], pbms_cloud_info, NULL},
	{ true, sysTableNames[SYS_BACKUP], pbms_backup_info, NULL},
#ifndef DRIZZLED
	{ true, sysTableNames[SYS_ENABLED], pbms_enabled_info, NULL},
#endif
#endif

	{ false, NULL, NULL, NULL}
	
};

//--------------------------
static SysTableType pbms_systable_type(const char *table)
{
	int i = 0;
	
	while ((i < SYS_UNKNOWN) && strcasecmp(table, sysTableNames[i])) i++;
	
	return((SysTableType) i );
}

//--------------------------
bool PBMSSystemTables::isSystemTable(bool isPBMS, const char *table)
{
	SysTableType i;
	
	i = pbms_systable_type(table);

	if (i == SYS_UNKNOWN)
		return false;
		
	return (pbms_internal_tables[i].is_pbms == isPBMS);
}

//--------------------------
#ifdef DRIZZLED
using namespace std;
using namespace drizzled;
#undef TABLE
#undef Field
static int pbms_create_proto_table(const char *engine_name, const char *name, DT_FIELD_INFO *info, DT_KEY_INFO *keys, drizzled::message::Table &table)
{
	message::Table::Field *field;
	message::Table::Field::FieldConstraints *field_constraints;
	message::Table::Field::StringFieldOptions *string_field_options;
	message::Table::TableOptions *table_options;

	table.set_name(name);
	table.set_name(name);
	table.set_type(message::Table::STANDARD);
	table.mutable_engine()->set_name(engine_name);
	
	table_options = table.mutable_options();
	table_options->set_collation_id(my_charset_utf8_bin.number);
	table_options->set_collation(my_charset_utf8_bin.name);
	
	while (info->field_name) {	
		field= table.add_field();
		
		field->set_name(info->field_name);
		if (info->comment)
			field->set_comment(info->comment);
			
		field_constraints= field->mutable_constraints();
		if (info->field_flags & NOT_NULL_FLAG)
			field_constraints->set_is_notnull(true);
		
		if (info->field_flags & UNSIGNED_FLAG)
			field_constraints->set_is_unsigned(true);
		else
			field_constraints->set_is_unsigned(false);

		switch (info->field_type) {
			case DRIZZLE_TYPE_VARCHAR:
				string_field_options = field->mutable_string_options();
				
				field->set_type(message::Table::Field::VARCHAR);
				string_field_options->set_length(info->field_length);
				if (info->field_charset) {
					string_field_options->set_collation(info->field_charset->name);
					string_field_options->set_collation_id(info->field_charset->number);
				}
				break;
				
			case DRIZZLE_TYPE_LONG:
				field->set_type(message::Table::Field::INTEGER);
				break;
				
			case DRIZZLE_TYPE_DOUBLE:
				field->set_type(message::Table::Field::DOUBLE);
				break;
				
			case DRIZZLE_TYPE_LONGLONG:
				field->set_type(message::Table::Field::BIGINT);
				break;
				
			case DRIZZLE_TYPE_TIMESTAMP:
				field->set_type(message::Table::Field::EPOCH);
				break;
				
			case DRIZZLE_TYPE_BLOB:
				field->set_type(message::Table::Field::BLOB);
				if (info->field_charset) {
					string_field_options = field->mutable_string_options();
					string_field_options->set_collation(info->field_charset->name);
					string_field_options->set_collation_id(info->field_charset->number);
				}
				break;
				
			default:
				assert(0); 
		}
		info++;
	}
	
			
	if (keys) {
		while (keys->key_name) {
			// To be done later. (maybe)
			keys++;
		}
	}

	return 0;
}
#define TABLE								drizzled::Table
#define Field								drizzled::Field

int PBMSSystemTables::getSystemTableInfo(const char *name, drizzled::message::Table &table)
{
	int err = 1, i = 0;
			
	while (pbms_internal_tables[i].name) {
		if (strcasecmp(name, pbms_internal_tables[i].name) == 0){
			err = pbms_create_proto_table("PBMS", name, pbms_internal_tables[i].info, pbms_internal_tables[i].keys, table);
			break;
		}
		i++;
	}
	
	return err;
}

void PBMSSystemTables::getSystemTableNames(bool isPBMS, std::set<std::string> &set_of_names)
{
	int i = 0;
			
	while (pbms_internal_tables[i].name) {
		if ( isPBMS == pbms_internal_tables[i].is_pbms){
			set_of_names.insert(pbms_internal_tables[i].name);
		}
		i++;
	}
	
}

#else // DRIZZLED
//--------------------------
static bool pbms_database_follder_exists( const char *db)
{
	struct stat stat_info;	
	char path[PATH_MAX];
	
	if (!db)
		return false;
		
	cs_strcpy(PATH_MAX, path, ms_my_get_mysql_home_path());
	cs_add_name_to_path(PATH_MAX, path, db);
	
	if (stat(path, &stat_info) == 0)
		return(stat_info.st_mode & S_IFDIR);
		
	return false;
}

int pbms_discover_system_tables(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen)
{
	int err = 1, i = 0;
	bool is_pbms = false;

	// Check that the database exists!
	if (!pbms_database_follder_exists(db))
		return err;
		
	is_pbms = (strcmp(db, "pbms") == 0);
		
	
	while (pbms_internal_tables[i].name) {
		if ((!strcasecmp(name, pbms_internal_tables[i].name)) && ( is_pbms == pbms_internal_tables[i].is_pbms)){
			err = ms_create_table_frm(hton, thd, db, name, pbms_internal_tables[i].info, pbms_internal_tables[i].keys, frmblob, frmlen);
			break;
		}
		i++;
	}
	
	return err;
}
#endif // DRIZZLED

// Transfer any physical PBMS ststem tables to another database.
void PBMSSystemTables::transferSystemTables(MSDatabase *dst_db, MSDatabase *src_db)
{
	enter_();
	push_(dst_db);
	push_(src_db);
	
	MSHTTPHeaderTable::transferTable(RETAIN(dst_db), RETAIN(src_db));
	MSVariableTable::transferTable(RETAIN(dst_db), RETAIN(src_db));
	MSCloudTable::transferTable(RETAIN(dst_db), RETAIN(src_db));
	MSBackupTable::transferTable(RETAIN(dst_db), RETAIN(src_db));
	
	release_(src_db);
	release_(dst_db);
	exit_();
}

//----------------
void PBMSSystemTables::removeSystemTables(CSString *db_path)
{
	enter_();
	push_(db_path);
	
	MSHTTPHeaderTable::removeTable(RETAIN(db_path));
	MSVariableTable::removeTable(RETAIN(db_path));
	MSCloudTable::removeTable(RETAIN(db_path));
	MSBackupTable::removeTable(RETAIN(db_path));
	
	release_(db_path);
	exit_();
}

//----------------
bool PBMSSystemTables::try_loadSystemTables(CSThread *self, int i, MSDatabase *db)
{
	volatile bool rtc = true;
	try_(a) {
		switch (i) {
			case 0:
				MSHTTPHeaderTable::loadTable(RETAIN(db));
				break;
			case 1:
				MSCloudTable::loadTable(RETAIN(db));
				break;
			case 2:
				MSBackupTable::loadTable(RETAIN(db));
				break;
			case 3:
				// Variable must be loaded after cloud and backup info
				// incase BLOB recovery is required.
				MSVariableTable::loadTable(RETAIN(db)); 
				break;
				
			default:
				ASSERT(false);
		}
		rtc = false;
	}
	catch_(a);
	cont_(a);
	return rtc;
}
//----------------
void PBMSSystemTables::loadSystemTables(MSDatabase *db)
{
	enter_();
	push_(db);
	
	for ( int i = 0; i < 4; i++) {
		if (try_loadSystemTables(self, i, db))
			self->logException();			
	}
	
	release_(db);
	exit_();
}

//----------------
// Dump all the system tables into one buffer.
typedef struct {
	CSDiskValue4 size_4;
	CSDiskValue1 tab_id_1;
	CSDiskValue4 tab_version_4;
} DumpHeaderRec, *DumpHeaderPtr;

typedef union {
		char *rec_chars;
		const char *rec_cchars;
		DumpHeaderPtr dumpHeader;
} DumpDiskData;

CSStringBuffer *PBMSSystemTables::dumpSystemTables(MSDatabase *db)
{
	CSStringBuffer *sysDump, *tabDump = NULL;
	uint32_t size, pos, tab_version;
	uint8_t tab_id = 0;
	DumpDiskData	d;

	enter_();
	push_(db);
	new_(sysDump, CSStringBuffer());
	push_(sysDump);
	pos = 0;
	
	for ( int i = 0; i < 4; i++) {
		switch (i) {
			case 0:
				tabDump = MSHTTPHeaderTable::dumpTable(RETAIN(db));
				tab_id = MSHTTPHeaderTable::tableID;
				tab_version = MSHTTPHeaderTable::tableVersion;
				break;
			case 1:
				tabDump = MSCloudTable::dumpTable(RETAIN(db));
				tab_id = MSCloudTable::tableID;
				tab_version = MSCloudTable::tableVersion;
				break;
			case 2:
				tabDump = MSBackupTable::dumpTable(RETAIN(db));
				tab_id = MSBackupTable::tableID;
				tab_version = MSBackupTable::tableVersion;
				break;
			case 3:
				tabDump = MSVariableTable::dumpTable(RETAIN(db)); // Dump the variables table last.
				tab_id = MSVariableTable::tableID;
				tab_version = MSVariableTable::tableVersion;
				break;
				
			default:
				ASSERT(false);
		}
		
		push_(tabDump);
		size = tabDump->length();
		
		// Grow the buffer for the header
		sysDump->setLength(pos + sizeof(DumpHeaderRec));
		
		// Add the dump header 
		d.rec_chars = sysDump->getBuffer(pos); 
		CS_SET_DISK_4(d.dumpHeader->size_4, size);;		
		CS_SET_DISK_1(d.dumpHeader->tab_id_1, tab_id); 		
		CS_SET_DISK_4(d.dumpHeader->tab_version_4, tab_version); 	
	
		sysDump->append(tabDump->getBuffer(0), size);
		pos += size + sizeof(DumpHeaderRec);
		release_(tabDump);
	}
	
	
	pop_(sysDump);
	release_(db);
	return_(sysDump);
}

//----------------
void PBMSSystemTables::restoreSystemTables(MSDatabase *db, const char *data, size_t size)
{
	uint32_t tab_size, tab_version;
	uint8_t tab_id;
	DumpDiskData	d;

	enter_();
	push_(db);
	
	while  ( size >= sizeof(DumpHeaderRec)) {
		d.rec_cchars = data;
		tab_size = CS_GET_DISK_4(d.dumpHeader->size_4);
		tab_id = CS_GET_DISK_1(d.dumpHeader->tab_id_1); 		
		tab_version = CS_GET_DISK_4(d.dumpHeader->tab_version_4); 	
		data += sizeof(DumpHeaderRec);
		size -= sizeof(DumpHeaderRec);
		
		if (size < tab_size) {
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "PBMS system table restore data truncated.");
		}
		
		switch (tab_id) {
			case MSHTTPHeaderTable::tableID:
				if (MSHTTPHeaderTable::tableVersion == tab_version)
					MSHTTPHeaderTable::restoreTable(RETAIN(db), data, tab_size);
				else
					CSException::logException(CS_CONTEXT, MS_ERR_SYSTAB_VERSION, "Restore "METADATA_HEADER_NAME" failed, incompatible table version" );
				break;
			case MSCloudTable::tableID:
				if (MSCloudTable::tableVersion == tab_version)
					MSCloudTable::restoreTable(RETAIN(db), data, tab_size);
				else
					CSException::logException(CS_CONTEXT, MS_ERR_SYSTAB_VERSION, "Restore "CLOUD_TABLE_NAME" failed, incompatible table version" );
				break;
			case MSBackupTable::tableID:
				if (MSBackupTable::tableVersion == tab_version)
					MSBackupTable::restoreTable(RETAIN(db), data, tab_size);
				else
					CSException::logException(CS_CONTEXT, MS_ERR_SYSTAB_VERSION, "Restore "BACKUP_TABLE_NAME" failed, incompatible table version" );
				break;				
			case MSVariableTable::tableID:
				if (MSVariableTable::tableVersion == tab_version)
					MSVariableTable::restoreTable(RETAIN(db), data, tab_size);
				else
					CSException::logException(CS_CONTEXT, MS_ERR_SYSTAB_VERSION, "Restore "VARIABLES_TABLE_NAME" failed, incompatible table version" );
				break;
			default:
				ASSERT(false);
		}
		 data += tab_size;
		 size -= tab_size;
	}
	
	if (size) {
		CSException::logException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "PBMS trailing garbage in system table restore data being ignored.");
	}
		
	release_(db);
	exit_();
}

/*
 * -------------------------------------------------------------------------
 * MYSQL UTILITIES
 */

void MSOpenSystemTable::setNotNullInRecord(Field *field, char *record)
{
#ifdef DRIZZLED
	if (field->null_ptr)
		record[(uint) (field->null_ptr - (uchar *) field->getTable()->getInsertRecord())] &= (uchar) ~field->null_bit;
#else
	if (field->null_ptr)
		record[(uint) (field->null_ptr - (uchar *) field->table->record[0])] &= (uchar) ~field->null_bit;
#endif
}

/*
 * -------------------------------------------------------------------------
 * OPEN SYSTEM TABLES
 */

MSOpenSystemTable::MSOpenSystemTable(MSSystemTableShare *share, TABLE *table):
CSRefObject()
{
	myShare = share;
	mySQLTable = table;
}

MSOpenSystemTable::~MSOpenSystemTable()
{
	MSSystemTableShare::releaseSystemTable(this);
}


/*
 * -------------------------------------------------------------------------
 * REPOSITORY TABLE
 */

MSRepositoryTable::MSRepositoryTable(MSSystemTableShare *share, TABLE *table):
MSOpenSystemTable(share, table),
iCompactor(NULL),
iRepoFile(NULL),
iBlobBuffer(NULL)
{
}

//-----------------------
MSRepositoryTable::~MSRepositoryTable()
{
	unuse();
	
	if (iBlobBuffer)
		iBlobBuffer->release();
}

//-----------------------
void MSRepositoryTable::use()
{
	MSDatabase *db;
	enter_();

	if (!iBlobBuffer)
		new_(iBlobBuffer, CSStringBuffer(20));
		
	db = myShare->mySysDatabase;
	if ((iCompactor = db->getCompactorThread())) {
		if (iCompactor->isMe(self))
			iCompactor = NULL;
		else {
			iCompactor->retain();
			iCompactor->suspend();
		}
	}
	exit_();
}

//-----------------------
void MSRepositoryTable::unuse()
{
	if (iCompactor) {
		iCompactor->resume();
		iCompactor->release();
		iCompactor = NULL;
	}
	if (iRepoFile) {
		iRepoFile->release();
		iRepoFile = NULL;
	}
	if (iBlobBuffer)
		iBlobBuffer->clear();
}


//-----------------------
void MSRepositoryTable::seqScanInit()
{
	enter_();
	
	// Flush all committed transactions to the repository file.
	MSTransactionManager::flush();

	iRepoIndex = 0;
	iRepoOffset = 0;
		
	exit_();
}

//-----------------------
bool MSRepositoryTable::resetScan(bool positioned, uint32_t repo_index)
{
	if (positioned) {
		if (iRepoFile && (repo_index != iRepoIndex)) {
			iRepoFile->release();
			iRepoFile = NULL;
		}
		
		iRepoIndex = repo_index;
	}
	if (iRepoFile) 
		return true;
		
	enter_();
	MSRepository	*repo = NULL;
	CSSyncVector	*repo_list = myShare->mySysDatabase->getRepositoryList();

	lock_(repo_list);
	for (; iRepoIndex<repo_list->size(); iRepoIndex++) {
		if ((repo = (MSRepository *) repo_list->get(iRepoIndex))) {
			iRepoFile = repo->openRepoFile();
			break;
		}
	}
	unlock_(repo_list);
	
	if (!iRepoFile)
		return_(false);
	iRepoFileSize = repo->getRepoFileSize();
	if ((!iRepoOffset) || !positioned) 
		iRepoOffset = repo->getRepoHeadSize();
		
	iRepoCurrentOffset = iRepoOffset;
		
	return_(true);
}

//-----------------------
bool MSRepositoryTable::seqScanNext(char *buf)
{

	enter_();
	iRepoCurrentOffset = iRepoOffset;
	
	if (returnSubRecord(buf))
		goto exit;

	restart:
	if ((!iRepoFile) && !MSRepositoryTable::resetScan(false))
		return false;

	while (iRepoOffset < iRepoFileSize) {
		if (returnRecord(buf))
			goto exit;
	}

	if (iRepoFile) {
		iRepoFile->release();
		iRepoFile = NULL;
		iRepoOffset = 0;
	}
	iRepoIndex++;
	goto restart;

	exit:
	return_(true);
}

//-----------------------
int	MSRepositoryTable::getRefLen()
{
	return sizeof(iRepoIndex) + sizeof(iRepoOffset);
}


//-----------------------
void MSRepositoryTable::seqScanPos(unsigned char *pos)
{
	mi_int4store(pos, iRepoIndex); pos +=4;
	mi_int8store(pos, iRepoCurrentOffset);
}

//-----------------------
void MSRepositoryTable::seqScanRead(uint32_t repo, uint64_t offset, char *buf)
{
	iRepoOffset = offset;

	if (!resetScan(true, repo))
		return;
		
	seqScanNext(buf);
}

//-----------------------
void MSRepositoryTable::seqScanRead(unsigned char *pos, char *buf)
{
	seqScanRead(mi_uint4korr( pos), mi_uint8korr(pos +4), buf);
}

//-----------------------
bool MSRepositoryTable::returnRecord(char *buf)
{
	CSMutex			*lock;
	MSBlobHeadRec	blob;
	uint16_t			head_size;
	uint64_t			blob_size;
	int				ref_count;
	size_t			ref_size;
	uint8_t			status;

	enter_();
	retry_read:
	lock = iRepoFile->myRepo->getRepoLock(iRepoOffset);
	lock_(lock);
	if (iRepoFile->read(&blob, iRepoOffset, sizeof(MSBlobHeadRec), 0) < sizeof(MSBlobHeadRec)) {
		unlock_(lock);
		iRepoOffset = iRepoFileSize;
		return_(false);
	}
	head_size = CS_GET_DISK_2(blob.rb_head_size_2);
	blob_size = CS_GET_DISK_6(blob.rb_blob_repo_size_6);
	ref_size = CS_GET_DISK_1(blob.rb_ref_size_1);
	ref_count = CS_GET_DISK_2(blob.rb_ref_count_2);
	status = CS_GET_DISK_1(blob.rb_status_1);
	if (ref_count <= 0 || ref_size == 0 ||
		head_size < iRepoFile->myRepo->getRepoBlobHeadSize() + ref_count * ref_size || 
		!VALID_BLOB_STATUS(status)) {
		/* Can't be true. Assume this is garbage! */
		unlock_(lock);
		iRepoOffset++;
		goto retry_read;
	}

	if (IN_USE_BLOB_STATUS(status)) {
		unlock_(lock);
		if (!returnRow(&blob, buf)) {
			/* This record may not have had any data of interest. */
			iRepoOffset++;
			goto retry_read;
		}
		iRepoOffset += head_size + blob_size;
		return_(true);
	}
	unlock_(lock);
	iRepoOffset += head_size + blob_size;
	return_(false);
}

//-----------------------
bool MSRepositoryTable::returnSubRecord(char *)
{
	return false;
}

//-----------------------
bool MSRepositoryTable::returnRow(MSBlobHeadPtr	blob, char *buf)
{
	TABLE		*table = mySQLTable;
	uint8_t		storage_type;
	uint32_t		access_count;
	uint32_t		last_access;
	uint32_t		last_ref;
	uint32_t		creation_time;
	uint32_t		access_code;
	uint16_t		head_size;
	uint16_t		alias_offset;
	uint64_t		blob_size;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;
	
	enter_();

	storage_type = CS_GET_DISK_1(blob->rb_storage_type_1);
	last_access = CS_GET_DISK_4(blob->rb_last_access_4);
	access_count = CS_GET_DISK_4(blob->rb_access_count_4);
	last_ref = CS_GET_DISK_4(blob->rb_last_ref_4);
	creation_time = CS_GET_DISK_4(blob->rb_create_time_4);
	head_size = CS_GET_DISK_2(blob->rb_head_size_2);
	blob_size = CS_GET_DISK_6(blob->rb_blob_repo_size_6);
	access_code = CS_GET_DISK_4(blob->rb_auth_code_4);
	alias_offset = CS_GET_DISK_2(blob->rb_alias_offset_2);

	/* ASSERT_COLUMN_MARKED_FOR_WRITE is failing when
	 * I use store()!??
	 * But I want to use it! :(
	 */
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
			case 'A':
				switch (curr_field->field_name[9]) {
					case 'd':
						ASSERT(strcmp(curr_field->field_name, "Access_code") == 0);
						curr_field->store(access_code, true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'u':
						ASSERT(strcmp(curr_field->field_name, "Access_count") == 0);
						curr_field->store(access_count, true);
						setNotNullInRecord(curr_field, buf);
						break;
				}
				break;
			case 'R':
				switch (curr_field->field_name[6]) {
					case 't':
						// Repository_id     INT
						ASSERT(strcmp(curr_field->field_name, "Repository_id") == 0);
						curr_field->store(iRepoFile->myRepo->getRepoID(), true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'l':
						// Repo_blob_offset  BIGINT
						ASSERT(strcmp(curr_field->field_name, "Repo_blob_offset") == 0);
						curr_field->store(iRepoOffset, true);
						setNotNullInRecord(curr_field, buf);
						break;
				}
				break;
			case 'B':
				switch (curr_field->field_name[5]) {
					case 's':
						// Blob_size         BIGINT
						ASSERT(strcmp(curr_field->field_name, "Blob_size") == 0);
						curr_field->store(blob_size, true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'a':
						// Blob_alias         
						ASSERT(strcmp(curr_field->field_name, "Blob_alias") == 0);
#ifdef HAVE_ALIAS_SUPPORT
						if (alias_offset) {
							char blob_alias[BLOB_ALIAS_LENGTH +1];
							CSMutex	*lock;
							uint64_t rsize;
							
							lock = iRepoFile->myRepo->getRepoLock(iRepoOffset);
							lock_(lock);
							rsize = iRepoFile->read(blob_alias, iRepoOffset + alias_offset, BLOB_ALIAS_LENGTH, 0);
							unlock_(lock);
							blob_alias[rsize] = 0;
							curr_field->store(blob_alias, strlen(blob_alias), &my_charset_utf8_general_ci);
							setNotNullInRecord(curr_field, buf);
						} else {
							curr_field->store((uint64_t) 0, true);
						}
#else
						curr_field->store((uint64_t) 0, true);
#endif
						
						break;
				}

				break;
			case 'M': // MD5_Checksum
				if (storage_type == MS_STANDARD_STORAGE) {
					char checksum[sizeof(Md5Digest) *2 +1];
					
					ASSERT(strcmp(curr_field->field_name, "MD5_Checksum") == 0);
					cs_bin_to_hex(sizeof(Md5Digest) *2 +1, checksum, sizeof(Md5Digest), &(blob->rb_blob_checksum_md5d));
					curr_field->store(checksum, sizeof(Md5Digest) *2, system_charset_info);
					setNotNullInRecord(curr_field, buf);
					
				} else
					curr_field->store((uint64_t) 0, true);
			
				break;
			case 'H':
				// Head_size         SMALLINT UNSIGNED
				ASSERT(strcmp(curr_field->field_name, "Head_size") == 0);
				curr_field->store(head_size, true);
				setNotNullInRecord(curr_field, buf);
				break;
			case 'C':
				// Creation_time     TIMESTAMP
				ASSERT(strcmp(curr_field->field_name, "Creation_time") == 0);
				curr_field->store(ms_my_1970_to_mysql_time(creation_time), true);
				setNotNullInRecord(curr_field, buf);
				break;
			case 'L':
				switch (curr_field->field_name[5]) {
					case 'r':
						// Last_ref_time     TIMESTAMP
						ASSERT(strcmp(curr_field->field_name, "Last_ref_time") == 0);
						curr_field->store(ms_my_1970_to_mysql_time(last_ref), true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'a':
						// Last_access_time  TIMESTAMP
						ASSERT(strcmp(curr_field->field_name, "Last_access_time") == 0);
						curr_field->store(ms_my_1970_to_mysql_time(last_access), true);
						setNotNullInRecord(curr_field, buf);
						break;
				}
				break;
		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return_(true);
}

/*
 * -------------------------------------------------------------------------
 * BLOB DATA TABLE
 */
//-----------------------
MSBlobDataTable::MSBlobDataTable(MSSystemTableShare *share, TABLE *table):MSRepositoryTable(share, table)
{
}

//-----------------------
MSBlobDataTable::~MSBlobDataTable()
{	
}

//-----------------------
bool MSBlobDataTable::returnRow(MSBlobHeadPtr blob, char *buf)
{
	TABLE		*table = mySQLTable;
	uint8_t		blob_type;
	uint16_t		head_size;
	uint64_t		blob_size;
	uint32		len;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;

	head_size = CS_GET_DISK_2(blob->rb_head_size_2);
	blob_size = CS_GET_DISK_6(blob->rb_blob_repo_size_6);
	blob_type = CS_GET_DISK_1(blob->rb_storage_type_1);

	/* ASSERT_COLUMN_MARKED_FOR_WRITE is failing when
	 * I use store()!??
	 * But I want to use it! :(
	 */
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
			case 'R':
				switch (curr_field->field_name[6]) {
					case 't':
						// Repository_id     INT
						ASSERT(strcmp(curr_field->field_name, "Repository_id") == 0);
						curr_field->store(iRepoFile->myRepo->getRepoID(), true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'l':
						// Repo_blob_offset  BIGINT
						ASSERT(strcmp(curr_field->field_name, "Repo_blob_offset") == 0);
						curr_field->store(iRepoOffset, true);
						setNotNullInRecord(curr_field, buf);
						break;
				}
				break;
			case 'B':
				// Blob_data         LONGBLOB
				ASSERT(strcmp(curr_field->field_name, "Blob_data") == 0);
				if (blob_size <= 0xFFFFFFF) {
					iBlobBuffer->setLength((uint32_t) blob_size);
					if (BLOB_IN_REPOSITORY(blob_type)) {
						len = iRepoFile->read(iBlobBuffer->getBuffer(0), iRepoOffset + head_size, (size_t) blob_size, 0);
					} else {
						CloudDB *blobCloud = myShare->mySysDatabase->myBlobCloud;
						CloudKeyRec key;
						ASSERT(blobCloud != NULL);
						
						MSRepoFile::getBlobKey(blob, &key);
						
						len = blobCloud->cl_getData(&key, iBlobBuffer->getBuffer(0), blob_size);
					}
					((Field_blob *) curr_field)->set_ptr(len, (byte *) iBlobBuffer->getBuffer(0));
					setNotNullInRecord(curr_field, buf);
				}
				break;
		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return true;
}

#ifdef HAVE_ALIAS_SUPPORT
/*
 * -------------------------------------------------------------------------
 * Alias DATA TABLE
 */
bool MSBlobAliasTable::returnRow(MSBlobHeadPtr blob, char *buf)
{
	TABLE		*table = mySQLTable;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;
	uint16_t		alias_offset; 
	char		blob_alias[BLOB_ALIAS_LENGTH +1];
	CSMutex		*lock;
	uint64_t		rsize;
	enter_();
	
	alias_offset = CS_GET_DISK_2(blob->rb_alias_offset_2);

	if (!alias_offset)
		return_(false);

	lock = iRepoFile->myRepo->getRepoLock(iRepoOffset);
	lock_(lock);
	rsize = iRepoFile->read(blob_alias, iRepoOffset + alias_offset, BLOB_ALIAS_LENGTH, 0);
	unlock_(lock);
	blob_alias[rsize] = 0;
	
	/* ASSERT_COLUMN_MARKED_FOR_WRITE is failing when
	 * I use store()!??
	 * But I want to use it! :(
	 */
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
			case 'R':
				switch (curr_field->field_name[6]) {
					case 't':
						// Repository_id     INT
						ASSERT(strcmp(curr_field->field_name, "Repository_id") == 0);
						curr_field->store(iRepoFile->myRepo->getRepoID(), true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'l':
						// Repo_blob_offset  BIGINT
						ASSERT(strcmp(curr_field->field_name, "Repo_blob_offset") == 0);
						curr_field->store(iRepoOffset, true);
						setNotNullInRecord(curr_field, buf);
						break;
				}
				break;
			case 'B':
				// Blob_alias         
				ASSERT(strcmp(curr_field->field_name, "Blob_alias") == 0);
				curr_field->store(blob_alias, strlen(blob_alias), &UTF8_CHARSET);
				setNotNullInRecord(curr_field, buf);
				
				break;
		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return_(true);
}

#endif

/*
 * -------------------------------------------------------------------------
 * REFERENCE TABLE
 */

MSReferenceTable::MSReferenceTable(MSSystemTableShare *share, TABLE *table):
MSRepositoryTable(share, table),
iRefDataList(NULL), 
iRefDataSize(0), 
iRefDataUsed(0),
iRefDataPos(0),
iRefOpenTable(NULL),
iRefTempLog(NULL)
{
}

MSReferenceTable::~MSReferenceTable()
{
	if (iRefDataList)
		cs_free(iRefDataList);
	if (iRefOpenTable)
		iRefOpenTable->returnToPool();
	if (iRefTempLog)
		iRefTempLog->release();
}

void MSReferenceTable::unuse()
{
	MSRepositoryTable::unuse();
	if (iRefDataList) {
		cs_free(iRefDataList);
		iRefDataList = NULL;
	}
	iRefDataSize = 0;
	if (iRefOpenTable) {
		iRefOpenTable->returnToPool();
		iRefOpenTable = NULL;
	}
	if (iRefTempLog) {
		iRefTempLog->release();
		iRefTempLog = NULL;
	}
}


void MSReferenceTable::seqScanInit()
{
	MSRepositoryTable::seqScanInit();
	iRefDataUsed = 0;
	iRefDataPos = 0;
}

int	MSReferenceTable::getRefLen()
{
	return sizeof(iRefDataUsed) + sizeof(iRefDataPos) + sizeof(iRefCurrentIndex) + sizeof(iRefCurrentOffset);
}

void MSReferenceTable::seqScanPos(unsigned char *pos)
{
	mi_int4store(pos, iRefCurrentDataPos); pos +=4;
	mi_int4store(pos, iRefCurrentDataUsed);pos +=4;	
	mi_int4store(pos, iRefCurrentIndex); pos +=4;
	mi_int8store(pos, iRefCurrentOffset);
}

void MSReferenceTable::seqScanRead(unsigned char *pos, char *buf)
{
	iRefDataPos = mi_uint4korr( pos); pos +=4;
	iRefDataUsed = mi_uint4korr(pos); pos +=4;
	iRefBlobRepo = mi_uint4korr(pos); pos +=4;
	iRefBlobOffset = mi_uint8korr(pos);
	MSRepositoryTable::seqScanRead(iRefBlobRepo, iRefBlobOffset, buf);
}

bool MSReferenceTable::seqScanNext(char *buf)
{
	iRefCurrentDataPos = iRefDataPos;
	iRefCurrentDataUsed = iRefDataUsed;
	
	// Reset the position
	return MSRepositoryTable::seqScanNext(buf);
}
// select * from pbms_reference order by blob_size DESC;
// select * from pbms_reference order by Table_name DESC;

bool MSReferenceTable::resetScan(bool positioned, uint32_t repo_index)
{
	CSMutex				*lock;
	MSBlobHeadRec		blob;
	uint16_t			head_size;
	uint64_t			blob_size;
	MSRepoPointersRec	ptr;
	size_t				ref_size;
	uint32_t			tab_index;
	uint32_t			ref_count;
	uint8_t				status;

	enter_();
	
	if (!MSRepositoryTable::resetScan(positioned, repo_index))
		return_(false);
	
	retry_read:
	lock = iRepoFile->myRepo->getRepoLock(iRepoOffset);
	lock_(lock);
	if (iRepoFile->read(&blob, iRepoOffset, sizeof(MSBlobHeadRec), 0) < sizeof(MSBlobHeadRec)) {
		unlock_(lock);
		iRepoOffset = iRepoFileSize;
		return_(false);
	}
	head_size = CS_GET_DISK_2(blob.rb_head_size_2);
	blob_size = CS_GET_DISK_6(blob.rb_blob_repo_size_6);
	ref_size = CS_GET_DISK_1(blob.rb_ref_size_1);
	ref_count = CS_GET_DISK_2(blob.rb_ref_count_2);
	status = CS_GET_DISK_1(blob.rb_status_1);
	if (ref_count <= 0 || ref_size == 0 ||
		head_size < iRepoFile->myRepo->getRepoBlobHeadSize() + ref_count * ref_size || 
		!VALID_BLOB_STATUS(status)) {
		/* Can't be true. Assume this is garbage! */
		unlock_(lock);
		iRepoOffset++;
		goto retry_read;
	}

	if (IN_USE_BLOB_STATUS(status)) {
		iBlobBuffer->setLength((uint32_t) head_size);
		if (iRepoFile->read(iBlobBuffer->getBuffer(0), iRepoOffset, head_size, 0) < head_size) {
			unlock_(lock);
			iRepoOffset = iRepoFileSize;
			return_(false);
		}
		unlock_(lock);

		iRefAuthCode = CS_GET_DISK_4(blob.rb_auth_code_4);
		iRefBlobSize = CS_GET_DISK_6(blob.rb_blob_data_size_6);;
		iRefBlobRepo = iRepoFile->myRepo->getRepoID();
		iRefBlobOffset = iRepoOffset;

		if (ref_count > iRefDataSize) {
			cs_realloc((void **) &iRefDataList, sizeof(MSRefDataRec) * ref_count);
			iRefDataSize = ref_count;
		}
		
		if (!positioned) 
			iRefDataPos = 0;

		// When ever the data position is reset the current location information
		// must also be reset so that it is consisent with the data position.
		iRefCurrentDataPos = iRefDataPos;
		iRefCurrentOffset = iRepoOffset;
		iRefCurrentIndex = iRepoIndex;
		iRefCurrentDataUsed = iRefDataUsed = ref_count;
		memset(iRefDataList, 0, sizeof(MSRefDataRec) * ref_count);

		uint32_t h = iRepoFile->myRepo->getRepoBlobHeadSize();
		ptr.rp_chars = iBlobBuffer->getBuffer(0) + h;
		for (uint32_t i=0; i<ref_count; i++) {
			switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
				case MS_BLOB_FREE_REF:
					break;
				case MS_BLOB_TABLE_REF: // The blob is intended for a file but has not been inserted yet.
					iRefDataList[i].rd_tab_id = CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4);
					iRefDataList[i].rd_blob_id = CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6);
					iRefDataList[i].rd_col_index = INVALID_INDEX;  // Means not used
					break;
				case MS_BLOB_DELETE_REF:
					tab_index = CS_GET_DISK_2(ptr.rp_temp_ref->tp_del_ref_2);
					if (tab_index && (tab_index <= ref_count)) {
						tab_index--;
						iRefDataList[tab_index].rd_temp_log_id = CS_GET_DISK_4(ptr.rp_temp_ref->tp_log_id_4);
						iRefDataList[tab_index].rd_temp_log_offset = CS_GET_DISK_4(ptr.rp_temp_ref->tp_offset_4);
					}
					else if (tab_index == INVALID_INDEX) {
						/* The is a reference from the temporary log only!! */
						iRefDataList[i].rd_tab_id = 0xFFFFFFFF;  // Indicates no table
						iRefDataList[i].rd_blob_id = iRepoOffset;
						iRefDataList[i].rd_blob_ref_id = CS_GET_DISK_4(ptr.rp_temp_ref->tp_log_id_4);;
						iRefDataList[i].rd_col_index = INVALID_INDEX;  // Means not used
						iRefDataList[i].rd_temp_log_id = CS_GET_DISK_4(ptr.rp_temp_ref->tp_log_id_4);
						iRefDataList[i].rd_temp_log_offset = CS_GET_DISK_4(ptr.rp_temp_ref->tp_offset_4);
					}
					break;
				default:
					MSRepoTableRefPtr	tab_ref;

					tab_index = CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2)-1;
					if (tab_index < ref_count) {
						tab_ref = (MSRepoTableRefPtr) (iBlobBuffer->getBuffer(0) + iRepoFile->myRepo->getRepoBlobHeadSize() + tab_index * ref_size);
	
						iRefDataList[i].rd_tab_id = CS_GET_DISK_4(tab_ref->tr_table_id_4);
						iRefDataList[i].rd_blob_id = CS_GET_DISK_6(tab_ref->tr_blob_id_6);
						iRefDataList[i].rd_col_index = CS_GET_DISK_2(ptr.rp_blob_ref->er_col_index_2);
						iRefDataList[i].rd_blob_ref_id = COMMIT_MASK(CS_GET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8));

						iRefDataList[tab_index].rd_ref_count++;
					}
					else {
						/* Can't be true. Assume this is garbage! */
						unlock_(lock);
						iRepoOffset++;
						goto retry_read;
					}
					break;
			}
			ptr.rp_chars += ref_size;
		}

		iRepoOffset += head_size + blob_size;

		return_(true);
	}
	unlock_(lock);
	iRepoOffset += head_size + blob_size;
	return_(false);
}

bool MSReferenceTable::returnRecord(char *buf)
{
	if (!resetScan(false))
		return false;
		
	return(returnSubRecord(buf));
}

bool MSReferenceTable::returnSubRecord(char *buf)
{
	uint32_t i;
	
	while (iRefDataPos < iRefDataUsed) {
		i = iRefDataPos++;
		if (iRefDataList[i].rd_tab_id) {
			if (iRefDataList[i].rd_col_index == INVALID_INDEX) {
				/* This is a table reference */
				if (!iRefDataList[i].rd_ref_count || iRefDataList[i].rd_temp_log_id) {
					returnRow(&iRefDataList[i], buf);
					return true;
				}
			}
			else {
				/* This is an engine reference */
				returnRow(&iRefDataList[i], buf);
				return true;
			}
		}
	}

	return false;
}

void MSReferenceTable::returnRow(MSRefDataPtr ref_data, char *buf)
{
	TABLE			*table = mySQLTable;
	Field			*curr_field;
	byte			*save;
	MY_BITMAP		*save_write_set;
	MY_BITMAP		*save_read_set;
	bool			have_times = false;
	time_t			delete_time;
	int32_t			countdown = 0;

	if (iRefOpenTable) {
		if (iRefOpenTable->getDBTable()->myTableID != ref_data->rd_tab_id) {
			iRefOpenTable->returnToPool();
			iRefOpenTable = NULL;
		}
	}

	if (!iRefOpenTable && ref_data->rd_tab_id != 0xFFFFFFFF)
		iRefOpenTable = MSTableList::getOpenTableByID(myShare->mySysDatabase->myDatabaseID, ref_data->rd_tab_id);

	if (ref_data->rd_temp_log_id) {
		if (iRefTempLog) {
			if (iRefTempLog->myTempLogID != ref_data->rd_temp_log_id) {
				iRefTempLog->release();
				iRefTempLog = NULL;
			}
		}
		if (!iRefTempLog)
			iRefTempLog = myShare->mySysDatabase->openTempLogFile(ref_data->rd_temp_log_id, NULL, NULL);

		if (iRefTempLog) {
			MSTempLogItemRec	log_item;

			if (iRefTempLog->read(&log_item, ref_data->rd_temp_log_offset, sizeof(MSTempLogItemRec), 0) == sizeof(MSTempLogItemRec)) {
				have_times = true;
				delete_time = CS_GET_DISK_4(log_item.ti_time_4);
				countdown = (int32_t) (delete_time + PBMSParameters::getTempBlobTimeout()) - (int32_t) time(NULL);
			}
		}
	}

	if (ref_data->rd_col_index != INVALID_INDEX) {
		if (iRefOpenTable) {
			if (iRefOpenTable->getDBTable()->isToDelete()) {
				have_times = true;
				iRefOpenTable->getDBTable()->getDeleteInfo(&ref_data->rd_temp_log_id, &ref_data->rd_temp_log_offset, &delete_time);
				ref_data->rd_col_index = INVALID_INDEX;
				countdown = (int32_t) (delete_time + PBMSParameters::getTempBlobTimeout()) - (int32_t) time(NULL);
			}
		}
		else
			ref_data->rd_col_index = INVALID_INDEX;
	}

	save_write_set = table->write_set;
	save_read_set = table->read_set;
	table->write_set = NULL;
	table->read_set = NULL;

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
			case 'B':
				switch (curr_field->field_name[5]) {
					case 'i':
						// Blob_id           BIGINT,
						ASSERT(strcmp(curr_field->field_name, "Blob_id") == 0);
						if (ref_data->rd_tab_id == 0xFFFFFFFF)
							curr_field->store(0, true);
						else {
							curr_field->store(ref_data->rd_blob_id, true);
							setNotNullInRecord(curr_field, buf);
						}
						break;
					case 'u':
						// Blob_url          VARCHAR(120),
						PBMSBlobURLRec blob_url;

						ASSERT(strcmp(curr_field->field_name, "Blob_url") == 0);
						if (ref_data->rd_tab_id != 0xFFFFFFFF) {
							iRefOpenTable->formatBlobURL(&blob_url, ref_data->rd_blob_id, iRefAuthCode, iRefBlobSize, ref_data->rd_blob_ref_id);
							curr_field->store(blob_url.bu_data, strlen(blob_url.bu_data) +1, &UTF8_CHARSET); // Include the null char in the url. This is the way it is stored in user tables.
							setNotNullInRecord(curr_field, buf);
						}
						break;
					case 's':
						// Blob_size         BIGINT,
						ASSERT(strcmp(curr_field->field_name, "Blob_size") == 0);
						curr_field->store(iRefBlobSize, true);
						setNotNullInRecord(curr_field, buf);
						break;
				}
				break;
			case 'C':
				// Column_ordinal       INT,
				ASSERT(strcmp(curr_field->field_name, "Column_ordinal") == 0);
				if (ref_data->rd_col_index != INVALID_INDEX) {
					curr_field->store(ref_data->rd_col_index +1, true);
					setNotNullInRecord(curr_field, buf);
				}
				break;
			case 'D':
				// Deletion_time     TIMESTAMP,
				ASSERT(strcmp(curr_field->field_name, "Deletion_time") == 0);
				if (have_times) {
					curr_field->store(ms_my_1970_to_mysql_time(delete_time), true);
					setNotNullInRecord(curr_field, buf);
				}
				break;
			case 'R':
				switch (curr_field->field_name[5]) {
					case 'i':
						// Repository_id     INT,
						ASSERT(strcmp(curr_field->field_name, "Repository_id") == 0);
						curr_field->store(iRefBlobRepo, true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'b':
						// Repo_blob_offset  BIGINT,
						ASSERT(strcmp(curr_field->field_name, "Repo_blob_offset") == 0);
						curr_field->store(iRefBlobOffset, true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'e':
						// Remove_in INT
						ASSERT(strcmp(curr_field->field_name, "Remove_in") == 0);
						if (have_times) {
							curr_field->store(countdown, false);
							setNotNullInRecord(curr_field, buf);
						}
						break;
				}
				break;
			case 'T':
				switch (curr_field->field_name[9]) {
					case 'e':
						// Table_name        CHAR(64),
						ASSERT(strcmp(curr_field->field_name, "Table_name") == 0);
						if (ref_data->rd_tab_id != 0xFFFFFFFF) {
							if (iRefOpenTable) {
								CSString *table_name = iRefOpenTable->getDBTable()->getTableName();
								curr_field->store(table_name->getCString(), table_name->length(), &UTF8_CHARSET);
							}
							else {
								char buffer[MS_TABLE_NAME_SIZE];
								
								snprintf(buffer, MS_TABLE_NAME_SIZE, "Table #%"PRIu32"", ref_data->rd_tab_id);
								curr_field->store(buffer, strlen(buffer), &UTF8_CHARSET);
							}
							setNotNullInRecord(curr_field, buf);
						}
						break;
					case 'i':
						// Temp_log_id       INT,
						ASSERT(strcmp(curr_field->field_name, "Temp_log_id") == 0);
						if (ref_data->rd_temp_log_id) {
							curr_field->store(ref_data->rd_temp_log_id, true);
							setNotNullInRecord(curr_field, buf);
						}
						break;
					case 'o':
						// Temp_log_offset   BIGINT
						ASSERT(strcmp(curr_field->field_name, "Temp_log_offset") == 0);
						if (ref_data->rd_temp_log_id) {
							curr_field->store(ref_data->rd_temp_log_offset, true);
							setNotNullInRecord(curr_field, buf);
						}
						break;
				}
				break;
		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	table->read_set = save_read_set;
}

/*
 * -------------------------------------------------------------------------
 * META DATA TABLE
 */
MSMetaDataTable::MSMetaDataTable(MSSystemTableShare *share, TABLE *table):
MSRepositoryTable(share, table),
iMetData(NULL), 
iMetCurrentBlobRepo(0),
iMetCurrentBlobOffset(0),
iMetCurrentDataPos(0),
iMetCurrentDataSize(0),
iMetDataPos(0),
iMetDataSize(0), 
iMetBlobRepo(0),
iMetBlobOffset(0),
iMetStateSaved(false)
{
}

MSMetaDataTable::~MSMetaDataTable()
{
	if (iMetData) {
		iMetData->release();
		iMetData = NULL;
	}
}

MSMetaDataTable *MSMetaDataTable::newMSMetaDataTable(MSDatabase *db)
{
	char path[PATH_MAX];
	
	cs_strcpy(PATH_MAX, path, db->myDatabasePath->getCString());
	db->release();
	cs_add_dir_char(PATH_MAX, path);
	cs_strcat(PATH_MAX, path, sysTableNames[SYS_META]);
	
	return  (MSMetaDataTable*) MSSystemTableShare::openSystemTable(path, NULL);
}

void MSMetaDataTable::use()
{
	MSRepositoryTable::use();
	new_(iMetData, CSStringBuffer(80));
	iMetDataSize = 0;
}

void MSMetaDataTable::unuse()
{
	MSRepositoryTable::unuse();
	if (iMetData) {
		iMetData->release();
		iMetData = NULL;
	}
	iMetDataSize = 0;
}


void MSMetaDataTable::seqScanInit()
{
	MSRepositoryTable::seqScanInit();
	iMetDataSize = 0;
	iMetDataPos = 0;
	iMetBlobRepo = 0;
	iMetBlobOffset = 0;
	iMetStateSaved = false;
}

void MSMetaDataTable::seqScanReset()
{
	seqScanPos(iMetState);
	seqScanInit();
	iMetStateSaved = true;
}

int	MSMetaDataTable::getRefLen()
{
	return sizeof(iMetCurrentDataPos) + sizeof(iMetCurrentDataSize) + sizeof(iMetCurrentBlobRepo) + sizeof(iMetCurrentBlobOffset);
}

void MSMetaDataTable::seqScanPos(unsigned char *pos)
{
	mi_int4store(pos, iMetCurrentDataPos); pos +=4;
	mi_int4store(pos, iMetCurrentDataSize);pos +=4;	
	mi_int4store(pos, iMetCurrentBlobRepo); pos +=4;
	mi_int8store(pos, iMetCurrentBlobOffset);
}

void MSMetaDataTable::seqScanRead(unsigned char *pos, char *buf)
{
	iMetStateSaved = false;
	iMetDataPos = mi_uint4korr( pos); pos +=4;
	iMetDataSize = mi_uint4korr(pos); pos +=4;
	iMetBlobRepo = mi_uint4korr(pos); pos +=4;
	iMetBlobOffset = mi_uint8korr(pos);
	MSRepositoryTable::seqScanRead(iMetBlobRepo, iMetBlobOffset, buf);
}

bool MSMetaDataTable::seqScanNext(char *buf)
{
	if (iMetStateSaved) {
		bool have_data;
		uint8_t *pos = iMetState;
		iMetDataPos = mi_uint4korr( pos); pos +=4;
		// Do not reset the meta data size.
		/*iMetDataSize = mi_uint4korr(pos); */pos +=4;
		iMetBlobRepo = mi_uint4korr(pos); pos +=4;
		iMetBlobOffset = mi_uint8korr(pos);
		iMetStateSaved = false;
		resetScan(true, &have_data, iMetBlobRepo);
	}
	
	iMetCurrentDataPos = iMetDataPos;
	iMetCurrentDataSize = iMetDataSize;
	
	return MSRepositoryTable::seqScanNext(buf);
}

bool MSMetaDataTable::resetScan(bool positioned, bool *have_data, uint32_t repo_index)
{
	CSMutex				*lock;
	MSBlobHeadRec		blob;
	uint16_t				head_size;
	uint64_t				blob_size;
	size_t				mdata_size, mdata_offset;
	uint8_t				status;

	enter_();
	
	*have_data = false;
	if (!MSRepositoryTable::resetScan(positioned, repo_index))
		return_(false);
	
	retry_read:
	lock = iRepoFile->myRepo->getRepoLock(iRepoOffset);
	lock_(lock);
	if (iRepoFile->read(&blob, iRepoOffset, sizeof(MSBlobHeadRec), 0) < sizeof(MSBlobHeadRec)) {
		unlock_(lock);
		iRepoOffset = iRepoFileSize;
		return_(false);
	}
	
	head_size = CS_GET_DISK_2(blob.rb_head_size_2);
	blob_size = CS_GET_DISK_6(blob.rb_blob_repo_size_6);
	mdata_size = CS_GET_DISK_2(blob.rb_mdata_size_2);
	mdata_offset = CS_GET_DISK_2(blob.rb_mdata_offset_2);
	
	status = CS_GET_DISK_1(blob.rb_status_1);
	if ((head_size < (mdata_offset + mdata_size)) || !VALID_BLOB_STATUS(status)) {
		/* Can't be true. Assume this is garbage! */
		unlock_(lock);
		iRepoOffset++;
		goto retry_read;
	}

	if (mdata_size && IN_USE_BLOB_STATUS(status)) {
		iMetData->setLength((uint32_t) mdata_size);
		if (iRepoFile->read(iMetData->getBuffer(0), iRepoOffset + mdata_offset, mdata_size, 0) < mdata_size) {
			unlock_(lock);
			iRepoOffset = iRepoFileSize;
			return_(false);
		}

		iMetBlobRepo = iRepoFile->myRepo->getRepoID();
		iMetBlobOffset = iRepoOffset;

		if (!positioned) 
			iMetDataPos = 0;

		iMetDataSize = mdata_size;
		
		// When ever the data position is reset the current location information
		// must also be reset to that it is consisent with the data position.
		iMetCurrentBlobOffset = iRepoOffset;
		iMetCurrentBlobRepo = iRepoIndex;		
		iMetCurrentDataPos = iMetDataPos;
		iMetCurrentDataSize = iMetDataSize;
		
		*have_data = true;
	}
	unlock_(lock);
	iRepoOffset += head_size + blob_size;
	return_(true);
}

bool MSMetaDataTable::returnRecord(char *buf)
{
	bool have_data = false;

	if (resetScan(false, &have_data) && have_data)
		return(returnSubRecord(buf));
		
	return false;
}

bool MSMetaDataTable::nextRecord(char **name, char **value)
{
	if (iMetDataPos < iMetDataSize) {
		char *data = iMetData->getBuffer(iMetDataPos);
		
		*name = data;
		data += strlen(*name) +1;
		*value = data;
		data += strlen(*value) +1;
		
		iMetDataPos += data - *name;
		ASSERT(iMetDataPos <= iMetDataSize);
		
		return true;		
	}

	return false;
	
}

bool MSMetaDataTable::returnSubRecord(char *buf)
{
	char *name, *value;
	
	if (nextRecord(&name, &value)) {
		returnRow(name, value, buf);		
		return true;		
	}

	return false;
}

void MSMetaDataTable::returnRow(char *name, char *value, char *buf)
{
	TABLE		*table = mySQLTable;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;

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
			case 'R':
				switch (curr_field->field_name[6]) {
					case 't':
						// Repository_id     INT
						ASSERT(strcmp(curr_field->field_name, "Repository_id") == 0);
						curr_field->store(iRepoFile->myRepo->getRepoID(), true);
						setNotNullInRecord(curr_field, buf);
						break;
					case 'l':
						// Repo_blob_offset  BIGINT
						ASSERT(strcmp(curr_field->field_name, "Repo_blob_offset") == 0);
						curr_field->store(iMetCurrentBlobOffset, true);
						setNotNullInRecord(curr_field, buf);
						break;
				}
				break;
			case 'N':
				// Name        
				ASSERT(strcmp(curr_field->field_name, "Name") == 0);
				curr_field->store(name, strlen(name), &UTF8_CHARSET);
				setNotNullInRecord(curr_field, buf);
				break;
			case 'V':
				// Value        
				ASSERT(strcmp(curr_field->field_name, "Value") == 0);
				curr_field->store(value, strlen(value), &my_charset_utf8_bin);
				setNotNullInRecord(curr_field, buf);
				break;
		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
}


#ifdef HAVE_ALIAS_SUPPORT
bool MSMetaDataTable::matchAlias(uint32_t repo_id, uint64_t offset, const char *alias)
{
	bool matched = false, have_data = false;
	enter_();
	
	if (resetScan(true, &have_data, repo_id) && have_data) {
		const char *blob_alias;
		MetaData md(iMetData->getBuffer(0), iMetDataSize);
		
		blob_alias = md.findAlias();
		matched = (blob_alias && !my_strcasecmp(&UTF8_CHARSET, blob_alias, alias));
	}
	
	return_(matched);
}
#endif

void MSMetaDataTable::insertRow(char *buf)
{
	uint32_t repo_index;
	String meta_name, meta_value;	
	uint16_t data_len;
	uint64_t repo_offset;
	char *data;		
	bool have_data, reset_alias = false;
	
	enter_();
	
	// Metadata inserts are ignored during reovery.
	// They will be restored from the dump table.
	if (myShare->mySysDatabase->isRecovering())
		exit_();
		
	seqScanReset();

	getFieldValue(buf, 0, &repo_index);
	getFieldValue(buf, 1, &repo_offset);
	getFieldValue(buf, 2, &meta_name);
	getFieldValue(buf, 3, &meta_value);
	
	if (!repo_index)
		CSException::throwException(CS_CONTEXT, HA_ERR_CANNOT_ADD_FOREIGN, "Invalid Repository_id");
		
	iRepoOffset = repo_offset;
	if (!resetScan(true, &have_data, repo_index -1))
		CSException::throwException(CS_CONTEXT, HA_ERR_CANNOT_ADD_FOREIGN, "Invalid Repository_id or Repo_blob_offset");
	
	const char *alias = NULL, *tag = meta_name.c_ptr_safe();
	
	if (iMetDataSize) {
		MetaData md(iMetData->getBuffer(0), iMetDataSize);
		// Check to see it this is a duplicate name.
		if (md.findName(tag))
			CSException::throwException(CS_CONTEXT, HA_ERR_FOUND_DUPP_KEY, "Meta data tag already exists.");
			
#ifdef HAVE_ALIAS_SUPPORT
		alias = md.findAlias();
#endif
	}
	
	// Create the meta data record.
#ifdef HAVE_ALIAS_SUPPORT
	if (alias)
		data = iMetData->getBuffer(0); // This is to be able to test if the alias pointer needs to be reset.
	else
#endif
		data = NULL;
		
	iMetData->setLength(iMetDataSize + meta_name.length() + meta_value.length()  + 2);
	
#ifdef HAVE_ALIAS_SUPPORT
	if (alias && (data != iMetData->getBuffer(0))) // The buffer moved, adjust the alias.
		alias += (iMetData->getBuffer(0) - data);
#endif
	
	data = iMetData->getBuffer(0);
	data_len = iMetDataSize;
	
#ifdef HAVE_ALIAS_SUPPORT
	if ((!alias) && !my_strcasecmp(&UTF8_CHARSET, MS_ALIAS_TAG, tag)) {
		reset_alias = true;
		memcpy(data + data_len, MS_ALIAS_TAG, meta_name->length()); // Use my alias tag so we do not need to wory about case.
		alias = data + data_len + meta_name->length() + 1; // Set the alias to the value location.
	} else 
#endif
		memcpy(data + data_len, meta_name.c_ptr_quick(), meta_name.length());
		
	data_len += meta_name.length();
	data[data_len] = 0;
	data_len++;

	memcpy(data + data_len, meta_value.c_ptr_quick(), meta_value.length());
	data_len += meta_value.length();
	data[data_len] = 0;
	data_len++;
	
	// Update the blob header with the new meta data.
	MSOpenTable	*otab = MSOpenTable::newOpenTable(NULL);
	push_(otab);
	iRepoFile->setBlobMetaData(otab, repo_offset, data, data_len, reset_alias, alias);
	release_(otab);
		
	exit_();
}
/*
insert into pbms_mata_data values(1, 921, "ATAG 3", "xx");
insert into pbms_mata_data values(1, 921, "ATAG 1", "A VALUE 1");
insert into pbms_mata_data values(1, 921, "ATAG 2", "xx");
insert into pbms_mata_data values(1, 383, "ATAG 2", "xx");
select * from pbms_mata_data;
 
delete from pbms_mata_data where value = "xx";
select * from pbms_mata_data;

update pbms_mata_data set value = "!!" where name = "ATAG 3";
update pbms_mata_data set Repo_blob_offset = 383 where value = "!!";

delete from pbms_mata_data where Repo_blob_offset = 921;
*/
//insert into pbms_mata_data values(1, 921, "blob_ALIAs", "My_alias");
//select * from pbms_mata_data;
//select * from pbms_repository;


void MSMetaDataTable::deleteRow(char *buf)
{
	uint32_t repo_index;
	String meta_name, meta_value;	
	uint16_t record_size;
	uint64_t repo_offset;
	char *data;
	bool have_data, reset_alias = false;
	
	enter_();
	
	seqScanReset();

	getFieldValue(buf, 0, &repo_index);
	getFieldValue(buf, 1, &repo_offset);
	getFieldValue(buf, 2, &meta_name);
	getFieldValue(buf, 3, &meta_value);

	if (!repo_index)
		CSException::throwException(CS_CONTEXT, HA_ERR_CANNOT_ADD_FOREIGN, "Invalid Repository_id");
		
	iRepoOffset = repo_offset;
	if (!resetScan(true, &have_data, repo_index -1))
		CSException::throwException(CS_CONTEXT, HA_ERR_CANNOT_ADD_FOREIGN, "Invalid Repository_id or Repo_blob_offset");
	
	const char *alias = NULL, *value = NULL, *tag = meta_name.c_ptr_safe();
	char *location;
	
	// Check to see name exists.
	MetaData md;

	md.use_data(iMetData->getBuffer(0), iMetDataSize);
	if (iMetDataSize) 
		value = md.findName(tag);
	
	if (value == NULL)
		CSException::throwException(CS_CONTEXT, HA_ERR_KEY_NOT_FOUND, "Meta data tag dosn't exists.");
			
#ifdef HAVE_ALIAS_SUPPORT
	alias = md.findAlias();
	
	if (alias == value) {
		reset_alias = true;
		alias = NULL;
	}
#endif
	
	// Create the meta data record.
	data = md.getBuffer();
	location = md.findNamePosition(tag);
	record_size = MetaData::recSize(location);
	iMetDataSize -= record_size;
	memmove(location, location + record_size, iMetDataSize - (location - data)); // Shift the meta data down

#ifdef HAVE_ALIAS_SUPPORT
	// Get the alias again incase it moved.
	if (alias)
		alias = md.findAlias();
#endif
	
	// Update the blob header with the new meta data.
	MSOpenTable	*otab = MSOpenTable::newOpenTable(NULL);
	push_(otab);
	iRepoFile->setBlobMetaData(otab, repo_offset, data, iMetDataSize, reset_alias, alias);
	release_(otab);

	exit_();
}

class UpdateRowCleanUp : public CSRefObject {
	bool do_cleanup;
	MSMetaDataTable *tab;
	char *row_data;
	
	uint32_t ref_id;

	public:
	
	UpdateRowCleanUp(): CSRefObject(),
		do_cleanup(false), tab(NULL), row_data(NULL){}
		
	~UpdateRowCleanUp() 
	{
		if (do_cleanup) {
			tab->deleteRow(row_data);
		}
	}
	
	void setCleanUp(MSMetaDataTable *table, char *data)
	{
		row_data = data;
		tab = table;
		do_cleanup = true;
	}
	
	void cancelCleanUp()
	{
		do_cleanup = false;
	}
	
};

void MSMetaDataTable::updateRow(char *old_data, char *new_data)
{
	uint32_t o_repo_index, n_repo_index;
	String n_meta_name, n_meta_value;	
	String o_meta_name, o_meta_value;	
	uint16_t record_size;
	uint64_t o_repo_offset, n_repo_offset;
	char *data;	
	bool have_data, reset_alias = false;
	
	enter_();
	
	seqScanReset();

	getFieldValue(new_data, 0, &n_repo_index);
	getFieldValue(new_data, 1, &n_repo_offset);
	getFieldValue(new_data, 2, &n_meta_name);
	getFieldValue(new_data, 3, &n_meta_value);

	getFieldValue(old_data, 0, &o_repo_index);
	getFieldValue(old_data, 1, &o_repo_offset);
	getFieldValue(old_data, 2, &o_meta_name);
	getFieldValue(old_data, 3, &o_meta_value);

	if ((!o_repo_index) || (!n_repo_index))
		CSException::throwException(CS_CONTEXT, HA_ERR_CANNOT_ADD_FOREIGN, "Invalid Repository_id");
	
	// If the meta data is not for the same BLOB then we do an insert and delete.
	if ((n_repo_index != o_repo_index) || (n_repo_offset != o_repo_offset)) {
		UpdateRowCleanUp *cleanup;
		new_(cleanup, UpdateRowCleanUp());
		push_(cleanup);

		insertRow(new_data);
		
		cleanup->setCleanUp(this, new_data);
		
		deleteRow(old_data);
		
		cleanup->cancelCleanUp();
		release_(cleanup);
		
		exit_();
	}
	
	iRepoOffset = n_repo_offset;
	if (!resetScan(true, &have_data, n_repo_index -1))
		CSException::throwException(CS_CONTEXT, HA_ERR_CANNOT_ADD_FOREIGN, "Invalid Repository_id or Repo_blob_offset");
	
	char *location;
	const char *value, *alias = NULL, *n_tag = n_meta_name.c_ptr_safe(), *o_tag = o_meta_name.c_ptr_safe();
	
	if (!my_strcasecmp(&UTF8_CHARSET, o_tag, n_tag))
		n_tag = NULL;
		
	MetaData md;

	md.use_data(iMetData->getBuffer(0), iMetDataSize);
	
	if ((!iMetDataSize) || ((value = md.findName(o_tag)) == NULL))
		CSException::throwException(CS_CONTEXT, HA_ERR_KEY_NOT_FOUND, "Meta data tag dosn't exists.");
			
	if (n_tag && (md.findName(n_tag) != NULL))
		CSException::throwException(CS_CONTEXT, HA_ERR_FOUND_DUPP_KEY, "Meta data tag already exists.");
		
#ifdef HAVE_ALIAS_SUPPORT
	alias = md.findAlias();

	if (alias == value) {
		reset_alias = true;
		alias = NULL; // The alias is being deleted.
	}
#endif
	
	if (!n_tag)
		n_tag = o_tag;
		
	// Create the meta data record.
	data = md.getBuffer();
	location = md.findNamePosition(o_tag);
	record_size = MetaData::recSize(location);
	iMetDataSize -= record_size;
	memmove(location, location + record_size, iMetDataSize - (location - data)); // Shift the meta data down
	
	// Add the updated meta data to the end of the block.
	iMetData->setLength(iMetDataSize + n_meta_name.length() + n_meta_value.length()  + 2);
	
	md.use_data(iMetData->getBuffer(0), iMetDataSize); // Reset this incase the buffer moved.
#ifdef HAVE_ALIAS_SUPPORT
	// Get the alias again incase it moved.
	if (alias)
		alias = md.findAlias();
#endif
	
	data = iMetData->getBuffer(0);
		
#ifdef HAVE_ALIAS_SUPPORT
	if ((!alias) && !my_strcasecmp(&UTF8_CHARSET, MS_ALIAS_TAG, n_tag)) {
		reset_alias = true;
		memcpy(data + iMetDataSize, MS_ALIAS_TAG, n_meta_name.length()); // Use my alias tag so we do not need to wory about case.
		alias = data + iMetDataSize + n_meta_name.length() + 1; // Set the alias to the value location.
	} else 
#endif
		memcpy(data + iMetDataSize, n_meta_name.c_ptr_quick(), n_meta_name.length());
		
	iMetDataSize += n_meta_name.length();
	data[iMetDataSize] = 0;
	iMetDataSize++;

	memcpy(data + iMetDataSize, n_meta_value.c_ptr_quick(), n_meta_value.length());
	iMetDataSize += n_meta_value.length();
	data[iMetDataSize] = 0;
	iMetDataSize++;
	
	
	// Update the blob header with the new meta data.
	MSOpenTable	*otab = MSOpenTable::newOpenTable(NULL);
	push_(otab);
	iRepoFile->setBlobMetaData(otab, n_repo_offset, data, iMetDataSize, reset_alias, alias);
	release_(otab);
		
	exit_();
}

/*
 * -------------------------------------------------------------------------
 * SYSTEM TABLE SHARES
 */

CSSyncSortedList *MSSystemTableShare::gSystemTableList;

MSSystemTableShare::MSSystemTableShare():
CSRefObject(),
myTablePath(NULL),
mySysDatabase(NULL),
iOpenCount(0)
{
	thr_lock_init(&myThrLock);
}

MSSystemTableShare::~MSSystemTableShare()
{
#ifdef DRIZZLED
	myThrLock.deinit();
#else
	thr_lock_delete(&myThrLock);
#endif
	if (myTablePath) {
		myTablePath->release();
		myTablePath = NULL;
	}
	if (mySysDatabase) {
		mySysDatabase->release();
		mySysDatabase = NULL;
	}
}

CSObject *MSSystemTableShare::getKey()
{
	return (CSObject *) myTablePath;
}

int MSSystemTableShare::compareKey(CSObject *key)
{
	return myTablePath->compare((CSString *) key);
}

void MSSystemTableShare::startUp()
{
	new_(gSystemTableList, CSSyncSortedList);
}

void MSSystemTableShare::shutDown()
{
	if (gSystemTableList) {
		gSystemTableList->release();
		gSystemTableList = NULL;
	}
}

MSOpenSystemTable *MSSystemTableShare::openSystemTable(const char *table_path, TABLE *table)
{
	CSString			*table_url;
	MSSystemTableShare	*share;
	MSOpenSystemTable	*otab = NULL;
	SysTableType		table_type;

	enter_();
	
	table_type =  pbms_systable_type(cs_last_name_of_path(table_path));
	if (table_type == SYS_UNKNOWN)
		CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_TABLE, "Table not found");
	
	table_path = cs_last_name_of_path(table_path, 2);
	table_url = CSString::newString(table_path);
	push_(table_url);

	lock_(gSystemTableList);
	if (!(share = (MSSystemTableShare *) gSystemTableList->find(table_url))) {
		share = MSSystemTableShare::newTableShare(RETAIN(table_url));
		gSystemTableList->add(share);
	}
	
	switch (table_type) {
		case SYS_REP:
			new_(otab, MSRepositoryTable(share, table));
			break;
		case SYS_REF:
			new_(otab, MSReferenceTable(share, table));
			break;
		case SYS_BLOB:
			new_(otab, MSBlobDataTable(share, table));
			break;
		case SYS_DUMP:
			new_(otab, MSDumpTable(share, table));
			break;
		case SYS_META:
			new_(otab, MSMetaDataTable(share, table));
			break;
		case SYS_HTTP:
			new_(otab, MSHTTPHeaderTable(share, table));
			break;
#ifdef HAVE_ALIAS_SUPPORT
		case SYS_ALIAS:
			new_(otab, MSBlobAliasTable(share, table));
			break;
#endif
		case SYS_VARIABLE:
			new_(otab, MSVariableTable(share, table));
			break;
		case SYS_CLOUD:
			new_(otab, MSCloudTable(share, table));
			break;
		case SYS_BACKUP:
			new_(otab, MSBackupTable(share, table));
			break;
#ifndef DRIZZLED
		case SYS_ENABLED:
			new_(otab, MSEnabledTable(share, table));
			break;
#endif
		case SYS_UNKNOWN:
			break;
	}
	
	share->iOpenCount++;
	unlock_(gSystemTableList);

	release_(table_url);
	return_(otab);
}

void MSSystemTableShare::removeDatabaseSystemTables(MSDatabase *doomed_db)
{
	MSSystemTableShare	*share;
	uint32_t i= 0;
	enter_();
	
	push_(doomed_db);
	lock_(gSystemTableList);
	while ((share = (MSSystemTableShare *) gSystemTableList->itemAt(i))) {
		if (share->mySysDatabase == doomed_db) {
			gSystemTableList->remove(share->myTablePath);
		} else
			i++;
	}
	
	unlock_(gSystemTableList);
	release_(doomed_db);
	exit_();
}

void MSSystemTableShare::releaseSystemTable(MSOpenSystemTable *tab)
{
	enter_();
	lock_(gSystemTableList);
	tab->myShare->iOpenCount--;
	if (!tab->myShare->iOpenCount) {
		gSystemTableList->remove(tab->myShare->myTablePath);
	}
	unlock_(gSystemTableList);
	exit_();
}

MSSystemTableShare *MSSystemTableShare::newTableShare(CSString *table_path)
{
	MSSystemTableShare *tab;

	enter_();
	if (!(tab = new MSSystemTableShare())) {
		table_path->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	push_(tab);
	tab->myTablePath = table_path;
	tab->mySysDatabase = MSDatabase::getDatabase(table_path->left("/", -1), true);
	pop_(tab);
	return_(tab);
}

void PBMSSystemTables::systemTablesStartUp()
{
	MSCloudTable::startUp();
	MSBackupTable::startUp();
}

void PBMSSystemTables::systemTableShutDown()
{
	MSBackupTable::shutDown();
	MSCloudTable::shutDown();
}

