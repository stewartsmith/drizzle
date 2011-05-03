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
 * 2009-05-29
 *
 * H&G2JCtL
 *
 * Repository backup.
 *
 * The backup is done by creating a new database with the same name and ID in the 
 * backup location. Then the pbms_dump table in the source database is initialized
 * for a sequential scan for backup. This has the effect of locking all current repository
 * files. Then the equvalent of  'insert into dst_db.pbms_dump (select * from src_db.pbms_dump);'
 * is performed. 
 *
 */

#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/session.h>
#include <drizzled/table.h>
#include <drizzled/message/table.pb.h>
#include <drizzled/charset.h>
#include <drizzled/table_proto.h>
#include <drizzled/field.h>
#include <drizzled/field/varstring.h>
#endif

#include "cslib/CSConfig.h"

#include <sys/types.h>
#include <inttypes.h>

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSStorage.h"

#include "defs_ms.h"
#include "system_table_ms.h"
#include "open_table_ms.h"
#include "table_ms.h"
#include "database_ms.h"
#include "repository_ms.h"
#include "backup_ms.h"
#include "transaction_ms.h"
#include "systab_variable_ms.h"
#include "systab_backup_ms.h"

uint32_t MSBackupInfo::gMaxInfoRef;
CSSyncSparseArray *MSBackupInfo::gBackupInfo;

//==========================================
MSBackupInfo::MSBackupInfo(	uint32_t id, 
							const char *name, 
							uint32_t db_id_arg, 
							time_t start, 
							time_t end, 
							bool _isDump, 
							const char *location, 
							uint32_t cloudRef_arg, 
							uint32_t cloudBackupNo_arg ):
	backupRefId(id),
	db_name(NULL),
	db_id(db_id_arg),
	startTime(start),
	completionTime(end),
	dump(_isDump),
	isRunning(false),
	backupLocation(NULL),
	cloudRef(cloudRef_arg),
	cloudBackupNo(cloudBackupNo_arg)
{
	db_name = CSString::newString(name);
	if (location && *location)		
		backupLocation = CSString::newString(location);		
}

//-------------------------------
MSBackupInfo::~MSBackupInfo()
{
	if (db_name)
		db_name->release();
	
	if (backupLocation)
		backupLocation->release();
}

//-------------------------------
void MSBackupInfo::startBackup(MSDatabase *pbms_db)
{
	MSDatabase *src_db;
	
	enter_();
	push_(pbms_db);
	
	src_db = MSDatabase::getDatabase(db_id);
	push_(src_db);
	
	startTime = time(NULL);
	
	src_db->startBackup(RETAIN(this));
	release_(src_db);
	
	isRunning = true;
	
	pop_(pbms_db);
	MSBackupTable::saveTable(pbms_db);
	exit_();
}

//-------------------------------
class StartDumpCleanUp : public CSRefObject {
	bool do_cleanup;
	uint32_t ref_id;

	public:
	
	StartDumpCleanUp(): CSRefObject(),
		do_cleanup(false){}
		
	~StartDumpCleanUp() 
	{
		if (do_cleanup) {
			MSBackupInfo::gBackupInfo->remove(ref_id);
		}
	}
	
	void setCleanUp(uint32_t id)
	{
		ref_id = id;
		do_cleanup = true;
	}
	
	void cancelCleanUp()
	{
		do_cleanup = false;
	}
	
};

MSBackupInfo *MSBackupInfo::startDump(MSDatabase *db, uint32_t cloud_ref, uint32_t backup_no)
{
	MSBackupInfo *info;
	uint32_t ref_id;
	StartDumpCleanUp *cleanup;
	
	enter_();
	push_(db);
	lock_(gBackupInfo);
	
	ref_id = gMaxInfoRef++;
	new_(info, MSBackupInfo(ref_id, db->myDatabaseName->getCString(), db->myDatabaseID, time(NULL), 0, true, NULL, cloud_ref, backup_no));
	push_(info);
	
	gBackupInfo->set(ref_id, RETAIN(info));
	
	info->isRunning = true;

	pop_(info);
	unlock_(gBackupInfo);
	push_(info);
	
	// Create a cleanup object to handle cleanup
	// after a possible exception.
	new_(cleanup, StartDumpCleanUp());
	push_(cleanup);
	cleanup->setCleanUp(ref_id);
	
	MSBackupTable::saveTable(RETAIN(db));
	
	cleanup->cancelCleanUp();
	release_(cleanup);
	
	pop_(info);
	release_(db);

	return_(info);
}
//-------------------------------
void MSBackupInfo::backupCompleted(MSDatabase *db)
{
	completionTime = time(NULL);	
	isRunning = false;
	MSBackupTable::saveTable(db);
}

//-------------------------------
void MSBackupInfo::backupTerminated(MSDatabase *db)
{
	enter_();
	push_(db);
	lock_(gBackupInfo);
	
	gBackupInfo->remove(backupRefId);
	unlock_(gBackupInfo);
	
	pop_(db);
	MSBackupTable::saveTable(db);
	exit_();
}

//==========================================
MSBackup::MSBackup():
CSDaemon(NULL),
bu_info(NULL),
bu_BackupList(NULL),
bu_Compactor(NULL),
bu_BackupRunning(false),
bu_State(BU_COMPLETED),
bu_SourceDatabase(NULL),
bu_Database(NULL),
bu_dst_dump(NULL),
bu_src_dump(NULL),
bu_size(0),
bu_completed(0),
bu_ID(0),
bu_start_time(0),
bu_TransactionManagerSuspended(false)
{
}

MSBackup *MSBackup::newMSBackup(MSBackupInfo *info)
{
	MSBackup *bu;
	enter_();
	
	push_(info);
	
	new_(bu, MSBackup());
	push_(bu);
	bu->bu_Database = MSDatabase::getBackupDatabase(RETAIN(info->backupLocation), RETAIN(info->db_name), info->db_id, true);
	pop_(bu);
	
	bu->bu_info = info;
	pop_(info);

	return_(bu);
}

//-------------------------------
class StartBackupCleanUp : public CSRefObject {
	bool do_cleanup;
	MSBackup *backup;

	public:
	
	StartBackupCleanUp(): CSRefObject(),
		do_cleanup(false){}
		
	~StartBackupCleanUp() 
	{
		if (do_cleanup) {
			backup->completeBackup();
		}
	}
	
	void setCleanUp(MSBackup *bup)
	{
		backup = bup;
		do_cleanup = true;
	}
	
	void cancelCleanUp()
	{
		do_cleanup = false;
	}
	
};

void MSBackup::startBackup(MSDatabase *src_db)
{
	CSSyncVector	*repo_list;
	bool			compacting = false;
	MSRepository	*repo;
	StartBackupCleanUp *cleanup;
	enter_();

	// Create a cleanup object to handle cleanup
	// after a possible exception.
	new_(cleanup, StartBackupCleanUp());
	push_(cleanup);
	cleanup->setCleanUp(this);

	bu_SourceDatabase = src_db;
	repo_list = bu_SourceDatabase->getRepositoryList();
	// Suspend the compactor before locking the list.
	bu_Compactor = bu_SourceDatabase->getCompactorThread();
	if (bu_Compactor) {
		bu_Compactor->retain();
		bu_Compactor->suspend();
	}

	// Build the list of repositories to be backed up.
	lock_(repo_list);

	new_(bu_BackupList, CSVector(repo_list->size()));
	for (uint32_t i = 0; i<repo_list->size(); i++) {
		if ((repo = (MSRepository *) repo_list->get(i))) {
			if (!repo->isRemovingFP && !repo->mustBeDeleted) {
				bu_BackupList->add(RETAIN(repo));
				if (repo->initBackup() == REPO_COMPACTING) 
					compacting = true; 
				
				if (!repo->myRepoHeadSize) {
					/* The file has not yet been opened, so the
					 * garbage count will not be known!
					 */
					MSRepoFile *repo_file;

					//repo->retain();
					//unlock_(myRepostoryList);
					//push_(repo);
					repo_file = repo->openRepoFile();
					repo_file->release();
					//release_(repo);
					//lock_(myRepostoryList);
					//goto retry;
				}
				
				bu_size += repo->myRepoFileSize; 

			}
		}
	}
	
	// Copy the table list to the backup database:
	uint32_t		next_tab = 0;
	MSTable		*tab;
	while ((tab = bu_SourceDatabase->getNextTable(&next_tab))) {
		push_(tab);
		bu_Database->addTable(tab->myTableID, tab->myTableName->getCString(), 0, false);
		release_(tab);
	}
	unlock_(repo_list);
	
	// Copy over any physical PBMS system tables.
	PBMSSystemTables::transferSystemTables(RETAIN(bu_Database), RETAIN(bu_SourceDatabase));

	// Load the system tables into the backup database. This will
	// initialize the database for cloud storage if required.
	PBMSSystemTables::loadSystemTables(RETAIN(bu_Database));
	
	// Set the cloud backup info.
	bu_Database->myBlobCloud->cl_setBackupInfo(RETAIN(bu_info));
	
	
	// Set the backup number in the pbms_variable tabe. (This is a hidden value.)
	// This value is used in case a drag and drop restore was done. When a data base is
	// first loaded this value is checked and if it is not zero then the backup record
	// will be read and any used to recover any BLOBs.
	// 
	char value[20];
	snprintf(value, 20, "%"PRIu32"", bu_info->getBackupRefId());
	MSVariableTable::setVariable(RETAIN(bu_Database), BACKUP_NUMBER_VAR, value);
	
	// Once the repositories are locked the compactor can be restarted
	// unless it is in the process of compacting a repository that is
	// being backed up.
	if (bu_Compactor && !compacting) {	
		bu_Compactor->resume();		
		bu_Compactor->release();		
		bu_Compactor = NULL;		
	}
	
	// Suspend the transaction writer while the backup is running.
	MSTransactionManager::suspend(true);
	bu_TransactionManagerSuspended = true;
	
	// Start the backup daemon thread.
	bu_ID = bu_start_time = time(NULL);
	start();
	
	cleanup->cancelCleanUp();
	release_(cleanup);

	exit_();
}

void MSBackup::completeBackup()
{
	if (bu_TransactionManagerSuspended) {	
		MSTransactionManager::resume();
		bu_TransactionManagerSuspended = false;
	}

	if (bu_BackupList) {
		MSRepository *repo;		
		
		while (bu_BackupList->size()) {
			repo = (MSRepository *) bu_BackupList->take(0);
			if (repo) {				
				repo->backupCompleted();
				repo->release();				
			}
		}
		bu_BackupList->release();
		bu_BackupList = NULL;
	}
		
	if (bu_Compactor) {
		bu_Compactor->resume();
		bu_Compactor->release();
		bu_Compactor = NULL;
	}
	
	if (bu_Database) {
		if (bu_State == BU_COMPLETED)
			bu_Database->releaseBackupDatabase();
		else 
			MSDatabase::dropDatabase(bu_Database);
			
		bu_Database = NULL;
	}

	if (bu_SourceDatabase){
		if (bu_State == BU_COMPLETED) 
			bu_info->backupCompleted(bu_SourceDatabase);
		else 
			bu_info->backupTerminated(bu_SourceDatabase);
		
		bu_SourceDatabase = NULL;
		bu_info->release();
		bu_info = NULL;
	}
	
	bu_BackupRunning = false;
}

bool MSBackup::doWork()
{
	enter_();
	try_(a) {
		CSMutex				*my_lock;
		MSRepository		*src_repo, *dst_repo;
		MSRepoFile			*src_file, *dst_file;
		off64_t				src_offset, prev_offset;
		uint16_t				head_size;
		uint64_t				blob_size, blob_data_size;
		CSStringBuffer		*head;
		MSRepoPointersRec	ptr;
		uint32_t				table_ref_count;
		uint32_t				blob_ref_count;
		int					ref_count;
		size_t				ref_size;
		uint32_t				auth_code;
		uint32_t				tab_id;
		uint64_t				blob_id;
		MSOpenTable			*otab;
		uint32_t				src_repo_id;
		uint8_t				status;
		uint8_t				blob_storage_type;
		uint16_t				tab_index;
		uint32_t				mod_time;
		char				*transferBuffer;
		CloudKeyRec			cloud_key;

	
		bu_BackupRunning = true;
		bu_State = BU_RUNNING; 

	/*
		// For testing:
		{
			int blockit = 0;
			myWaitTime = 5 * 1000;  // Time in milli-seconds
			while (blockit)
				return_(true);
		}
	*/
	
		transferBuffer = (char*) cs_malloc(MS_BACKUP_BUFFER_SIZE);
		push_ptr_(transferBuffer);
		
		new_(head, CSStringBuffer(100));
		push_(head);

		src_repo = (MSRepository*)bu_BackupList->get(0);
		while (src_repo && !myMustQuit) {
			src_offset = 0;
			src_file = src_repo->openRepoFile();
			push_(src_file);

			dst_repo = bu_Database->lockRepo(src_repo->myRepoFileSize - src_repo->myGarbageCount);
			frompool_(dst_repo);
			dst_file = dst_repo->openRepoFile();
			push_(dst_file);
			
			src_repo_id = src_repo->myRepoID;
			src_offset = src_repo->myRepoHeadSize;
			prev_offset = 0;
			while (src_offset < src_repo->myRepoFileSize) {	
	retry_read:
					
				bu_completed += src_offset - prev_offset;
				prev_offset = src_offset;
				suspended();

				if (myMustQuit)
					break;
				
				// A lock is required here because references and dereferences to the
				// BLOBs can result in the repository record being updated while 
				// it is being copied.
				my_lock = &src_repo->myRepoLock[src_offset % CS_REPO_REC_LOCK_COUNT];
				lock_(my_lock);
				head->setLength(src_repo->myRepoBlobHeadSize);
				if (src_file->read(head->getBuffer(0), src_offset, src_repo->myRepoBlobHeadSize, 0) < src_repo->myRepoBlobHeadSize) { 
					unlock_(my_lock);
					break;
				}
					
				ptr.rp_chars = head->getBuffer(0);
				ref_size = CS_GET_DISK_1(ptr.rp_head->rb_ref_size_1);
				ref_count = CS_GET_DISK_2(ptr.rp_head->rb_ref_count_2);
				head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
				blob_size = CS_GET_DISK_6(ptr.rp_head->rb_blob_repo_size_6);
				blob_data_size = CS_GET_DISK_6(ptr.rp_head->rb_blob_data_size_6);
				auth_code = CS_GET_DISK_4(ptr.rp_head->rb_auth_code_4);
				status = CS_GET_DISK_1(ptr.rp_head->rb_status_1);
				mod_time = CS_GET_DISK_4(ptr.rp_head->rb_mod_time_4);
				
				blob_storage_type = CS_GET_DISK_1(ptr.rp_head->rb_storage_type_1);
				if (blob_storage_type == MS_CLOUD_STORAGE) {
					MSRepoFile::getBlobKey(ptr.rp_head, &cloud_key);
				}

				// If the BLOB was modified after the start of the backup
				// then set the mod time to the backup time to ensure that
				// a backup for update will work correctly.
				if (mod_time > bu_start_time)
					CS_SET_DISK_4(ptr.rp_head->rb_mod_time_4, bu_start_time);
					
				// If the BLOB was moved during the time of this backup then copy
				// it to the backup location as a referenced BLOB.
				if ((status == MS_BLOB_MOVED)  && (bu_ID == (uint32_t) CS_GET_DISK_4(ptr.rp_head->rb_backup_id_4))) {
					status = MS_BLOB_REFERENCED;
					CS_SET_DISK_1(ptr.rp_head->rb_status_1, status);
				}
				
				// sanity check
				if ((blob_data_size == 0) || ref_count <= 0 || ref_size == 0 ||
					head_size < src_repo->myRepoBlobHeadSize + ref_count * ref_size ||
					!VALID_BLOB_STATUS(status)) {
					/* Can't be true. Assume this is garbage! */
					src_offset++;
					unlock_(my_lock);
					continue;
				}
				
				
				if ((status == MS_BLOB_REFERENCED) || (status == MS_BLOB_MOVED)) {
					head->setLength(head_size);
					if (src_file->read(head->getBuffer(0) + src_repo->myRepoBlobHeadSize, src_offset + src_repo->myRepoBlobHeadSize, head_size  - src_repo->myRepoBlobHeadSize, 0) != (head_size- src_repo->myRepoBlobHeadSize)) {
						unlock_(my_lock);
						break;
					}

					table_ref_count = 0;
					blob_ref_count = 0;
					
					// Loop through all the references removing temporary references 
					// and counting table and blob references.
					
					ptr.rp_chars = head->getBuffer(0) + src_repo->myRepoBlobHeadSize;
					for (int count = 0; count < ref_count; count++) {
						switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
							case MS_BLOB_FREE_REF:
								break;
							case MS_BLOB_TABLE_REF:
								// Unlike the compactor, table refs are not checked because
								// they do not yet exist in the backup database.
								table_ref_count++;
								break;
							case MS_BLOB_DELETE_REF:
								// These are temporary references from the TempLog file. 
								// They are not copied to the backup. 
								CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
								break;
							default:
								// Must be a BLOB reference
								
								tab_index = CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2);
								if (tab_index && (tab_index <= ref_count)) {
									// Only committed references are backed up.
									if (IS_COMMITTED(CS_GET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8))) {
										MSRepoTableRefPtr	tab_ref;
										tab_ref = (MSRepoTableRefPtr) (head->getBuffer(0) + src_repo->myRepoBlobHeadSize + (tab_index-1) * ref_size);
										if (CS_GET_DISK_2(tab_ref->rr_type_2) == MS_BLOB_TABLE_REF)
											blob_ref_count++;
									} else {
										CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
									}
								
								} else {
									/* Can't be true. Assume this is garbage! */
									src_offset++;
									unlock_(my_lock);
									goto retry_read;
								}
								break;
						}
						ptr.rp_chars += ref_size;
					}


					// If there are still blob references then the record needs to be backed up.
					if (table_ref_count && blob_ref_count) {

						off64_t dst_offset;

						dst_offset = dst_repo->myRepoFileSize;
						
						/* Write the header. */
						dst_file->write(head->getBuffer(0), dst_offset, head_size);

						/* Copy the BLOB over: */
						if (blob_storage_type == MS_CLOUD_STORAGE) { 
							bu_Database->myBlobCloud->cl_backupBLOB(&cloud_key);
						} else
							CSFile::transfer(RETAIN(dst_file), dst_offset + head_size, RETAIN(src_file), src_offset + head_size, blob_size, transferBuffer, MS_BACKUP_BUFFER_SIZE);
					
						/* Update the references: */
						ptr.rp_chars = head->getBuffer(0) + src_repo->myRepoBlobHeadSize;
						for (int count = 0; count < ref_count; count++) {
							switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
								case MS_BLOB_FREE_REF:
								case MS_BLOB_DELETE_REF:
									break;
								case MS_BLOB_TABLE_REF:
									tab_id = CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4);
									blob_id = CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6);

									if ((otab = MSTableList::getOpenTableByID(bu_Database->myDatabaseID, tab_id))) {
										frompool_(otab);
										otab->getDBTable()->setBlobHandle(otab, blob_id, dst_repo->myRepoID, dst_offset, blob_size, head_size, auth_code);
//CSException::throwException(CS_CONTEXT, MS_ERR_NOT_IMPLEMENTED, "What if an error ocurred here!");

										backtopool_(otab);
									}
									break;
								default:
									break;
							}
							ptr.rp_chars += ref_size;
						}

						dst_repo->myRepoFileSize += head_size + blob_size;
					}
				}
				unlock_(my_lock);
				src_offset += head_size + blob_size;
			}
			bu_completed += src_offset - prev_offset;
			
			// close the destination repository and cleanup.
			release_(dst_file);
			backtopool_(dst_repo);
			release_(src_file);
			
			// release the source repository and get the next one in the list.
			src_repo->backupCompleted();
			bu_BackupList->remove(0);
			
			src_repo = (MSRepository*)bu_BackupList->get(0);
		}
				
		release_(head);
		release_(transferBuffer);
		if (myMustQuit)
			bu_State = BU_TERMINATED; 
		else
			bu_State = BU_COMPLETED; 
			
	}	
	
	catch_(a) {
		logException();
	}
	
	cont_(a);	
	completeBackup();
	myMustQuit = true;
	return_(true);
}

void *MSBackup::completeWork()
{
	if (bu_SourceDatabase || bu_BackupList || bu_Compactor || bu_info) {
		// We shouldn't be here
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "MSBackup::completeBackup() not called");
		if (bu_SourceDatabase) {
			 bu_SourceDatabase->release();
			 bu_SourceDatabase = NULL;
		}
			
		if (bu_BackupList) {
			 bu_BackupList->release();
			 bu_BackupList = NULL;
		}

			
		if (bu_Compactor) {
			 bu_Compactor->release();
			 bu_Compactor = NULL;
		}

			
		if (bu_info) {
			 bu_info->release();
			 bu_info = NULL;
		}

	}
	return NULL;
}
