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
 * 2007-05-25
 *
 * H&G2JCtL
 *
 * Network interface.
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
#endif

#include "cslib/CSConfig.h"

#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#include "string.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSLog.h"
#include "cslib/CSDirectory.h"
#include "cslib/CSStrUtil.h"

#include "database_ms.h"
#include "open_table_ms.h"
#include "backup_ms.h"
#include "table_ms.h"
#include "temp_log_ms.h"
#include "network_ms.h"
#include "mysql_ms.h"
#include "pbmslib.h"
#include "transaction_ms.h"
//#include "systab_variable_ms.h"
#include "systab_httpheader_ms.h"
#include "parameters_ms.h"
#include "pbmsdaemon_ms.h"



CSSyncSortedList	*MSDatabase::gDatabaseList;
CSSparseArray		*MSDatabase::gDatabaseArray;
/*
 * -------------------------------------------------------------------------
 * PBMS DATABASES
 */

MSDatabase::MSDatabase():
myIsPBMS(false),
myDatabaseID(0),
myDatabaseName(NULL),
myDatabasePath(NULL),
myTempLogArray(NULL),
myCompactorThread(NULL),
myTempLogThread(NULL),
myRepostoryList(NULL),
myBlobCloud(NULL),
myBlobType(MS_STANDARD_STORAGE),
isBackup(false),
iBackupThread(NULL),
iBackupTime(0),
iRecovering(false),
#ifdef HAVE_ALIAS_SUPPORT
iBlobAliases(NULL),
#endif
iClosing(false),
iTableList(NULL),
iTableArray(NULL),
iMaxTableID(0),
iWriteTempLog(NULL),
iDropping(false),
iNextBlobRefId(0)
{
//startTracking();
}

MSDatabase::~MSDatabase()
{
	iClosing = true;
	if (iBackupThread) {
		iBackupThread->stop();
		iBackupThread->release();
		iBackupThread = NULL;
	}
	
	if (myTempLogThread) {
		myTempLogThread->stop();
		myTempLogThread->release();
		myTempLogThread = NULL;
	}
	if (myCompactorThread) {
		myRepostoryList->wakeup(); // The compator thread waits on this.
		myCompactorThread->stop();
		myCompactorThread->release();
		myCompactorThread = NULL;
	}
	
	if (myDatabasePath)
		myDatabasePath->release();
		
	if (myDatabaseName)
		myDatabaseName->release();
		
	iWriteTempLog = NULL;
	if (myTempLogArray) {
		myTempLogArray->clear();
		myTempLogArray->release();
	}
	if (iTableList) {
		iTableList->clear();
		iTableList->release();
	}
	if (iTableArray){
		iTableArray->clear();
		iTableArray->release();
	}
	if (myRepostoryList) {
		myRepostoryList->clear();
		myRepostoryList->release();
	}
#ifdef HAVE_ALIAS_SUPPORT
	if (iBlobAliases) {
		iBlobAliases->ma_close();
		iBlobAliases->release();
	}
#endif
	if (myBlobCloud) {
		myBlobCloud->release();
	}
}

uint32_t MSDatabase::fileToTableId(const char *file_name, const char *name_part)
{
	uint32_t value = 0;

	if (file_name) {
		const char *num = file_name +  strlen(file_name) - 1;
		
		while (num >= file_name && *num != '-')
			num--;
		if (name_part) {
			/* Check the name part of the file: */
			int len = strlen(name_part);
			
			if (len != num - file_name)
				return 0;
			if (strncmp(file_name, name_part, len) != 0)
				return 0;
		}
		num++;
		if (isdigit(*num))
			sscanf(num, "%"PRIu32"", &value);
	}
	return value;
}

const char *MSDatabase::fileToTableName(size_t size, char *tab_name, const char *file_name)
{
	const char	*cptr;
	size_t		len;

	file_name = cs_last_name_of_path(file_name);
	cptr = file_name + strlen(file_name) - 1;
	while (cptr > file_name && *cptr != '.')
		cptr--;
	if (cptr > file_name && *cptr == '.') {
		if (strncmp(cptr, ".bs", 2) == 0) {
			cptr--;
			while (cptr > file_name && isdigit(*cptr))
				cptr--;
		}
	}

	len = cptr - file_name;
	if (len > size-1)
		len = size-1;

	memcpy(tab_name, file_name, len);
	tab_name[len] = 0;

	/* Return a pointer to what was removed! */
	return file_name + len;
}


const char *MSDatabase::getDatabaseNameCString()
{
	return myDatabaseName->getCString();
}

MSTable *MSDatabase::getTable(CSString *tab_name, bool create)
{
	MSTable *tab;
	
	enter_();
	push_(tab_name);
	lock_(iTableList);
	if (!(tab = (MSTable *) iTableList->find(tab_name))) {

		if (create) {
			/* Create a new table: */
			tab = MSTable::newTable(iMaxTableID+1, RETAIN(tab_name), this, (off64_t) 0, false);
			iTableList->add(tab);
			iTableArray->set(iMaxTableID+1, RETAIN(tab));
			iMaxTableID++;
		}
	}
	if (tab)
		tab->retain();
	unlock_(iTableList);
	release_(tab_name);
	return_(tab);
}

MSTable *MSDatabase::getTable(const char *tab_name, bool create)
{
	return getTable(CSString::newString(tab_name), create);
}


MSTable *MSDatabase::getTable(uint32_t tab_id, bool missing_ok)
{
	MSTable *tab;
	
	enter_();
	lock_(iTableList);
	if (!(tab = (MSTable *) iTableArray->get((uint32_t) tab_id))) {
		if (missing_ok) {
			unlock_(iTableList);
			return_(NULL);
		}
		char buffer[CS_EXC_MESSAGE_SIZE];

		cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown table #");
		cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, (uint32_t) tab_id);
		cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, " in database ");
		cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, getDatabaseNameCString());
		CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_TABLE, buffer);
	}
	tab->retain();
	unlock_(iTableList);
	return_(tab);
}

MSTable *MSDatabase::getNextTable(uint32_t *pos)
{
	uint32_t i = *pos;
	MSTable *tab = NULL;
	
	enter_();
	lock_(iTableList);
	while (i < iTableList->getSize()) {
		tab = (MSTable *) iTableList->itemAt(i++);
		if (!tab->isToDelete())
			break;
		tab = NULL;
	}
	if (tab)
		tab->retain();
	unlock_(iTableList);
	*pos = i;
	return_(tab);
}

void MSDatabase::addTable(uint32_t tab_id, const char *tab_name, off64_t file_size, bool to_delete)
{
	MSTable	*tab;

	if (tab_id > iMaxTableID)
		iMaxTableID = tab_id;
	tab = MSTable::newTable(tab_id, tab_name, this, file_size, to_delete);
	iTableList->add(tab);
	iTableArray->set(tab_id, RETAIN(tab));
}

void MSDatabase::addTableFromFile(CSDirectory *dir, const char *file_name, bool to_delete)
{
	off64_t	file_size;
	uint32_t	file_id;
	char	tab_name[MS_TABLE_NAME_SIZE];

	dir->info(NULL, &file_size, NULL);
	file_id = fileToTableId(file_name);
	fileToTableName(MS_TABLE_NAME_SIZE, tab_name, file_name);
	addTable(file_id, tab_name, file_size, to_delete);
}

void MSDatabase::removeTable(MSTable *tab)
{
	enter_();
	push_(tab);
	lock_(iTableList);
	iTableList->remove(tab->myTableName);
	iTableArray->remove(tab->myTableID);
	unlock_(iTableList);
	release_(tab);
	exit_();
}

void MSDatabase::dropTable(MSTable *tab)
{
	enter_();
	push_(tab);
	lock_(iTableList);
	iTableList->remove(tab->myTableName);
	iTableArray->remove(tab->myTableID);

	// Cute: you drop the table by adding it with the 'to_delete' flag set to 'true'
	addTable(tab->myTableID, tab->myTableName->getCString(), tab->getTableFileSize(), true);

	unlock_(iTableList);
	release_(tab);
	exit_();
}

// This function is used when dropping tables from a database before
// dropping the database itself. 
CSString *MSDatabase::getATableName()
{
	uint32_t i = 0;
	MSTable *tab;
	CSString *name = NULL;
	
	enter_();
	lock_(iTableList);

	while ((tab = (MSTable *) iTableList->itemAt(i++)) && tab->isToDelete()) ;
	if (tab) {
		name = tab->getTableName();
		name->retain();
	}
	unlock_(iTableList);
	return_(name);
}

uint32_t MSDatabase::getTableCount()
{
	uint32_t cnt = 0, i = 0;
	MSTable *tab;
	
	enter_();
	lock_(iTableList);

	while ((tab = (MSTable *) iTableList->itemAt(i++))) {
		if (!tab->isToDelete())
			cnt++;
	}

	unlock_(iTableList);
	return_(cnt);
}


void MSDatabase::renameTable(MSTable *tab, const char *to_name)
{
	enter_();
	lock_(iTableList);
	iTableList->remove(tab->myTableName);
	iTableArray->remove(tab->myTableID);

	addTable(tab->myTableID, to_name, tab->getTableFileSize(), false);

	unlock_(iTableList);
	exit_();
}

void MSDatabase::openWriteRepo(MSOpenTable *otab)
{
	if (otab->myWriteRepo && otab->myWriteRepoFile)
		return;

	enter_();
	if (!otab->myWriteRepo)
		otab->myWriteRepo = lockRepo(0);

	/* Now open the repo file for the open table: */
	otab->myWriteRepo->openRepoFileForWriting(otab);
	exit_();
}

MSRepository *MSDatabase::getRepoFullOfTrash(time_t *ret_wait_time)
{
	MSRepository	*repo = NULL;
	time_t			wait_time = 0;
	
	if (ret_wait_time)
		wait_time = *ret_wait_time;
	enter_();
	lock_(myRepostoryList);
	for (uint32_t i=0; i<myRepostoryList->size(); i++) {
		retry:
		if ((repo = (MSRepository *) myRepostoryList->get(i))) {
			if (!repo->isRemovingFP && !repo->mustBeDeleted && !repo->isRepoLocked()) {
				if (!repo->myRepoHeadSize) {
					/* The file has not yet been opened, so the
					 * garbage count will not be known!
					 */
					MSRepoFile *repo_file;

					repo->retain();
					unlock_(myRepostoryList);
					push_(repo);
					repo_file = repo->openRepoFile();
					repo_file->release();
					release_(repo);
					lock_(myRepostoryList);
					goto retry;
				}
				if (repo->getGarbageLevel() >= PBMSParameters::getGarbageThreshold()) {
					/* Make sure there are not temp BLOBs in this repository that have
					 * not yet timed out:
					 */
					time_t now = time(NULL);
					time_t then = repo->myLastTempTime;

					/* Check if there are any temp BLOBs to be removed: */
					if (now > (time_t)(then + PBMSParameters::getTempBlobTimeout())) {
						repo->lockRepo(REPO_COMPACTING); 
						repo->retain();
						break;
					}
					else {
						/* There are temp BLOBs to wait for... */
						if (!wait_time || wait_time > MSTempLog::adjustWaitTime(then, now))
							wait_time = MSTempLog::adjustWaitTime(then, now);
					}
				}
			}
			repo = NULL;
		}
	}
	unlock_(myRepostoryList);
	if (ret_wait_time)
		*ret_wait_time = wait_time;
	return_(repo);
}

MSRepository *MSDatabase::lockRepo(off64_t size)
{
	MSRepository	*repo;
	uint32_t			free_slot;

	enter_();
	lock_(myRepostoryList);
	free_slot = myRepostoryList->size();
	/* Find an unlocked repository file that is below the write threshold: */
	for (uint32_t i=0; i<myRepostoryList->size(); i++) {
		if ((repo = (MSRepository *) myRepostoryList->get(i))) {
			if ((!repo->isRepoLocked()) && (!repo->isRemovingFP) && (!repo->mustBeDeleted) &&
				((repo->myRepoFileSize + size) < PBMSParameters::getRepoThreshold()) 
				/**/ && (repo->getGarbageLevel() < PBMSParameters::getGarbageThreshold()))
				goto found1;
		}
		else {
			if (i < free_slot)
				free_slot = i;
		}
	}

	/* None found, create a new repo file: */
	new_(repo, MSRepository(free_slot + 1, this, 0));
	myRepostoryList->set(free_slot, repo);

	found1:
	repo->retain();
	repo->lockRepo(REPO_WRITE);  // <- The MSRepository::backToPool() will unlock this.
	unlock_(myRepostoryList);
	return_(repo);
}

MSRepoFile *MSDatabase::getRepoFileFromPool(uint32_t repo_id, bool missing_ok)
{
	MSRepository	*repo;
	MSRepoFile		*file;

	enter_();
	lock_(myRepostoryList);
	if (!(repo = (MSRepository *) myRepostoryList->get(repo_id - 1))) {
		if (!missing_ok) {
			char buffer[CS_EXC_MESSAGE_SIZE];

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown repository file: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, (uint32_t) repo_id);
			CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, buffer);
		}
		unlock_(myRepostoryList);
		return_(NULL);
	}
	if (repo->isRemovingFP) {
		char buffer[CS_EXC_MESSAGE_SIZE];

		cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Repository will be removed: ");
		cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, (uint32_t) repo_id);
		CSException::throwException(CS_CONTEXT, MS_ERR_REMOVING_REPO, buffer);
	}
	repo->retain(); /* Release is here: [++] */
	file = repo->getRepoFile();
	unlock_(myRepostoryList);

	if (!file) {
		file = repo->openRepoFile();
		lock_(myRepostoryList);
		repo->addRepoFile(RETAIN(file));
		unlock_(myRepostoryList);
	}
	return_(file);
}

void MSDatabase::returnRepoFileToPool(MSRepoFile *file)
{
	MSRepository	*repo;

	enter_();
	lock_(myRepostoryList);
	push_(file);
	if ((repo = file->myRepo)) {
		if (repo->isRemovingFP) {
			repo->removeRepoFile(file); // No retain expected
			myRepostoryList->wakeup();
		}
		else
			repo->returnRepoFile(file); // No retain expected
		repo->release(); /* [++] here is the release.  */
	}
	release_(file);
	unlock_(myRepostoryList);
	exit_();
}

void MSDatabase::removeRepo(uint32_t repo_id, bool *mustQuit)
{
	MSRepository *repo;

	enter_();
	lock_(myRepostoryList);
	while ((!mustQuit || !*mustQuit) && !iClosing) {
		if (!(repo = (MSRepository *) myRepostoryList->get(repo_id - 1)))
			break;
		repo->isRemovingFP = true;
		if (repo->removeRepoFilesNotInUse()) {
			myRepostoryList->set(repo_id - 1, NULL);
			break;
		}
		/*
		 * Wait for the files that are in use to be
		 * freed.
		 */
		myRepostoryList->wait();
	}
	unlock_(myRepostoryList);
	exit_();
}

void MSDatabase::queueTempLogEvent(MSOpenTable *otab, int type, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code,
	 uint32_t *log_id, uint32_t *log_offset, uint32_t *q_time)
{
	MSTempLogItemRec	item;
	uint32_t				timev;

	// Each otab object holds an handle to an instance of an OPEN
	// temp log. This is so that each thread has it's own open temp log 
	// and doesn't need to be opened and close it constantly.
	
	enter_();
	lock_(myTempLogArray);
	if (!iWriteTempLog) {
		iWriteTempLog = (MSTempLog *) myTempLogArray->last();
		if (!iWriteTempLog) {
			new_(iWriteTempLog, MSTempLog(1, this, 0));
			myTempLogArray->set(1, iWriteTempLog);
		}
	}
	if (!otab->myTempLogFile)
		otab->myTempLogFile = iWriteTempLog->openTempLog();
	else if (otab->myTempLogFile->myTempLogID != iWriteTempLog->myLogID) {
		otab->myTempLogFile->release();
		otab->myTempLogFile = NULL;
		otab->myTempLogFile = iWriteTempLog->openTempLog();
	}

	if (iWriteTempLog->myTempLogSize >= PBMSParameters::getTempLogThreshold()) {
		uint32_t tmp_log_id = iWriteTempLog->myLogID + 1;

		new_(iWriteTempLog, MSTempLog(tmp_log_id, this, 0));
		myTempLogArray->set(tmp_log_id, iWriteTempLog);

		otab->myTempLogFile->release();
		otab->myTempLogFile = NULL;
		otab->myTempLogFile = iWriteTempLog->openTempLog();

	}

	timev = time(NULL);
	*log_id = iWriteTempLog->myLogID;
	*log_offset = (uint32_t) iWriteTempLog->myTempLogSize;
	if (q_time)
		*q_time = timev;
	iWriteTempLog->myTempLogSize += iWriteTempLog->myTemplogRecSize;
	unlock_(myTempLogArray);

	CS_SET_DISK_1(item.ti_type_1, type);
	CS_SET_DISK_4(item.ti_table_id_4, tab_id);
	CS_SET_DISK_6(item.ti_blob_id_6, blob_id);
	CS_SET_DISK_4(item.ti_auth_code_4, auth_code);
	CS_SET_DISK_4(item.ti_time_4, timev);
	otab->myTempLogFile->write(&item, *log_offset, sizeof(MSTempLogItemRec));
	
	
	exit_();
}

#ifdef HAVE_ALIAS_SUPPORT
void MSDatabase::queueForDeletion(MSOpenTable *otab, int type, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code,
	 uint32_t *log_id, uint32_t *log_offset, uint32_t *q_time, MSDiskAliasPtr aliasDiskRec)
{
	enter_();
	
	queueTempLogEvent(otab, type, tab_id, blob_id, auth_code, log_id, log_offset, q_time);
		
	// If it has an alias remove it from the ailias index.
	if (aliasDiskRec) {
		try_(a) {
			deleteBlobAlias(aliasDiskRec);
		}
		catch_(a);
		self->logException();
		cont_(a);
	}
	
	exit_();
}
#endif

MSTempLogFile *MSDatabase::openTempLogFile(uint32_t log_id, size_t *log_rec_size, size_t *log_head_size)
{
	MSTempLog		*log;
	MSTempLogFile	*log_file = NULL;

	enter_();
	lock_(myTempLogArray);
	if (log_id)
		log = (MSTempLog *) myTempLogArray->get(log_id);
	else
		log = (MSTempLog *) myTempLogArray->first();
	if (log) {
		log_file = log->openTempLog();
		if (log_rec_size)
			*log_rec_size = log->myTemplogRecSize;
		if (log_head_size)
			*log_head_size = log->myTempLogHeadSize;
	}
	unlock_(myTempLogArray);
	return_(log_file);
}

uint32_t MSDatabase::getTempLogCount()
{
	uint32_t count;

	enter_();
	lock_(myTempLogArray);
	count = myTempLogArray->size();
	unlock_(myTempLogArray);
	return_(count);
}

void MSDatabase::removeTempLog(uint32_t log_id)
{
	enter_();
	lock_(myTempLogArray);
	myTempLogArray->remove(log_id);
	unlock_(myTempLogArray);
	exit_();
}

CSObject *MSDatabase::getKey()
{
	return (CSObject *) myDatabaseName;
}

int MSDatabase::compareKey(CSObject *key)
{
	return myDatabaseName->compare((CSString *) key);
}

MSCompactorThread *MSDatabase::getCompactorThread()
{
	return myCompactorThread;
}

CSSyncVector *MSDatabase::getRepositoryList()
{	
	return myRepostoryList;
}

#ifdef HAVE_ALIAS_SUPPORT
uint32_t MSDatabase::registerBlobAlias(uint32_t repo_id, uint64_t repo_offset, const char *alias)
{
	uint32_t hash;
	bool can_retry = true;
	enter_();
	
retry:
	lock_(&iBlobAliaseLock);
	
	try_(a) {
		hash = iBlobAliases->addAlias(repo_id, repo_offset, alias);
	}
	
	catch_(a) {
		unlock_(&iBlobAliaseLock);
		if (can_retry) {
			// It can be that a duplicater alias exists that was deleted
			// but the transaction has not been written to the repository yet.
			// Flush all committed transactions to the repository file.
			MSTransactionManager::flush();
			can_retry = false;
			goto retry;
		}
		throw_();
	}
	
	cont_(a);
	unlock_(&iBlobAliaseLock);
	return_(hash);
}

uint32_t MSDatabase::updateBlobAlias(uint32_t repo_id, uint64_t repo_offset, uint32_t old_alias_hash, const char *alias)
{
	uint32_t new_hash;
	enter_();
	lock_(&iBlobAliaseLock);
	
	new_hash = iBlobAliases->addAlias(repo_id, repo_offset, alias);
	iBlobAliases->deleteAlias(repo_id, repo_offset, old_alias_hash);

	unlock_(&iBlobAliaseLock);
	return_(new_hash);
}

void MSDatabase::deleteBlobAlias(MSDiskAliasPtr diskRec)
{
	enter_();
	lock_(&iBlobAliaseLock);
	iBlobAliases->deleteAlias(diskRec);
	unlock_(&iBlobAliaseLock);
	exit_();
}

void MSDatabase::deleteBlobAlias(uint32_t repo_id, uint64_t repo_offset, uint32_t alias_hash)
{
	MSDiskAliasRec diskRec;
	
	CS_SET_DISK_4(diskRec.ar_repo_id_4, repo_id);	
	CS_SET_DISK_8(diskRec.ar_offset_8, repo_offset);	
	CS_SET_DISK_4(diskRec.ar_hash_4, alias_hash);
	deleteBlobAlias(&diskRec);
}

void MSDatabase::moveBlobAlias(uint32_t old_repo_id, uint64_t old_repo_offset, uint32_t alias_hash, uint32_t new_repo_id, uint64_t new_repo_offset)
{
	enter_();
	lock_(&iBlobAliaseLock);
	iBlobAliases->resetAlias(old_repo_id, old_repo_offset, alias_hash, new_repo_id, new_repo_offset);
	unlock_(&iBlobAliaseLock);
	exit_();
}
#endif

bool MSDatabase::isValidHeaderField(const char *name)
{
	bool is_valid = false;
	CSString		*header;
	enter_();

	if (name && *name) {
		if (strcasecmp(name, MS_ALIAS_TAG)) {
			lock_(&iHTTPMetaDataHeaders);
			header = CSString::newString(name);
			push_(header);
				
			is_valid = (iHTTPMetaDataHeaders.find(header) != NULL);
			release_(header);
			
			unlock_(&iHTTPMetaDataHeaders);
		} else 
			is_valid = true;
	}
	
	return_(is_valid);
}

void MSDatabase::startUp(const char *default_http_headers)
{
	enter_();
	
	new_(gDatabaseList, CSSyncSortedList);
	new_(gDatabaseArray, CSSparseArray(5));
	MSHTTPHeaderTable::setDefaultMetaDataHeaders(default_http_headers);
	PBMSSystemTables::systemTablesStartUp();
	PBMSParameters::setBackupDatabaseID(1);
	exit_();
}

void MSDatabase::stopThreads()
{
	MSDatabase *db;

	enter_();
	if (gDatabaseList) {
		lock_(gDatabaseList);
		for (int i=0;;i++) {
			if (!(db = (MSDatabase *) gDatabaseList->itemAt(i)))
				break;
			db->iClosing = true;
			
			if (db->myTempLogThread) {
				db->myTempLogThread->stop();
				db->myTempLogThread->release();
				db->myTempLogThread = NULL;
			}
			if (db->myCompactorThread) {
				db->myRepostoryList->wakeup(); // The compator thread waits on this.
				db->myCompactorThread->stop();
				db->myCompactorThread->release();
				db->myCompactorThread = NULL;
			}
			
			if (db->iBackupThread) {
				db->iBackupThread->stop();
				db->iBackupThread->release();
				db->iBackupThread = NULL;
			}
	
		}
		
		unlock_(gDatabaseList);
	}
	exit_();
}

void MSDatabase::shutDown()
{
		
	if (gDatabaseArray) {
		gDatabaseArray->clear();
		gDatabaseArray->release();
		gDatabaseArray = NULL;
	}
	
	if (gDatabaseList) {
		gDatabaseList->clear();
		gDatabaseList->release();
		gDatabaseList = NULL;
	}
	
	MSHTTPHeaderTable::releaseDefaultMetaDataHeaders();
	PBMSSystemTables::systemTableShutDown();
}

void MSDatabase::setBackupDatabase()
{
	enter_();
	// I need to give the backup database a unique fake database ID.
	// This is so that it is not confused with the database being backed
	// backed up when opening tables.
	
	// Normally database IDs are generated by time(NULL) so small database IDs
	// are safe to use as fake IDs.
	 
	lock_(gDatabaseList);
	myDatabaseID = PBMSParameters::getBackupDatabaseID() +1;
	PBMSParameters::setBackupDatabaseID(myDatabaseID);
	gDatabaseArray->set(myDatabaseID, RETAIN(this));
	isBackup = true;
	
	// Notify the cloud storage, if any, that it is a backup.
	// This is important because if the backup database is dropped
	// we need to be sure that only the BLOBs belonging to the
	// backup are removed from the cloud.
	myBlobCloud->cl_setCloudIsBackup(); 
	
	unlock_(gDatabaseList);
	
	// Rename the database path so that it is obviouse that this is an incomplete backup database.
	// When the backup is completed it will be renamed back.
	CSPath *new_path = CSPath::newPath(myDatabasePath->concat("#"));
	push_(new_path);
	
	if (new_path->exists()) 
		new_path->remove();
	
	CSPath *db_path = CSPath::newPath(RETAIN(myDatabasePath));
	push_(db_path);
	
	db_path->rename(new_path->getNameCString());
	myDatabasePath->release();
	myDatabasePath = new_path->getString();
	myDatabasePath->retain();
	
	release_(db_path);
	release_(new_path);
	
	
	exit_();
}

void MSDatabase::releaseBackupDatabase()
{
	enter_();

	
	// The backup has completed succefully, rename the path to the correct name.
	CSPath *db_path = CSPath::newPath(myDatabasePath->getCString());
	push_(db_path);
	
	myDatabasePath->setLength(myDatabasePath->length()-1);
	db_path->rename(cs_last_name_of_path(myDatabasePath->getCString()));
	release_(db_path);
	
	// Remove the backup database object.
	lock_(gDatabaseList);
	gDatabaseArray->remove(myDatabaseID);
	MSTableList::removeDatabaseTables(this); // Will also release the database object.
	unlock_(gDatabaseList);
	
	
	exit_();
}

void MSDatabase::startBackup(MSBackupInfo *backup_info)
{
	enter_();

	push_(backup_info);
	if (iBackupThread) {
		if (iBackupThread->isRunning()) {
			CSException::throwException(CS_CONTEXT, MS_ERR_DUPLICATE_DB, "A backup is still running.");
		}
		iBackupThread->release();
		iBackupThread = NULL;
	}
	
	pop_(backup_info);
	iBackupThread = MSBackup::newMSBackup(backup_info);
	
	try_(a) {
		iBackupThread->startBackup(RETAIN(this));
	}
	
	catch_(a) {
		iBackupThread->release();
		iBackupThread = NULL;
		throw_();
	}
	cont_(a);
	
	exit_();
}

bool MSDatabase::backupStatus(uint64_t *total, uint64_t *completed, bool *completed_ok)
{
	bool done;
	
	enter_();
	
	if (iBackupThread) {
		*total = iBackupThread->getBackupSize();
		*completed = iBackupThread->getBackupCompletedSize();
		done = !iBackupThread->isRunning();
		*completed = (iBackupThread->getStatus() == 0);
	} else {
		*completed_ok = done = true;
		*total = *completed = 0;			
	}
		
	return_(done);
}

uint32_t MSDatabase::backupID()
{ 
	return (iBackupThread)?iBackupThread->backupID(): 0;
}

void MSDatabase::terminateBackup()
{
	if (iBackupThread) {
		iBackupThread->stop();
		iBackupThread->release();
		iBackupThread = NULL;
	}
}

MSDatabase *MSDatabase::getDatabase(CSString *db_name, bool create)
{
	MSDatabase *db;
	enter_();
	push_(db_name);
	
	
	lock_(gDatabaseList);
	if (!(db = (MSDatabase *) gDatabaseList->find(db_name))) {
		db = MSDatabase::loadDatabase(RETAIN(db_name), create);
		if (!db)
			goto exit;
	} else
		db->retain();
	
	exit:
	unlock_(gDatabaseList);
	release_(db_name);
	return_(db);
}

MSDatabase *MSDatabase::getDatabase(const char *db_name, bool create)
{
	return getDatabase(CSString::newString(db_name), create);
}

MSDatabase *MSDatabase::getDatabase(uint32_t db_id, bool missing_ok)
{
	MSDatabase *db;
	
	enter_();
	lock_(gDatabaseList);
	if ((db = (MSDatabase *) gDatabaseArray->get((uint32_t) db_id))) 
		db->retain();
	else {
		// Look for the database folder with the correct ID:
		CSPath *path = CSPath::newPath(PBMSDaemon::getPBMSDir());
		push_(path);
		if (path->exists()) {
			CSDirectory *dir;
			dir = CSDirectory::newDirectory(RETAIN(path));
			push_(dir);
			dir->open();
			
			while (dir->next() && !db) {
				if (!dir->isFile()) {
					const char *ptr, *dir_name  = dir->name();
					ptr = dir_name + strlen(dir_name) -1;
					
					while (ptr > dir_name && *ptr != '-') ptr--;
					
					if (*ptr ==  '-') {
						int len = ptr - dir_name;
						ptr++;
						if ((strtoul(ptr, NULL, 10) == db_id) && len) {
							db = getDatabase(CSString::newString(dir_name, len), true);
							ASSERT(db->myDatabaseID == db_id);
						}
					}
				}
			}
			release_(dir);
		}
		release_(path);		
	}
	unlock_(gDatabaseList);
	
	if ((!db) && !missing_ok) {
		char buffer[CS_EXC_MESSAGE_SIZE];

		cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown database #");
		cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, (uint32_t) db_id);
		CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_DB, buffer);
	}
	return_(db);
}

void MSDatabase::wakeTempLogThreads()
{
	MSDatabase *db;

	if (!gDatabaseList)
		return;
	enter_();
	lock_(gDatabaseList);
	for (int i=0;;i++) {
		if (!(db = (MSDatabase *) gDatabaseList->itemAt(i)))
			break;
		if (db->myTempLogThread)
			db->myTempLogThread->wakeup();
	}
	unlock_(gDatabaseList);
	exit_();
}

uint32_t MSDatabase::getDBID(CSPath *path, CSString *db_name)
{
	CSDirectory		*dir;
	uint32_t			db_id = 0;
	int				len = db_name->length();
	const char		*ptr;
	
	enter_();
	push_(db_name);
	push_(path);
	
	// Search for the ID of the database
	dir = CSDirectory::newDirectory(RETAIN(path));
	push_(dir);
	dir->open();
	while (dir->next() && !db_id) 
		{
		if (!dir->isFile()){
			ptr = dir->name() + strlen(dir->name()) -1;
			while (ptr > dir->name() && isdigit(*ptr)) ptr--;
			if ((*ptr == '-') && (len == (ptr - dir->name())) && !db_name->compare(dir->name(), len) ) {
				db_id = atol(ptr+1);				
			}
		}
	}
	release_(dir);
	
	if (!db_id) {
		db_id = time(NULL);

		while (1) { // search for a unique db_id
			dir = CSDirectory::newDirectory(RETAIN(path));
			push_(dir);
			dir->open();
			while (db_id && dir->next()) {
				if (!dir->isFile()) {
					ptr = dir->name() + strlen(dir->name()) -1;
					while (ptr > dir->name() && isdigit(*ptr)) ptr--;
					if ((*ptr == '-') && (db_id == strtoul(ptr+1, NULL, 10))) {
						db_id = 0;				
					}
				}
			}
			release_(dir);
			if (db_id)
				break;
			sleep(1); // Allow 1 second to pass.
			db_id = time(NULL);
		} 
	}
	
	release_(path);
	release_(db_name);
	return_(db_id);
}

CSPath *MSDatabase::createDatabasePath(const char *location, CSString *db_name, uint32_t *db_id_ptr, bool *create, bool is_pbms)
{
	bool create_path = *create;	
	CSPath *path = NULL;
	char name_buffer[MS_DATABASE_NAME_SIZE + 40];
	uint32_t db_id;
	enter_();
	
	push_(db_name);
	*create = false;
	path = CSPath::newPath(location, "pbms");
	push_(path);
	if (!path->exists()) {
		if (!create_path){
			release_(path);
			path = NULL;
			goto done;
		}
			
		*create = true;
		path->makeDir();
	}

	// If this is the pbms database then nothing more is to be done.
	if (is_pbms)
		goto done;
	
	if ((!db_id_ptr) || !*db_id_ptr) {
		db_id = getDBID(RETAIN(path), RETAIN(db_name));
		if (db_id_ptr)
			*db_id_ptr = db_id;
	} else
		db_id = *db_id_ptr;

	// Create the PBMS database name with ID
	cs_strcpy(MS_DATABASE_NAME_SIZE + 40, name_buffer, db_name->getCString());
	cs_strcat(MS_DATABASE_NAME_SIZE + 40, name_buffer, "-");
	cs_strcat(MS_DATABASE_NAME_SIZE + 40, name_buffer, (uint32_t) db_id);
			
	pop_(path);
	path = CSPath::newPath(path, name_buffer);
	push_(path);
	if (!path->exists()) {
		if (create_path) {
			*create = true;
			path->makeDir();
		} else {
			release_(path);
			path = NULL;
		}
			
	}
	
done:
	if (path)
		pop_(path);
	release_(db_name);
	return_(path);
}



MSDatabase *MSDatabase::newDatabase(const char *db_location, CSString *db_name, uint32_t	db_id, bool create)
{
	MSDatabase		*db = NULL;
	CSDirectory		*dir;
	MSRepository	*repo;
	CSPath			*path;
	const char		*file_name;
	uint32_t		file_id;
	off64_t			file_size;
	MSTempLog		*log;
	uint32_t		to_delete = 0;
	CSString		*db_path;
	bool			is_pbms = false;

	enter_();

	push_(db_name);

	//is_pbms = (strcmp(db_name->getCString(), "pbms") == 0); To be done later.
	
	/*
	 * Block the creation of the pbms database if there is no MySQL database. 
	 * The database name is case sensitive here if the file system names are
	 * case sensitive. This is desirable.
	 */
	path = CSPath::newPath(ms_my_get_mysql_home_path(), RETAIN(db_name));
	push_(path);
	if (create && !path->exists()) {
		CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_DB, db_name->getCString());
	}
	release_(path);
	
	 // Create the database path, if 'create' == false then it can return NULL
	path = createDatabasePath(db_location, RETAIN(db_name), &db_id, &create, is_pbms);
	if (!path) {
		release_(db_name);
		return_(NULL);
	}
	push_(path);
	
	// Create the database object and initialize it.
	if (!(db = new MSDatabase())) {
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	
	db->myIsPBMS = is_pbms;
	db_path	= path->getString();
	db_path->retain();
	release_(path);
	path = NULL;
		
	db->iNextBlobRefId = (uint32_t) time(NULL);
	db->iNextBlobRefId <<= 32;
	db->iNextBlobRefId = COMMIT_MASK(db->iNextBlobRefId);
	db->iNextBlobRefId++;
	
	db->myDatabaseID = db_id;
	db->myDatabasePath = db_path;
	db->myDatabaseName = db_name;
	new_(db->myBlobCloud, CloudDB(db_id));

	pop_(db_name);	
	
	push_(db);
	
	
	new_(db->myTempLogArray, CSSyncSparseArray(20));
	new_(db->iTableList, CSSyncSortedList());
	new_(db->iTableArray, CSSparseArray(20));
	new_(db->myRepostoryList, CSSyncVector(20));
	
	if (!is_pbms) { //
#ifdef HAVE_ALIAS_SUPPORT
		//db->retain(); no retain here, MSAlias() takes a back ref.
		new_(db->iBlobAliases, MSAlias(db));
#endif		
		/* "Load" the database: */

		/* Get the max table ID: */
		dir = CSDirectory::newDirectory(RETAIN(db_path));
		push_(dir);
		dir->open();
		while (dir->next()) {
			file_name = dir->name();
			if (dir->isFile() && cs_is_extension(file_name, "bst"))
				db->addTableFromFile(dir, file_name, false);
		}
		release_(dir);

		path = CSPath::newPath(RETAIN(db_path), "bs-repository");
		if (path->exists()) {
			dir = CSDirectory::newDirectory(path);
			push_(dir);
			dir->open();
			while (dir->next()) {
				file_name = dir->name();
				if (dir->isFile() && cs_is_extension(file_name, "bs")) {
					if ((file_id = fileToTableId(file_name, "repo"))) {
						dir->info(NULL, &file_size, NULL);
						new_(repo, MSRepository(file_id, db, file_size));
						db->myRepostoryList->set(file_id - 1, repo);
					}
				}
			}
			release_(dir);
		}
		else {
			path->makeDir();
			path->release();
		}

		path = CSPath::newPath(RETAIN(db_path), "bs-logs");
		if (path->exists()) {
			dir = CSDirectory::newDirectory(path);
			push_(dir);
			dir->open();
			while (dir->next()) {
				file_name = dir->name();
				if (dir->isFile()) {
					if (cs_is_extension(file_name, "bs")) {
						if ((file_id = fileToTableId(file_name, "temp"))) {
							dir->info(NULL, &file_size, NULL);
							new_(log, MSTempLog(file_id, db, file_size));
							db->myTempLogArray->set(file_id, log);
						}
					}
					else if (cs_is_extension(file_name, "bst")) {
						db->addTableFromFile(dir, file_name, true);
						to_delete++;
					}
				}
			}
			release_(dir);
		}
		else {
			path->makeDir();
			path->release();
		}

		if (to_delete) {
			/* Go through and prepare all the tables that are to
			 * be deleted:
			 */
			uint32_t	i = 0;
			MSTable	*tab;
			
			while ((tab = (MSTable *) db->iTableList->itemAt(i))) {
				if (tab->isToDelete())
					tab->prepareToDelete();
				i++;
			}
		}

	}
	pop_(db);

	return_(db);
}

void MSDatabase::startThreads()
{
	enter_();

	if (myIsPBMS)
		exit_();
		
#ifdef HAVE_ALIAS_SUPPORT
	// iBlobAliases->ma_open() must be called before starting any threads.
	iBlobAliases->ma_open(); 
#endif

	new_(myTempLogThread, MSTempLogThread(1 * 1000, this));
	myTempLogThread->start();

#ifdef MS_COMPACTOR_POLLS
	new_(myCompactorThread, MSCompactorThread(MS_COMPACTOR_POLL_FREQ, this));
#else
	new_(myCompactorThread, MSCompactorThread(MS_DEFAULT_COMPACTOR_WAIT * 1000, this));
#endif

	
	myCompactorThread->start();
	exit_();
}


void MSDatabase::dropDatabase() 
{
	enter_();

	iDropping = true;
	iClosing = true;
	
	if (iBackupThread) {
		iBackupThread->stop();
		iBackupThread->release();
		iBackupThread = NULL;
	}
	
	if (myTempLogThread) {
		myTempLogThread->stop();
		myTempLogThread->release();
		myTempLogThread = NULL;
	}
	
	if (myCompactorThread) {
		myRepostoryList->wakeup(); // The compator thread waits on this.
		myCompactorThread->stop();
		myCompactorThread->release();
		myCompactorThread = NULL;
	}
	
	// Call cloud drop database even if the database is not currrently
	// using cloud storage just in case to was in the past. If the connection
	// to the cloud is not setup then nothing will be done.
	try_(a) {
		myBlobCloud->cl_dropDB();
	}
	catch_(a) {
		self->logException();
	}
	cont_(a);
	exit_();
}

void MSDatabase::removeDatabasePath(CSString *doomedDatabasePath ) 
{
	CSPath *path = NULL;
	CSDirectory *dir = NULL;
	const char *file_name;
	enter_();
	
	push_(doomedDatabasePath);
	
	// Delete repository files
	path = CSPath::newPath(RETAIN(doomedDatabasePath), "bs-repository");
	push_(path);
	if (path->exists()) {
		dir = CSDirectory::newDirectory(RETAIN(path));
		push_(dir);
		dir->open();
		while (dir->next()) {
			file_name = dir->name();
			if (dir->isFile() && cs_is_extension(file_name, "bs")) {
				dir->deleteEntry();
			}
		}
		release_(dir);
		if (path->isEmpty())
			path->removeDir();
	}
	release_(path);

	// Delete temp log files.
	path = CSPath::newPath(RETAIN(doomedDatabasePath), "bs-logs");
	push_(path);
	if (path->exists()) {
		dir = CSDirectory::newDirectory(RETAIN(path));
		push_(dir);
		dir->open();
		while (dir->next()) {
			file_name = dir->name();
			if (dir->isFile() && (cs_is_extension(file_name, "bs") || cs_is_extension(file_name, "bst"))) {
				dir->deleteEntry();
			}
		}
		release_(dir);
		if (path->isEmpty())
			path->removeDir();
	}
	release_(path);

	// Delete table reference files.
	dir = CSDirectory::newDirectory(RETAIN(doomedDatabasePath));
	push_(dir);
	dir->open();
	while (dir->next()) {
		file_name = dir->name();
		if (dir->isFile() && cs_is_extension(file_name, "bst"))
			dir->deleteEntry();
	}
	release_(dir);

#ifdef HAVE_ALIAS_SUPPORT
	path = CSPath::newPath(RETAIN(doomedDatabasePath), ACTIVE_ALIAS_INDEX);
	push_(path);
	path->removeFile();
	release_(path);
#endif

	PBMSSystemTables::removeSystemTables(RETAIN(doomedDatabasePath));
	
	path = CSPath::newPath(RETAIN(doomedDatabasePath));
	push_(path);
	if (path->isEmpty() && !path->isLink()) {
		path->removeDir();
	} else { 
		CSStringBuffer *new_name;
		// If the database folder is not empty we rename it to get it out of the way.
		// If it is not renamed it will be reused if a database with the same name is
		// created again wich will result in the database ID being reused which may
		// have some bad side effects.
		new_(new_name, CSStringBuffer());
		push_(new_name);
		new_name->append(cs_last_name_of_path(doomedDatabasePath->getCString()));
		new_name->append("_DROPPED");
		path->rename(new_name->getCString());
		release_(new_name);
	}
	release_(path);

	release_(doomedDatabasePath);
	
	path = CSPath::newPath(PBMSDaemon::getPBMSDir());
	push_(path);
	if (path->isEmpty() && !path->isLink()) {
		path->removeDir();
	}
	release_(path);

	exit_();
}

/* Drop the PBMS database if it exists.
 * The root folder 'pbms' will be deleted also
 * if it is empty and not a symbolic link.
 * The database folder in 'pbms' is deleted if it is empty and
 * it is not a symbolic link.
 */
void MSDatabase::dropDatabase(MSDatabase *doomedDatabase, const char *db_name ) 
{
	CSString *doomedDatabasePath = NULL;

	enter_();
	
	if (doomedDatabase) {
		push_(doomedDatabase);
		
		// Remove any pending transactions for the dropped database.
		// This is important because if the database is restored it will have the
		// same database ID and the old transactions would be applied to it.
		MSTransactionManager::dropDatabase(doomedDatabase->myDatabaseID);
		
		doomedDatabasePath = doomedDatabase->myDatabasePath;
		doomedDatabasePath->retain(); // Hold on to this path after the database has been released.
		
		MSTableList::removeDatabaseTables(RETAIN(doomedDatabase));
		MSSystemTableShare::removeDatabaseSystemTables(RETAIN(doomedDatabase));

		doomedDatabase->dropDatabase(); // Shutdown database threads.
		
		// To avoid a deadlock a lock is not taken on the database list
		// if shutdown is in progress. The only database that would be
		// dropped during a shutdown is an incomplete backup database.
		ASSERT(doomedDatabase->isBackup || !self->myMustQuit);
		if (!self->myMustQuit) 
			lock_(gDatabaseList); // Be sure to shutdown the database before locking this or it can lead to deadlocks
			
		gDatabaseArray->remove(doomedDatabase->myDatabaseID);
		if (!doomedDatabase->isBackup)
			gDatabaseList->remove(doomedDatabase->getKey());
		if (!self->myMustQuit) 
			unlock_(gDatabaseList); 
		ASSERT(doomedDatabase->getRefCount() == 1);
		release_(doomedDatabase);
		
	} else {
		CSPath *path;
		bool create = false;
		uint32_t db_id;
		
		path = createDatabasePath(ms_my_get_mysql_home_path(), CSString::newString(db_name), &db_id, &create);
		
		if (path) {
			MSTransactionManager::dropDatabase(db_id);

			push_(path);
			doomedDatabasePath = path->getString();
			doomedDatabasePath->retain(); // Hold on to this path after the database has been released.
			release_(path);
		}
	}
	
	if (doomedDatabasePath)
		removeDatabasePath(doomedDatabasePath);
	
	exit_();
}

void MSDatabase::dropDatabase(const char *db_name ) 
{
	enter_();
	dropDatabase(getDatabase(db_name, false), db_name);
	exit_();
}

// The table_path can be several things here:
// 1: <absalute path>/<database>/<table>
// 2: <absalute path>/<database>
// 3: <database>/<table>
bool MSDatabase::convertTablePathToIDs(const char *table_path, uint32_t *db_id, uint32_t *tab_id, bool create) 
{
	const char	*base = ms_my_get_mysql_home_path();
	CSString	*table_url;
	CSString	*db_path = NULL;
	CSString	*db_name = NULL;
	CSString	*tab_name = NULL;
	MSDatabase	*db;
	enter_();
	
	*db_id = 0;
	*tab_id = 0;
	
	table_url = CSString::newString(table_path);
	if (table_url->startsWith(base)) {
		table_url = table_url->right(base);
	}
	push_(table_url);


	db_path = table_url->left("/", -1);
	push_(db_path);
	tab_name = table_url->right("/", -1);
	
	pop_(db_path);
	release_(table_url);

	if (db_path->length() == 0) { // Only a database name was supplied.
		db_path->release();
		db_name = tab_name;
		tab_name = NULL;
	} else {
		if (tab_name->length() == 0) {
			tab_name->release();
			tab_name = NULL;
		} else
			push_(tab_name);
		push_(db_path);
		db_name = db_path->right("/", -1);
		pop_(db_path);
		if (db_name->length() == 0) {
			db_name->release();
			db_name = db_path;
		} else {
			db_path->release();
			db_path = NULL;
		}
	}
	
	db = MSDatabase::getDatabase(db_name, create); // This will release db_name
	if (db) {
		*db_id = db->myDatabaseID;
		if (tab_name) {
			MSTable		*tab;
			pop_(tab_name);
			push_(db);
			tab = db->getTable(tab_name, create);// This will release tab_name
			pop_(db);
			if (tab) {
				*tab_id = tab->myTableID;
				tab->release();
			}
		}
			
		db->release();
	}
	
	return_((*tab_id > 0) && (*db_id > 0));
}

bool MSDatabase::convertTableAndDatabaseToIDs(const char *db_name, const char *tab_name, uint32_t *db_id, uint32_t *tab_id, bool create) 
{
	MSDatabase	*db;
	enter_();
	
	*db_id = 0;
	*tab_id = 0;
	
	db = MSDatabase::getDatabase(db_name, create); 
	if (db) {
		push_(db);
		*db_id = db->myDatabaseID;
		if (tab_name) {
			MSTable		*tab;
			tab = db->getTable(tab_name, create);
			if (tab) {
				*tab_id = tab->myTableID;
				tab->release();
			}
		}
			
		release_(db);
	}
	
	return_((*tab_id > 0) && (*db_id > 0));
}

MSDatabase *MSDatabase::loadDatabase(CSString *db_name, bool create)
{
	MSDatabase *db;
	enter_();
	
	db = newDatabase(ms_my_get_mysql_home_path(), db_name, 0, create);
	
	if (db) {
		push_(db);
		
		gDatabaseList->add(RETAIN(db));
		
		gDatabaseArray->set(db->myDatabaseID, RETAIN(db));
		db->startThreads();
		PBMSSystemTables::loadSystemTables(RETAIN(db));
			
		pop_(db);
	}
	return_(db);
}

uint32_t MSDatabase::getDatabaseID(CSString *db_name, bool create)
{
	MSDatabase *db;
	uint32_t id = 0;
	enter_();
	push_(db_name);
	
	
	lock_(gDatabaseList);
	if (!(db = (MSDatabase *) gDatabaseList->find(db_name))) {
		db = MSDatabase::loadDatabase(RETAIN(db_name), create);
		if (!db)
			goto exit;
		id = db->myDatabaseID;
		db->release();
	} else
		id = db->myDatabaseID;
	
	exit:
	unlock_(gDatabaseList);
	release_(db_name);
	return_(id);
}


uint32_t MSDatabase::getDatabaseID(const char *db_name, bool create)
{
	return getDatabaseID(CSString::newString(db_name), create);
}

MSDatabase *MSDatabase::getBackupDatabase(CSString *db_location, CSString *db_name, uint32_t db_id, bool create)
{
	bool was_created = create; 
	CSPath *path;
	MSDatabase *db;
	enter_();
	
	push_(db_location);
	push_(db_name);
	// If the db already exists and 'create' == true then the existing db
	// must be deleted.
	// Create the database path, if 'create' == false then it can return NULL
	path = createDatabasePath(db_location->getCString(), RETAIN(db_name), &db_id, &was_created);
	if (!path) {
		CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_DB, db_name->getCString());
	}
	push_(path);
	
	// If we wanted to create it but it already exists then throw an error.
	if ( create && !was_created) {
		char str[120];
		snprintf(str, 120, "Duplicate database: %s", db_name->getCString());
		CSException::throwException(CS_CONTEXT, MS_ERR_DUPLICATE_DB, str);
	}
		
	release_(path);
	pop_(db_name);
	// everything looks OK
	db = newDatabase(db_location->getCString(), db_name, db_id, create);
	db->setBackupDatabase();
	release_(db_location);
	return_(db);
}

