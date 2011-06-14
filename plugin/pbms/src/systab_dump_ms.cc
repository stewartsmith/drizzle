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
#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/session.h>
#include <drizzled/sql_lex.h>
#include <drizzled/field/blob.h>
#endif

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


#include "systab_dump_ms.h"


DT_FIELD_INFO pbms_dump_info[]=
{
	{"Data",			NOVAL, NULL, MYSQL_TYPE_LONG_BLOB,	&my_charset_bin,	NOT_NULL_FLAG,	"A BLOB repository record"},
	{NULL,NOVAL, NULL, MYSQL_TYPE_STRING,NULL, 0, NULL}
};

DT_KEY_INFO pbms_dump_keys[]=
{
	{NULL, 0, {NULL}}
};


/*
 * -------------------------------------------------------------------------
 * DUMP TABLE
 */
//-----------------------
MSDumpTable::MSDumpTable(MSSystemTableShare *share, TABLE *table):
	MSRepositoryTable(share, table)
{
}

//-----------------------
MSDumpTable::~MSDumpTable()
{
}

//-----------------------
void MSDumpTable::use()
{	
	dt_hasInfo = dt_hasCompleted = dt_haveCloudInfo = false;
	dt_headerSize = 0;
	
	// Suspend the transaction writer while the dump is running.
	MSTransactionManager::suspend(true);

	MSRepositoryTable::use();
}

//-----------------------
void MSDumpTable::unuse()
{
	MSBackupInfo *backupInfo;
	
	backupInfo = myShare->mySysDatabase->myBlobCloud->cl_getBackupInfo();
	if (backupInfo) {
		enter_();
		push_(backupInfo);
		myShare->mySysDatabase->myBlobCloud->cl_clearBackupInfo();
		if (backupInfo->isBackupRunning()) {
			if (dt_hasCompleted) 
				backupInfo->backupCompleted(RETAIN(myShare->mySysDatabase));
			else
				backupInfo->backupTerminated(RETAIN(myShare->mySysDatabase));
		}
		release_(backupInfo);
		outer_();
	}
	
	MSTransactionManager::resume();
	MSRepositoryTable::unuse();
}

//-----------------------
void MSDumpTable::seqScanInit()
{
	dt_hasInfo = dt_hasCompleted = false;
	return MSRepositoryTable::seqScanInit();
}
//-----------------------
bool MSDumpTable::seqScanNext(char *buf)
{
	if (!dt_hasInfo) {
		dt_hasInfo = true;
		return returnInfoRow(buf);
	}
	// Reset the position
	if (!MSRepositoryTable::seqScanNext(buf)) 
		dt_hasCompleted = true;
	
	return !dt_hasCompleted;
}

//-----------------------
bool MSDumpTable::returnDumpRow(char *record, uint64_t record_size, char *buf)
{
	TABLE		*table = mySQLTable;
	Field		*curr_field;
	byte		*save;
	MY_BITMAP	*save_write_set;




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
			case 'D':
			case 'd':
				// Data         LONGBLOB
				ASSERT(strcmp(curr_field->field_name, "Data") == 0);
				if (record_size <= 0xFFFFFFF) {
					((Field_blob *) curr_field)->set_ptr(record_size, (byte *) record);
					setNotNullInRecord(curr_field, buf);
				}
				break;
		}
		curr_field->ptr = save;
	}

	table->write_set = save_write_set;
	return true;
}

//-----------------------
bool MSDumpTable::returnRow(MSBlobHeadPtr blob, char *buf)
{
	uint64_t		record_size, blob_repo_size;
	uint16_t		ref_size, ref_count, refs = 0, table_refs = 0, header_size;
	uint8_t		blob_storage_type;
	MSRepoPointersRec	ptr;
	MSDatabase *myDB = myShare->mySysDatabase;
	enter_();

	// Reset the references for the BLOB and recreate
	// the temp log references.
	ref_count = CS_GET_DISK_2(blob->rb_ref_count_2);
	ref_size = CS_GET_DISK_1(blob->rb_ref_size_1);

	blob_storage_type = CS_GET_DISK_1(blob->rb_storage_type_1);

	header_size = CS_GET_DISK_2(blob->rb_head_size_2);
	blob_repo_size = CS_GET_DISK_6(blob->rb_blob_repo_size_6);
	
	iBlobBuffer->setLength(header_size);
	iRepoFile->read(iBlobBuffer->getBuffer(0), iRepoOffset, (size_t) header_size, header_size);

	// First check to see if the BLOB is referenced
	ptr.rp_chars = iBlobBuffer->getBuffer(0) + dt_headerSize;
	for (int count = 0; count < ref_count; count++) {
		int ref_type = CS_GET_DISK_2(ptr.rp_ref->rr_type_2);
		
		switch (ref_type) {
			case MS_BLOB_TABLE_REF:
				table_refs++;
				break;
				
			case MS_BLOB_FREE_REF:
			case MS_BLOB_DELETE_REF:
				break;
		
			default: // Assumed to be a MSRepoBlobRefRec.
				// Only committed references are backed up.
				if (IS_COMMITTED(CS_GET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8))) {
					refs++;
				} else {
					CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
				}
				
				break;
		}
		
		ptr.rp_chars += ref_size;		
	}
	
	
	if (refs && table_refs) { // Unreferenced BLOBs are ignored.
		if (blob_storage_type == MS_CLOUD_STORAGE) {
			CloudKeyRec	cloud_key;
			MSRepoFile::getBlobKey(blob, &cloud_key);
			myDB->myBlobCloud->cl_backupBLOB(&cloud_key);
			record_size = header_size;
		} else {
			record_size = header_size + blob_repo_size;
			iBlobBuffer->setLength(record_size);
			iRepoFile->read(iBlobBuffer->getBuffer(header_size), iRepoOffset + header_size, (size_t) blob_repo_size, blob_repo_size);
		}
	} else {
		record_size = 0; // An empty record is returned for unreferenced BLOBs.
	}


	return_(returnDumpRow(iBlobBuffer->getBuffer(0), record_size, buf));
}

//-----------------------
#define INC_INFO_SPACE(i) record_size+=i; space-=i;ptr+=i;
#define MS_DUMP_MAGIC 0x5A74C1EB
typedef struct {
	CSDiskValue4 ti_table_id_4;
	char		 ti_name[1]; // variable length buffer
} TabInfoRec, *TabInfoPtr;

typedef struct {
	CSDiskValue4 di_magic_4;
	CSDiskValue2 di_header_size_2;
}	RepInfoRec, *RepInfoPtr;

// Repository DUMP info record format: 
// <Dump magic><BLOB header size><database ID><backup number><sysTables size><sysTables dump>[<table ID><table name>]...
bool MSDumpTable::returnInfoRow(char *buf)
{
	uint64_t		record_size = 0, space = 1024;
	char		*ptr;
	MSTable		*tab;
	uint32_t		space_needed, next_tab = 0, cloudRef, cloudbackupNo, backupRef;
	RepInfoPtr	rep_info;
	TabInfoPtr	tab_info;
	CSStringBuffer *sysTablesDump;
	MSBackupInfo *backupInfo;
	CSDiskData	d;
	enter_();

	// Setup the sysvar table with the cloud backup number then dump it.
	if (myShare->mySysDatabase->myBlobType == MS_CLOUD_STORAGE) {
		cloudbackupNo = myShare->mySysDatabase->myBlobCloud->cl_getNextBackupNumber();
		cloudRef = myShare->mySysDatabase->myBlobCloud->cl_getDefaultCloudRef();
	} else {
		// It is still possible that the database contains BLOBs in cloud storage
		// even if it isn't currently flaged to use cloud storage.
		cloudbackupNo = cloudRef = 0;
	}

	backupInfo = MSBackupInfo::startDump(RETAIN(myShare->mySysDatabase), cloudRef, cloudbackupNo);
	backupRef = backupInfo->getBackupRefId();
	myShare->mySysDatabase->myBlobCloud->cl_setBackupInfo(backupInfo);	
	
	dt_cloudbackupDBID = myShare->mySysDatabase->myDatabaseID;
	
	sysTablesDump = PBMSSystemTables::dumpSystemTables(RETAIN(myShare->mySysDatabase));
	push_(sysTablesDump);
	
	iBlobBuffer->setLength(space + sysTablesDump->length() + 4 + 4);
	ptr = iBlobBuffer->getBuffer(0);
	rep_info = (RepInfoPtr) iBlobBuffer->getBuffer(0);
	dt_headerSize = sizeof(MSBlobHeadRec);
	
	
	CS_SET_DISK_4(rep_info->di_magic_4, MS_DUMP_MAGIC);
	CS_SET_DISK_2(rep_info->di_header_size_2, dt_headerSize);
	
	INC_INFO_SPACE(sizeof(RepInfoRec));
	
	d.rec_chars = ptr;
	CS_SET_DISK_4(d.int_val->val_4, dt_cloudbackupDBID);
	INC_INFO_SPACE(4);
	
	d.rec_chars = ptr;
	CS_SET_DISK_4(d.int_val->val_4, backupRef);
	INC_INFO_SPACE(4);
	
	// Add the system tables to the dump
	d.rec_chars = ptr;
	CS_SET_DISK_4(d.int_val->val_4, sysTablesDump->length());
	INC_INFO_SPACE(4);
	memcpy(ptr, sysTablesDump->getBuffer(0), sysTablesDump->length());
	INC_INFO_SPACE(sysTablesDump->length());
	release_(sysTablesDump);
	sysTablesDump = NULL;
			
	tab_info = (TabInfoPtr)ptr;
	
	// Get a list of the tables containing BLOB references. 
	while ((tab = myShare->mySysDatabase->getNextTable(&next_tab))) {
		push_(tab);
		space_needed = tab->myTableName->length() + 5;
		if (space < space_needed) {
			space += 1024;
			iBlobBuffer->setLength(space);
			ptr = iBlobBuffer->getBuffer(0) + record_size;
		}
		
		tab_info = (TabInfoPtr)ptr;
		CS_SET_DISK_4(tab_info->ti_table_id_4, tab->myTableID);
		strcpy(tab_info->ti_name, tab->myTableName->getCString());
		INC_INFO_SPACE(space_needed);
		
		release_(tab);
	}
	
	return_(returnDumpRow(iBlobBuffer->getBuffer(0), record_size, buf));
}

#define INC_INFO_REC(i) info_buffer+=i; length-=i; tab_info = (TabInfoPtr) info_buffer;
//-----------------------
void MSDumpTable::setUpRepository(const char *info_buffer, uint32_t length)
{
	uint32_t tab_id, magic;
	MSDatabase *myDB = myShare->mySysDatabase;
	RepInfoPtr	rep_info = (RepInfoPtr) info_buffer;
	TabInfoPtr	tab_info;
	uint32_t		sys_size, backupRefID;
	MSBackupInfo *backupInfo;	
	CSDiskData	d;
	
	if (length < sizeof(RepInfoRec)) {
		CSException::throwException(CS_CONTEXT, CS_ERR_INVALID_RECORD, "Invalid repository info record.");
	}

	magic = CS_GET_DISK_4(rep_info->di_magic_4);
	if (CS_GET_DISK_4(rep_info->di_magic_4) != MS_DUMP_MAGIC) {
		CSException::throwException(CS_CONTEXT, CS_ERR_BAD_HEADER_MAGIC, "Invalid repository info record.");
	}
	
	dt_headerSize = CS_GET_DISK_2(rep_info->di_header_size_2);
	INC_INFO_REC(sizeof(RepInfoRec));
	
	d.rec_cchars = info_buffer;
	dt_cloudbackupDBID = CS_GET_DISK_4(d.int_val->val_4);
	INC_INFO_REC(4);
	
	// Get the backup information
	d.rec_cchars = info_buffer;
	backupRefID = CS_GET_DISK_4(d.int_val->val_4);
	INC_INFO_REC(4);
	
	// If the backup information is missing then the restore may still
	// be able to complete so long as cloud storage was not used.
	backupInfo = MSBackupInfo::findBackupInfo(backupRefID);
	if (backupInfo)  {
		myShare->mySysDatabase->myBlobCloud->cl_setBackupInfo(backupInfo);
		dt_haveCloudInfo = true;
	}
	
	// Restore the System table.
	d.rec_cchars = info_buffer;
	sys_size = CS_GET_DISK_4(d.int_val->val_4);
	INC_INFO_REC(4);
	
	PBMSSystemTables::restoreSystemTables(RETAIN(myDB), info_buffer, sys_size);
	INC_INFO_REC(sys_size);

	while (length > 5) {
		tab_id = CS_GET_DISK_4(tab_info->ti_table_id_4);
		myDB->addTable(tab_id, tab_info->ti_name, 0, false);
		INC_INFO_REC(strlen(tab_info->ti_name) +5);
	}
	
	if (length)
		CSException::throwException(CS_CONTEXT, CS_ERR_INVALID_RECORD, "Invalid repository info record.");		
}


//-----------------------
void MSDumpTable::insertRow(char *buf)
{	
	TABLE	*table = mySQLTable;
	Field_blob *field;
	uint32_t packlength, length;
	const char *blob_rec, *blob_ptr;
	
	field = (Field_blob *)GET_FIELD(table, 0);
	
    /* Get the blob record: */
#ifdef DRIZZLED
    blob_rec= buf + field->offset(table->getInsertRecord());
    packlength= field->pack_length() - table->getShare()->sizeBlobPtr();
#else
    blob_rec= buf + field->offset(table->record[0]);
    packlength= field->pack_length() - table->s->sizeBlobPtr();
#endif

    memcpy(&blob_ptr, blob_rec +packlength, sizeof(char*));
    length= field->get_length();
	
	if (!dt_hasInfo) {
		setUpRepository(blob_ptr, length);
		dt_hasInfo = true;
	} else
		insertRepoRow((MSBlobHeadPtr)blob_ptr, length);
	
}

//-----------------------
void MSDumpTable::insertRepoRow(MSBlobHeadPtr blob, uint32_t length)
{	
	MSRepository *repo;
	MSRepoFile *repo_file;
	uint64_t		repo_offset;
	uint64_t		blob_data_size;
	uint32_t		auth_code;
	uint16_t ref_size, ref_count, refs = 0, table_refs = 0;
	uint8_t		blob_storage_type;
	MSRepoPointersRec	ptr;
	MSDatabase *myDB = myShare->mySysDatabase;
	CloudKeyRec	cloud_key;
	enter_();

	if (!length)
		exit_();
	
	if (length != (CS_GET_DISK_2(blob->rb_head_size_2) + CS_GET_DISK_6(blob->rb_blob_repo_size_6))) {
		CSException::throwException(CS_CONTEXT, MS_ERR_INVALID_RECORD, "Damaged Repository record");
	}
	
	// Get a repository file.
	repo = myDB->lockRepo(length);
	frompool_(repo);
	
	repo_file = myDB->getRepoFileFromPool(repo->myRepoID, false);
	frompool_(repo_file);

	repo_offset = repo->myRepoFileSize;
	
	// Reset the references for the BLOB and recreate
	// the temp log references.
	auth_code = CS_GET_DISK_4(blob->rb_auth_code_4);
	ref_count = CS_GET_DISK_2(blob->rb_ref_count_2);
	ref_size = CS_GET_DISK_1(blob->rb_ref_size_1);
	blob_data_size = CS_GET_DISK_6(blob->rb_blob_data_size_6);

	blob_storage_type = CS_GET_DISK_1(blob->rb_storage_type_1);
	if (blob_storage_type == MS_CLOUD_STORAGE) {
		MSRepoFile::getBlobKey(blob, &cloud_key);
	}

	// First check to see if the BLOB is referenced
	ptr.rp_chars = ((char*) blob) + dt_headerSize;
	for (int count = 0; count < ref_count; count++) {
		int ref_type = CS_GET_DISK_2(ptr.rp_ref->rr_type_2);
		
		switch (ref_type) {
			case MS_BLOB_TABLE_REF:
				table_refs++;
				break;
				
			case MS_BLOB_FREE_REF:
			case MS_BLOB_DELETE_REF:
				break;
		
			default: // Assumed to be a MSRepoBlobRefRec.
				// Only committed references are backed up.
				if (IS_COMMITTED(CS_GET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8))) {
					refs++;
				} else {
					CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
				}
				
				break;
		}
		
		ptr.rp_chars += ref_size;		
	}
	
	
	if (refs && table_refs) { // Unreferenced BLOBs are ignored.
	
	
		// Set table references.
		ptr.rp_chars = ((char*) blob) + dt_headerSize;
		for (int count = 0; count < ref_count; count++) {
			int ref_type = CS_GET_DISK_2(ptr.rp_ref->rr_type_2);
			MSOpenTable	*otab;
			uint32_t		tab_id;
			uint64_t		blob_id;
			
			switch (ref_type) {
				case MS_BLOB_TABLE_REF:
					tab_id = CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4);
					blob_id = CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6);
					otab = MSTableList::getOpenTableByID(myDB->myDatabaseID, tab_id);
			
					frompool_(otab);
					otab->getDBTable()->setBlobHandle(otab, blob_id, repo->myRepoID, repo_offset, blob_data_size, dt_headerSize, auth_code);
					backtopool_(otab);
					break;
					
				case MS_BLOB_DELETE_REF:
					break;
								
				case MS_BLOB_FREE_REF:
				default: 
					break;
			}
				
			ptr.rp_chars += ref_size;		
		}	
	
		// Write the repository record.
		repo_file->write(blob, repo_offset, length);
		repo->myRepoFileSize += length;
		
#ifdef HAVE_ALIAS_SUPPORT
		uint16_t alias_offset;
		if (alias_offset = CS_GET_DISK_2(blob->rb_alias_offset_2)) { 
			myDB->registerBlobAlias(repo->myRepoID, repo_offset, ((char*)blob) + alias_offset);
		}
#endif		
		if (blob_storage_type == MS_CLOUD_STORAGE) {
			if (!dt_haveCloudInfo) {
				CSException::throwException(CS_CONTEXT, MS_ERR_MISSING_CLOUD_REFFERENCE, "Missing cloud backup information.");
			}
			myDB->myBlobCloud->cl_restoreBLOB(&cloud_key, dt_cloudbackupDBID);
		}
	}

	backtopool_(repo_file);
	backtopool_(repo);
	exit_();
}



