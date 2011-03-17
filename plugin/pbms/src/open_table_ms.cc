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
 * 2007-06-04
 *
 * H&G2JCtL
 *
 * Media Stream Tables.
 *
 */
#include "cslib/CSConfig.h"

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSLog.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSPath.h"

#include "open_table_ms.h"
#include "table_ms.h"
#include "connection_handler_ms.h"
#include "engine_ms.h"
#include "transaction_ms.h"
#include "parameters_ms.h"

/*
 * ---------------------------------------------------------------
 * OPEN TABLES
 */

MSOpenTable::MSOpenTable():
CSRefObject(),
CSPooled(),
inUse(true),
isNotATable(false),
nextTable(NULL),
myPool(NULL),
myTableFile(NULL),
myWriteRepo(NULL),
myWriteRepoFile(NULL),
myTempLogFile(NULL),
iNextLink(NULL),
iPrevLink(NULL)
//iUseSize(0),
//iUseCount(0),
//iUsedBlobs(0)
{
	memset(myOTBuffer, 0, MS_OT_BUFFER_SIZE); // wipe this to make valgrind happy.
}

MSOpenTable::~MSOpenTable()
{
	close();
}

void MSOpenTable::close()
{
	enter_();
	if (myTableFile) {
		myTableFile->release();
		myTableFile = NULL;
	}
	closeForWriting();
	if (myTempLogFile) {
		myTempLogFile->release();
		myTempLogFile = NULL;
	}
/*
	if (iUsedBlobs) {
		cs_free(iUsedBlobs);
		iUsedBlobs = NULL;
	}
	iUseCount = 0;
	iUseSize = 0;
*/
	exit_();
}

void MSOpenTable::returnToPool()
{
	MSTableList::releaseTable(this);
}

// This cleanup class is used to reset the 
// repository size if something goes wrong.
class CreateBlobCleanUp : public CSRefObject {
	bool do_cleanup;
	uint64_t old_size;
	MSOpenTable *ot;
	MSRepository *repo;

	public:
	
	CreateBlobCleanUp(): CSRefObject(),
		do_cleanup(false){}
		
	~CreateBlobCleanUp() 
	{
		if (do_cleanup) {
			repo->setRepoFileSize(ot, old_size);

		}
	}
	
	void setCleanUp(MSOpenTable *ot_arg, MSRepository *repo_arg, uint64_t size)
	{
		old_size = size;
		repo = repo_arg;
		ot = ot_arg;
		do_cleanup = true;
	}
	
	void cancelCleanUp()
	{
		do_cleanup = false;
	}
	
};

void MSOpenTable::createBlob(PBMSBlobURLPtr bh, uint64_t blob_size, char *metadata, uint16_t metadata_size, CSInputStream *stream, CloudKeyPtr cloud_key, Md5Digest *checksum)
{
	uint64_t repo_offset;
	uint64_t blob_id = 0;
	uint32_t	auth_code;
	uint16_t head_size;
	uint32_t	log_id;
	uint32_t log_offset;
	uint32_t temp_time;
	uint64_t repo_size;
	uint64_t repo_id;
	Md5Digest my_checksum;
	CloudKeyRec cloud_key_rec;
	CreateBlobCleanUp *cleanup;
	enter_();
	
	new_(cleanup, CreateBlobCleanUp());
	push_(cleanup);
	
	if (!checksum)
		checksum = &my_checksum;
		
	if (stream) push_(stream);
	openForWriting();
	ASSERT(myWriteRepo);
	auth_code = random();
	repo_size = myWriteRepo->getRepoFileSize();
	temp_time =	myWriteRepo->myLastTempTime;

	// If an exception occurs the cleanup operation will be called.
	cleanup->setCleanUp(this, myWriteRepo, repo_size);

	head_size = myWriteRepo->getDefaultHeaderSize(metadata_size);
	if (getDB()->myBlobType == MS_STANDARD_STORAGE) {
		pop_(stream);
		repo_offset = myWriteRepo->receiveBlob(this, head_size, blob_size, checksum, stream);
	} else {
		ASSERT(getDB()->myBlobType == MS_CLOUD_STORAGE);
		CloudDB *cloud = getDB()->myBlobCloud;
		
		if (!cloud)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Creating cloud BLOB without cloud.");
	
		repo_offset = repo_size + head_size;
		memset(checksum, 0, sizeof(Md5Digest)); // The checksum is only for local storage.
		
		// If there is a stream then the data has not been sent to the cloud yet.
		if (stream) { 
			cloud_key = &cloud_key_rec;
			cloud->cl_getNewKey(cloud_key);
			pop_(stream);
			cloud->cl_putData(cloud_key, stream, blob_size);
		}
		
	}
	
	repo_id = myWriteRepo->myRepoID;
	if (isNotATable) {	
		getDB()->queueForDeletion(this, MS_TL_REPO_REF, repo_id, repo_offset, auth_code, &log_id, &log_offset, &temp_time);
		formatRepoURL(bh, repo_id, repo_offset, auth_code, blob_size);
	}
	else {
		blob_id = getDBTable()->createBlobHandle(this, myWriteRepo->myRepoID, repo_offset, blob_size, head_size, auth_code);
		getDB()->queueForDeletion(this, MS_TL_BLOB_REF, getDBTable()->myTableID, blob_id, auth_code, &log_id, &log_offset, &temp_time);
		formatBlobURL(bh, blob_id, auth_code, blob_size, 0);
	}
	
	myWriteRepo->writeBlobHead(this, repo_offset, myWriteRepo->myRepoDefRefSize, head_size, blob_size, checksum, metadata, metadata_size, blob_id, auth_code, log_id, log_offset, getDB()->myBlobType, cloud_key);
	
	cleanup->cancelCleanUp();
	release_(cleanup);
	
	exit_();
}

// BLOBs created with this method are always created as standard local BLOBs. (No cloud storage)
void MSOpenTable::createBlob(PBMSBlobIDPtr blob_id, uint64_t blob_size, char *metadata, uint16_t metadata_size)
{
	uint64_t repo_size;
	uint64_t repo_offset;
	uint64_t repo_id;
	uint32_t	auth_code;
	uint16_t head_size;
	uint32_t	log_id;
	uint32_t log_offset;
	uint32_t temp_time;
	CreateBlobCleanUp *cleanup;
	enter_();
	
	new_(cleanup, CreateBlobCleanUp());
	push_(cleanup);

	openForWriting();
	ASSERT(myWriteRepo);
	auth_code = random();
	
	repo_size = myWriteRepo->getRepoFileSize();
	
	// If an exception occurs the cleanup operation will be called.
	cleanup->setCleanUp(this, myWriteRepo, repo_size);

	head_size = myWriteRepo->getDefaultHeaderSize(metadata_size);

	repo_offset = myWriteRepo->receiveBlob(this, head_size, blob_size);
	repo_id = myWriteRepo->myRepoID;
	temp_time = myWriteRepo->myLastTempTime;
	getDB()->queueForDeletion(this, MS_TL_REPO_REF, repo_id, repo_offset, auth_code, &log_id, &log_offset, &temp_time);
	myWriteRepo->myLastTempTime = temp_time;
	myWriteRepo->writeBlobHead(this, repo_offset, myWriteRepo->myRepoDefRefSize, head_size, blob_size, NULL, metadata, metadata_size, 0, auth_code, log_id, log_offset, MS_STANDARD_STORAGE, NULL);
	// myWriteRepo->setRepoFileSize(this, repo_offset + head_size + blob_size);This is now set by writeBlobHead()
	
	blob_id->bi_db_id = getDB()->myDatabaseID;
	blob_id->bi_blob_id = repo_offset;
	blob_id->bi_tab_id = repo_id;
	blob_id->bi_auth_code = auth_code;
	blob_id->bi_blob_size = blob_size;
	blob_id->bi_blob_type = MS_URL_TYPE_REPO;
	blob_id->bi_blob_ref_id = 0;
	
	cleanup->cancelCleanUp();
	release_(cleanup);

	exit_();
}

void MSOpenTable::sendRepoBlob(uint64_t blob_id, uint64_t req_offset, uint64_t req_size, uint32_t auth_code, bool info_only, CSHTTPOutputStream *stream)
{
	uint32_t		repo_id;
	uint64_t		offset;
	uint64_t		size;
	uint16_t		head_size;
	MSRepoFile	*repo_file;

	enter_();
	openForReading();
	getDBTable()->readBlobHandle(this, blob_id, &auth_code, &repo_id, &offset, &size, &head_size, true);
	repo_file = getDB()->getRepoFileFromPool(repo_id, false);
	frompool_(repo_file);
	//repo_file->sendBlob(this, offset, head_size, size, stream);
	repo_file->sendBlob(this, offset, req_offset, req_size, 0, false, info_only, stream);
	backtopool_(repo_file);
	exit_();
}

void MSOpenTable::freeReference(uint64_t blob_id, uint64_t blob_ref_id)
{
	uint32_t		repo_id;
	uint64_t		offset;
	uint64_t		blob_size;
	uint16_t		head_size;
	MSRepoFile	*repo_file;
	uint32_t		auth_code = 0;

	enter_();
	openForReading();

	getDBTable()->readBlobHandle(this, blob_id, &auth_code, &repo_id, &offset, &blob_size, &head_size, true);
	repo_file = getDB()->getRepoFileFromPool(repo_id, false);

	frompool_(repo_file);
	repo_file->releaseBlob(this, offset, head_size, getDBTable()->myTableID, blob_id, blob_ref_id, auth_code);
	backtopool_(repo_file);

	exit_();
}

void MSOpenTable::commitReference(uint64_t blob_id, uint64_t blob_ref_id)
{
	uint32_t		repo_id;
	uint64_t		offset;
	uint64_t		blob_size;
	uint16_t		head_size;
	MSRepoFile	*repo_file;
	uint32_t		auth_code = 0;

	enter_();
	openForReading();
	
	getDBTable()->readBlobHandle(this, blob_id, &auth_code, &repo_id, &offset, &blob_size, &head_size, true);
	repo_file = getDB()->getRepoFileFromPool(repo_id, false);

	frompool_(repo_file);
	repo_file->commitBlob(this, offset, head_size, getDBTable()->myTableID, blob_id, blob_ref_id, auth_code);
	backtopool_(repo_file);

	exit_();
}

void MSOpenTable::useBlob(int type, uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code, uint16_t col_index, uint64_t blob_size, uint64_t blob_ref_id, PBMSBlobURLPtr ret_blob_url)
{
	MSRepoFile		*repo_file= NULL;
	MSBlobHeadRec	blob;
	CSInputStream	*stream;
	MSDatabase		*blob_db;
	int				state;
	uint16_t			head_size;
	uint64_t			repo_offset;
	uint32_t			repo_id;

	enter_();

	blob_db = getDB();
		
	if (!blob_db->isRecovering()) {
		// During recovery the only thing that needs to be done is to 
		// reset the database ID which is done when the URL is created.
		// Create the URL using the table ID passed in not the one from 
		// the table associated with this object.

		openForReading();
		if (type == MS_URL_TYPE_REPO) { // There is no table reference associated with this BLOB yet.
			uint32_t		ac;
			uint8_t		status;
			bool		same_db = true;

			if (blob_db->myDatabaseID == db_id)
				repo_file = blob_db->getRepoFileFromPool(tab_id, false);
			else {
				same_db = false;
				blob_db = MSDatabase::getDatabase(db_id);
				push_(blob_db);
				repo_file = blob_db->getRepoFileFromPool(tab_id, false);
				release_(blob_db);
				blob_db = repo_file->myRepo->myRepoDatabase;
			}
		
			frompool_(repo_file);
			repo_file->read(&blob, blob_id, MS_MIN_BLOB_HEAD_SIZE, MS_MIN_BLOB_HEAD_SIZE);

			repo_offset = blob_id;
			blob_size  = CS_GET_DISK_6(blob.rb_blob_data_size_6);
			head_size = CS_GET_DISK_2(blob.rb_head_size_2);
			 
			ac = CS_GET_DISK_4(blob.rb_auth_code_4);
			if (auth_code != ac)
				CSException::throwException(CS_CONTEXT, MS_ERR_AUTH_FAILED, "Invalid BLOB identifier");
			status = CS_GET_DISK_1(blob.rb_status_1);
			if ( ! IN_USE_BLOB_STATUS(status))
				CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB has already been deleted");

			if (same_db) {
				// Create a table reference to the BLOB:
				repo_id = tab_id;
				blob_id = getDBTable()->createBlobHandle(this, tab_id, blob_id, blob_size, head_size, auth_code);
				state = MS_UB_NEW_HANDLE;
			}
			else {
				
				getDB()->openWriteRepo(this);

				// If either databases are using cloud storage then this is
				// not supported yet.			
				if (getDB()->myBlobCloud || myWriteRepo->myRepoDatabase->myBlobCloud)
					CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Copying cloud BLOB between databases is not supported.");
			
				stream = repo_file->getInputStream(repo_offset);
				push_(stream);
				repo_offset = myWriteRepo->copyBlob(this, head_size + blob_size, stream);			
				release_(stream);

				// Create a table reference to the BLOB:
				repo_id = myWriteRepo->myRepoID;
				blob_id = getDBTable()->createBlobHandle(this, myWriteRepo->myRepoID, repo_offset, blob_size, head_size, auth_code);
				state = MS_UB_NEW_BLOB;
			}
			backtopool_(repo_file);
		}
		else {

			if (blob_db->myDatabaseID == db_id && getDBTable()->myTableID == tab_id) {
				getDBTable()->readBlobHandle(this, blob_id, &auth_code, &repo_id, &repo_offset, &blob_size, &head_size, true);
				
				state = MS_UB_SAME_TAB;
			}
			else {
				MSOpenTable *blob_otab;

				blob_otab = MSTableList::getOpenTableByID(db_id, tab_id);
				frompool_(blob_otab);
				blob_otab->getDBTable()->readBlobHandle(blob_otab, blob_id, &auth_code, &repo_id, &repo_offset, &blob_size, &head_size, true);
				if (blob_db->myDatabaseID == db_id) {
					blob_id = getDBTable()->findBlobHandle(this, repo_id, repo_offset, blob_size, head_size, auth_code);
					if (blob_id == 0)
						blob_id = getDBTable()->createBlobHandle(this, repo_id, repo_offset, blob_size, head_size, auth_code);
					state = MS_UB_NEW_HANDLE;
				}
				else {

					// If either databases are using cloud storage then this is
					// not supported yet.			
					if (blob_db->myBlobCloud || myWriteRepo->myRepoDatabase->myBlobCloud)
						CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Copying cloud BLOB between databases is not supported.");

					// NOTE: For each BLOB reference copied from one database to another a new
					// BLOB will be created. This can result in multiple copies fo the same BLOB
					// in the destination database. One way around this would be to redisign things
					// so that there is one BLOB repository shared across all databases. 
					blob_db->openWriteRepo(this);
									
					stream = repo_file->getInputStream(repo_offset);
					push_(stream);
					
					repo_offset = myWriteRepo->copyBlob(this, head_size + blob_size, stream);
					
					release_(stream);

					repo_id = myWriteRepo->myRepoID;
					blob_id = getDBTable()->createBlobHandle(this, myWriteRepo->myRepoID, repo_offset, blob_size, head_size, auth_code);
					state = MS_UB_NEW_BLOB;
				}
				backtopool_(blob_otab);
			}
			
		}
		
		blob_ref_id = blob_db->newBlobRefId();
		
		// Always use the table ID of this table because regardless of
		// where the BLOB ref came from it is being inserted into this table.
		tab_id = getDBTable()->myTableID; 
		
		// Add the BLOB reference to the repository.
		repo_file = blob_db->getRepoFileFromPool(repo_id, false);		
		frompool_(repo_file);
		repo_file->referenceBlob(this, repo_offset, head_size, tab_id, blob_id, blob_ref_id, auth_code, col_index);		
		backtopool_(repo_file);
		
		MSTransactionManager::referenceBLOB(getDB()->myDatabaseID, tab_id, blob_id, blob_ref_id);

	} 
	
	formatBlobURL(ret_blob_url, blob_id, auth_code, blob_size, tab_id, blob_ref_id);
		
	exit_();
}

void MSOpenTable::releaseReference(uint64_t blob_id, uint64_t blob_ref_id)
{
	enter_();
	
	MSTransactionManager::dereferenceBLOB(getDB()->myDatabaseID, getDBTable()->myTableID, blob_id, blob_ref_id);

	exit_();
}

void MSOpenTable::checkBlob(CSStringBuffer *buffer, uint64_t blob_id, uint32_t auth_code, uint32_t temp_log_id, uint32_t temp_log_offset)
{
	uint32_t		repo_id;
	uint64_t		offset;
	uint64_t		size;
	uint16_t		head_size;
	MSRepoFile	*repo_file;

	enter_();
	openForReading();
	if (getDBTable()->readBlobHandle(this, blob_id, &auth_code, &repo_id, &offset, &size, &head_size, false)) {
		if ((repo_file = getDB()->getRepoFileFromPool(repo_id, true))) {
			frompool_(repo_file);
			repo_file->checkBlob(buffer, offset, auth_code, temp_log_id, temp_log_offset);
			backtopool_(repo_file);
		}
		else
			getDBTable()->freeBlobHandle(this, blob_id, repo_id, offset, auth_code);
	}
	exit_();
}

bool MSOpenTable::deleteReferences(uint32_t temp_log_id, uint32_t temp_log_offset, bool *must_quit)
{
	MSTableHeadRec		tab_head;
	off64_t				blob_id;
	MSTableBlobRec		tab_blob;
	uint32_t				repo_id;
	uint64_t				repo_offset;
	uint16_t				head_size;
	uint32_t				auth_code;
	MSRepoFile			*repo_file = NULL;
	bool				result = true;

	enter_();
	openForReading();
	if (myTableFile->read(&tab_head, 0, offsetof(MSTableHeadRec, th_reserved_4), 0) < offsetof(MSTableHeadRec, th_reserved_4))
		/* Nothing to read, delete it ... */
		goto exit;
	if (CS_GET_DISK_4(tab_head.th_temp_log_id_4) != temp_log_id ||
		CS_GET_DISK_4(tab_head.th_temp_log_offset_4) != temp_log_offset) {
		/* Wrong delete reference (ignore): */
		result = false;
		goto exit;
	}

	blob_id = CS_GET_DISK_2(tab_head.th_head_size_2);
	while (blob_id + sizeof(MSTableBlobRec) <= getDBTable()->getTableFileSize()) {
		if (*must_quit) {
			/* Bit of a waste of work, but we must quit! */
			result = false;
			break;
		}
		if (myTableFile->read(&tab_blob, blob_id, sizeof(MSTableBlobRec), 0) < sizeof(MSTableBlobRec))
			break;
		repo_id = CS_GET_DISK_3(tab_blob.tb_repo_id_3);
		repo_offset = CS_GET_DISK_6(tab_blob.tb_offset_6);
		head_size = CS_GET_DISK_2(tab_blob.tb_header_size_2);
		auth_code = CS_GET_DISK_4(tab_blob.tb_auth_code_4);
		if (repo_file && repo_file->myRepo->myRepoID != repo_id) {
			backtopool_(repo_file);
			repo_file = NULL;
		}
		if (!repo_file) {
			repo_file = getDB()->getRepoFileFromPool(repo_id, true);
			if (repo_file)
				frompool_(repo_file);
		}
		if (repo_file) 
			repo_file->freeTableReference(this, repo_offset, head_size, getDBTable()->myTableID, blob_id, auth_code);
		
		blob_id += sizeof(MSTableBlobRec);
	}
	
	if (repo_file)
		backtopool_(repo_file);

	exit:
	return_(result);
}

void MSOpenTable::openForReading()
{
	if (!myTableFile && !isNotATable)
		myTableFile = getDBTable()->openTableFile();
}

void MSOpenTable::openForWriting()
{
	if (myTableFile && myWriteRepo && myWriteRepoFile)
		return;
	enter_();
	openForReading();
	if (!myWriteRepo || !myWriteRepoFile)
		getDB()->openWriteRepo(this);
	exit_();
}

void MSOpenTable::closeForWriting()
{
	if (myWriteRepoFile) {		
		myWriteRepoFile->myRepo->syncHead(myWriteRepoFile);
		myWriteRepoFile->release();
		myWriteRepoFile = NULL;
	}
	if (myWriteRepo) {
		myWriteRepo->unlockRepo(REPO_WRITE);
#ifndef MS_COMPACTOR_POLLS
		if (myWriteRepo->getGarbageLevel() >= PBMSParameters::getGarbageThreshold()) {
			if (myWriteRepo->myRepoDatabase->myCompactorThread)
				myWriteRepo->myRepoDatabase->myCompactorThread->wakeup();
		}
#endif
		myWriteRepo->release();
		myWriteRepo = NULL;
	}
}

uint32_t MSOpenTable::getTableID()
{
	return myPool->myPoolTable->myTableID;
}

MSTable *MSOpenTable::getDBTable()
{
	return myPool->myPoolTable;
}

MSDatabase *MSOpenTable::getDB()
{
	return myPool->myPoolDB;
}

void MSOpenTable::formatBlobURL(PBMSBlobURLPtr blob_url, uint64_t blob_id, uint32_t auth_code, uint64_t blob_size, uint32_t tab_id, uint64_t blob_ref_id)
{
	MSBlobURLRec blob;
	
	blob.bu_type = MS_URL_TYPE_BLOB;
	blob.bu_db_id = getDB()->myDatabaseID;
	blob.bu_tab_id = tab_id;
	blob.bu_blob_id = blob_id;
	blob.bu_auth_code = auth_code;
	blob.bu_server_id = PBMSParameters::getServerID();
	blob.bu_blob_size = blob_size;
	blob.bu_blob_ref_id = blob_ref_id;
	
	PBMSBlobURLTools::buildBlobURL(&blob, blob_url);
	
}
void MSOpenTable::formatBlobURL(PBMSBlobURLPtr blob_url, uint64_t blob_id, uint32_t auth_code, uint64_t blob_size, uint64_t blob_ref_id)
{
	formatBlobURL(blob_url, blob_id, auth_code, blob_size, getDBTable()->myTableID, blob_ref_id);
}
void MSOpenTable::formatRepoURL(PBMSBlobURLPtr blob_url, uint32_t log_id, uint64_t log_offset, uint32_t auth_code, uint64_t blob_size)
{
	MSBlobURLRec blob;
	
	blob.bu_type = MS_URL_TYPE_REPO;
	blob.bu_db_id = getDB()->myDatabaseID;
	blob.bu_tab_id = log_id;
	blob.bu_blob_id = log_offset;
	blob.bu_auth_code = auth_code;
	blob.bu_server_id = PBMSParameters::getServerID();
	blob.bu_blob_size = blob_size;
	blob.bu_blob_ref_id = 0;
	
	PBMSBlobURLTools::buildBlobURL(&blob, blob_url);
}

MSOpenTable *MSOpenTable::newOpenTable(MSOpenTablePool *pool)
{
	MSOpenTable *otab;
	
	if (!(otab = new MSOpenTable()))
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	if ((otab->myPool = pool))
		otab->isNotATable = pool->myPoolTable == NULL;
	else
		otab->isNotATable = false;
		
	return otab;
}

/*
 * ---------------------------------------------------------------
 * OPEN TABLE POOLS
 */

MSOpenTablePool::MSOpenTablePool():
myPoolTableID(0),
isRemovingTP(false),
myPoolTable(NULL),
myPoolDB(NULL),
iTablePool(NULL)
{
}

MSOpenTablePool::~MSOpenTablePool()
{
	isRemovingTP = true;
	removeOpenTablesNotInUse();
	/* With this, I also delete those that are in use!: */
	iPoolTables.clear();
	if (myPoolTable)
		myPoolTable->release();
	if (myPoolDB)
		myPoolDB->release();
}

#ifdef DEBUG
void MSOpenTablePool::check()
{
	MSOpenTable	*otab, *ptab;
	bool		found;

	if ((otab = (MSOpenTable *) iPoolTables.getBack())) {
		do {
			found = false;
			ptab = iTablePool;
			while (ptab) {
				if (ptab == otab) {
					ASSERT(!found);
					found = true;
				}
				ptab = ptab->nextTable;
			}
			if (otab->inUse) {
				ASSERT(!found);
			}
			else {
				ASSERT(found);
			}
			otab = (MSOpenTable *) otab->getNextLink();
		} while (otab);
	}
	else
		ASSERT(!iTablePool);
}
#endif

/*
 * This returns the table referenced. So it is safe from the pool being
 * destroyed.
 */
MSOpenTable *MSOpenTablePool::getPoolTable()
{
	MSOpenTable *otab;

	if ((otab = iTablePool)) {
		iTablePool = otab->nextTable;
		otab->nextTable = NULL;
		ASSERT(!otab->inUse);
		otab->inUse = true;
		otab->retain();
	}
	return otab;
}

void MSOpenTablePool::returnOpenTable(MSOpenTable *otab)
{
	otab->inUse = false;
	otab->nextTable = iTablePool;
	iTablePool = otab;
}

/*
 * Add a table to the pool, but do not release it!
 */
void MSOpenTablePool::addOpenTable(MSOpenTable *otab)
{
	iPoolTables.addFront(otab);
}

void MSOpenTablePool::removeOpenTable(MSOpenTable *otab)
{
	otab->close();
	iPoolTables.remove(otab);
}

void MSOpenTablePool::removeOpenTablesNotInUse()
{
	MSOpenTable *otab, *curr_otab;

	iTablePool = NULL;
	/* Remove all tables that are not in use: */
	if ((otab = (MSOpenTable *) iPoolTables.getBack())) {
		do {
			curr_otab = otab;
			otab = (MSOpenTable *) otab->getNextLink();
			if (!curr_otab->inUse)
				iPoolTables.remove(curr_otab);
		} while (otab);
	}
}

void MSOpenTablePool::returnToPool()
{
	MSTableList::removeTablePool(this);
}

MSOpenTablePool *MSOpenTablePool::newPool(uint32_t db_id, uint32_t tab_id)
{
	MSOpenTablePool *pool;
	enter_();
	
	if (!(pool = new MSOpenTablePool())) {
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	push_(pool);
	pool->myPoolDB = MSDatabase::getDatabase(db_id);
	pool->myPoolTableID = tab_id;
	if (tab_id)
		pool->myPoolTable = pool->myPoolDB->getTable(tab_id, false);
	pop_(pool);
	return_(pool);
}

/*
 * ---------------------------------------------------------------
 * TABLE LIST
 */

CSSyncOrderedList		*MSTableList::gPoolListByID;

MSTableList::MSTableList()
{	
}

MSTableList::~MSTableList()
{
}

void MSTableList::startUp()
{
	new_(gPoolListByID, CSSyncOrderedList);
}

void MSTableList::shutDown()
{
	if (gPoolListByID) {
		gPoolListByID->clear();
		gPoolListByID->release();
		gPoolListByID = NULL;
	}
}

class MSTableKey : public CSOrderKey {
public:
	uint32_t	myKeyDatabaseID;
	uint32_t	myKeyTableID;

	MSTableKey(): myKeyDatabaseID(0), myKeyTableID(0){ }

	virtual ~MSTableKey() {
	}

	int compareKey(CSObject *key) {return CSObject::compareKey(key);}
	virtual int compareKey(CSOrderKey *x) {
		MSTableKey	*key = (MSTableKey *) x;
		int			r = 0;

		if (myKeyDatabaseID < key->myKeyDatabaseID)
			r = -1;
		else if (myKeyDatabaseID > key->myKeyDatabaseID)
			r = 1;
			
		if (r == 0) {
			if (myKeyTableID < key->myKeyTableID)
				r = -1;
			else if (myKeyTableID > key->myKeyTableID)
				r = 1;
		}
		return r;
	}

public:
	static MSTableKey *newTableKey(uint32_t db_id, uint32_t tab_id)
	{
		MSTableKey *key;

		if (!(key = new MSTableKey())) {
			CSException::throwOSError(CS_CONTEXT, ENOMEM);
		}
		key->myKeyDatabaseID = db_id;
		key->myKeyTableID = tab_id;
		return key;
	}
};

MSOpenTable *MSTableList::getOpenTableByID(uint32_t db_id, uint32_t tab_id)
{
	MSOpenTablePool		*pool;
	MSOpenTable			*otab = NULL;
	MSTableKey			key;

	enter_();
	lock_(gPoolListByID);
	key.myKeyDatabaseID = db_id;
	key.myKeyTableID = tab_id;
	pool = (MSOpenTablePool *) gPoolListByID->find(&key);
	if (!pool) {
		MSTableKey	*key_ptr;
		pool = MSOpenTablePool::newPool(db_id, tab_id);
		key_ptr = MSTableKey::newTableKey(db_id, tab_id);
		gPoolListByID->add(key_ptr, pool);
	}
	if (!(otab = pool->getPoolTable())) {
		otab = MSOpenTable::newOpenTable(pool);
		pool->addOpenTable(otab);
		otab->retain();
	}
	unlock_(gPoolListByID);
	return_(otab);
}

MSOpenTable *MSTableList::getOpenTableForDB(uint32_t db_id)
{
	return(MSTableList::getOpenTableByID(db_id, 0));
}


void MSTableList::releaseTable(MSOpenTable *otab)
{
	MSOpenTablePool	*pool;

	enter_();
	lock_(gPoolListByID);
	push_(otab);
	if ((pool = otab->myPool)) {
		if (pool->isRemovingTP) {
			pool->removeOpenTable(otab);
			gPoolListByID->wakeup();
		}
		else
			pool->returnOpenTable(otab);
	}
	release_(otab);
	unlock_(gPoolListByID);
	exit_();
}

bool MSTableList::removeTablePoolIfEmpty(MSOpenTablePool *pool)
{
	enter_();
	if (pool->getSize() == 0) {
		MSTableKey	key;
		
		key.myKeyDatabaseID = pool->myPoolDB->myDatabaseID;
		key.myKeyTableID = pool->myPoolTableID;
		gPoolListByID->remove(&key);
		/* TODO: Remove the table from the database, if it does not exist
		 * on disk.
		 */
		return_(true);
	}
	return_(false);
}

void MSTableList::removeTablePool(MSOpenTablePool *pool)
{
	enter_();
	lock_(gPoolListByID);
	for (;;) {
		pool->isRemovingTP = true;
		pool->removeOpenTablesNotInUse();
		if (removeTablePoolIfEmpty(pool)) 
			break;

		/*
		 * Wait for the tables that are in use to be
		 * freed.
		 */
		gPoolListByID->wait();
	}
	unlock_(gPoolListByID);
	exit_();
}

/*
 * Close the pool associated with this open table.
 */
void MSTableList::removeTablePool(MSOpenTable *otab)
{
	MSOpenTablePool *pool;
	MSTableKey	key;
	
	key.myKeyDatabaseID = otab->getDB()->myDatabaseID;
	key.myKeyTableID = otab->getTableID();

	enter_();
	frompool_(otab);
	lock_(gPoolListByID);
	for (;;) {
		if (!(pool = (MSOpenTablePool *) gPoolListByID->find(&key)))
			break;
		pool->isRemovingTP = true;
		pool->removeOpenTablesNotInUse();
		if (removeTablePoolIfEmpty(pool))
			break;
		/*
		 * Wait for the tables that are in use to be
		 * freed.
		 */
		gPoolListByID->wait();
	}
	unlock_(gPoolListByID);
	backtopool_(otab);
	exit_();
}

void MSTableList::removeDatabaseTables(MSDatabase *database)
{
	MSOpenTablePool *pool;
	uint32_t			idx;
	

	enter_();
	push_(database);
	
	retry:
	lock_(gPoolListByID);
	idx = 0;
	while ((pool = (MSOpenTablePool *) gPoolListByID->itemAt(idx))) {
		if (pool->myPoolDB == database) {
			break;
		}
		idx++;
	}
	unlock_(gPoolListByID);

	if (pool) {
		removeTablePool(pool);
		goto retry;
	}
	
	release_(database);
	exit_();
}

// lockTablePoolForDeletion() is only called to lock a pool for a table which is about  to be removed.
// When the pool is returned then it will be removed from the global pool list.
MSOpenTablePool *MSTableList::lockTablePoolForDeletion(uint32_t db_id, uint32_t tab_id, CSString *db_name, CSString *tab_name)
{
	MSOpenTablePool *pool;
	MSTableKey		key;

	enter_();

	push_(db_name);
	if (tab_name)
		push_(tab_name);
		
	key.myKeyDatabaseID = db_id;
	key.myKeyTableID = tab_id;
	
	lock_(gPoolListByID);

	for (;;) {
		if (!(pool = (MSOpenTablePool *) gPoolListByID->find(&key))) {
			char buffer[CS_EXC_MESSAGE_SIZE];

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Table is temporarily not available: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, db_name->getCString());
			if(tab_name) {
				cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, ".");
				cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, tab_name->getCString());
			}
			CSException::throwException(CS_CONTEXT, MS_ERR_TABLE_LOCKED, buffer);
		}
		pool->isRemovingTP = true;
		pool->removeOpenTablesNotInUse();
		if (pool->getSize() == 0) {
			// pool->retain();	Do not do this. The return to pool will free this by removing it from the list.	
			break;
		}
		/*
		 * Wait for the tables that are in use to be
		 * freed.
		 */
		gPoolListByID->wait();
	}
	unlock_(gPoolListByID);
	
	if (tab_name)
		release_(tab_name);
	release_(db_name);
	return_(pool);	
	
}

MSOpenTablePool *MSTableList::lockTablePoolForDeletion(MSTable *tab)
{
	CSString *tab_name = NULL, *db_name;
	uint32_t db_id, tab_id;
	
	enter_();

	db_name = tab->myDatabase->myDatabaseName;
	db_name->retain();

	tab_name = tab->myTableName;
	tab_name->retain();
	
	db_id = tab->myDatabase->myDatabaseID;
	tab_id = tab->myTableID;
	
	tab->release();
	
	return_( lockTablePoolForDeletion(db_id, tab_id, db_name, tab_name));
}

MSOpenTablePool *MSTableList::lockTablePoolForDeletion(MSOpenTable *otab)
{
	CSString *tab_name = NULL, *db_name;
	uint32_t db_id, tab_id;
	MSTable *tab;

	enter_();
	
	tab = otab->getDBTable();
	if (tab) {
		tab_name = tab->myTableName;
		tab_name->retain();
	}
	
	db_name = otab->getDB()->myDatabaseName;
	db_name->retain();

	db_id = otab->getDB()->myDatabaseID;
	tab_id = otab->getTableID();

	otab->returnToPool();

	return_( lockTablePoolForDeletion(db_id, tab_id, db_name, tab_name));
	
}


