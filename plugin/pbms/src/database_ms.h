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
 * 2007-06-26
 *
 * H&G2JCtL
 *
 * Contains all the information about an open database.
 *
 */

#pragma once
#ifndef __DATABASE_MS_H__
#define __DATABASE_MS_H__

#include "cslib/CSDefs.h"
#include "cslib/CSStorage.h"
#include "cslib/CSStrUtil.h"

#include "table_ms.h"
#include "repository_ms.h"
#ifdef HAVE_ALIAS_SUPPORT
#include "alias_ms.h"
#endif
#include "temp_log_ms.h"
#include "compactor_ms.h"
#include "cloud_ms.h"

class MSOpenTable;
class MSBackup;

class MSDatabase : public CSSharedRefObject {
public:
	bool				myIsPBMS;
	uint32_t				myDatabaseID;
	CSString			*myDatabaseName;	// The database name may not be related to the database path,
	CSString			*myDatabasePath;	// do not make any assumptions here!
	CSSyncSparseArray	*myTempLogArray;
	MSCompactorThread	*myCompactorThread;
	MSTempLogThread		*myTempLogThread;
	CSSyncVector		*myRepostoryList;
	CloudDB				*myBlobCloud;
	uint8_t				myBlobType;			// Cloud or repository



	MSDatabase();
	virtual ~MSDatabase();

	const char *getDatabaseNameCString();
	
	MSTable *getTable(CSString *tab_name, bool create);
	MSTable *getTable(const char *tab_name, bool create);
	MSTable *getTable(uint32_t tab_id, bool missing_ok);
	MSTable *getNextTable(uint32_t *pos);

	void addTable(uint32_t tab_id, const char *tab_name, off64_t file_size, bool to_delete);
	void addTableFromFile(CSDirectory *dir, const char *file_name, bool to_delete);
	void removeTable(MSTable *tab);
	void dropTable(MSTable *tab);
	void renameTable(MSTable *tab, const char *to_name);
	CSString *getATableName();
	uint32_t getTableCount();

	void openWriteRepo(MSOpenTable *otab);

	MSRepository *getRepoFullOfTrash(time_t *wait_time);
	MSRepository *lockRepo(off64_t size);
	void removeRepo(uint32_t repo_id, bool *mustQuit);

	MSRepoFile *getRepoFileFromPool(uint32_t repo_id, bool missing_ok);
	void returnRepoFileToPool(MSRepoFile *file);
	
	uint64_t newBlobRefId() // Returns a unique blob referfence Id.
	{
		uint64_t id;
		enter_();
		lock_(&iBlobRefIdLock);
		id = iNextBlobRefId++;
		unlock_(&iBlobRefIdLock);
		return_(COMMIT_MASK(id));
	}
	

private:
	void queueTempLogEvent(MSOpenTable *otab, int type, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code, uint32_t *log_id, uint32_t *log_offset, uint32_t *q_time);
public:
#ifdef HAVE_ALIAS_SUPPORT
	void queueForDeletion(MSOpenTable *otab, int type, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code, uint32_t *log_id, uint32_t *log_offset, uint32_t *q_time, MSDiskAliasPtr aliasDiskRec);
#else
	void queueForDeletion(MSOpenTable *otab, int type, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code, uint32_t *log_id, uint32_t *log_offset, uint32_t *q_time)
	{
		queueTempLogEvent(otab, type, tab_id, blob_id, auth_code, log_id, log_offset, q_time);
	}
#endif
	MSTempLogFile *openTempLogFile(uint32_t log_id, size_t *log_rec_size, size_t *log_head_size);
	uint32_t getTempLogCount();
	void removeTempLog(uint32_t log_id);

	/* Make this object sortable: */
	virtual CSObject *getKey();
	virtual int compareKey(CSObject *);

	MSCompactorThread *getCompactorThread();
	CSSyncVector *getRepositoryList();

#ifdef HAVE_ALIAS_SUPPORT
	bool findBlobWithAlias(const char *alias, uint32_t *repo_id = NULL, uint64_t *repo_offset = NULL)
	{
		bool found;
		uint32_t x_repo_id;
		uint64_t x_repo_offset;
		bool referenced; // The BLOB can be referenced or non referenced.
		
		enter_();
		if (!repo_id) repo_id = &x_repo_id;
		if (!repo_offset) repo_offset = &x_repo_offset;
		
		lock_(&iBlobAliaseLock);
		found = iBlobAliases->findBlobByAlias(alias, &referenced, repo_id, repo_offset);
		unlock_(&iBlobAliaseLock);
		return_(found);
	}
	uint32_t registerBlobAlias(uint32_t repo_id, uint64_t repo_offset, const char *alias);
	uint32_t updateBlobAlias(uint32_t repo_id, uint64_t repo_offset, uint32_t old_alias_hash, const char *alias);
	void deleteBlobAlias(MSDiskAliasPtr diskRec);
	void deleteBlobAlias(uint32_t repo_id, uint64_t repo_offset, uint32_t alias_hash);
	void moveBlobAlias(uint32_t old_repo_id, uint64_t old_repo_offset, uint32_t alias_hash, uint32_t new_repo_id, uint64_t new_repo_offset);
#endif

	bool isValidHeaderField(const char *name);
	
	bool isRecovering() { return iRecovering;} // Indicates the database is being recovered from a dump.
	void setRecovering(bool recovering) { // Indicate if the database is being recovered from a dump.
		if (iRecovering == recovering)
			return;
		iRecovering = recovering;
		if (iRecovering) {
			myCompactorThread->suspend();
			myTempLogThread->suspend();
		} else {
			myCompactorThread->resume();
			myTempLogThread->resume();
		}
	}
	
	bool isBackup;
	void setBackupDatabase(); // Signals the database that it is the destination for a backup process.
	void releaseBackupDatabase(); 
	
	void startBackup(MSBackupInfo *backup_info);
	void terminateBackup();
	bool backupStatus(uint64_t *total, uint64_t *completed, bool *completed_ok);
	uint32_t backupID();
	
private:
	MSBackup			*iBackupThread;
	uint32_t				iBackupTime; // The time at which the backup was started.
	bool				iRecovering;
#ifdef HAVE_ALIAS_SUPPORT
	MSAlias				*iBlobAliases;
	CSLock				iBlobAliaseLock; // Write lock for the BLOB aliases index. This is required because of the .
#endif
	bool				iClosing;

	CSSyncSortedList	*iTableList;
	CSSparseArray		*iTableArray;
	uint32_t				iMaxTableID;

	MSTempLog			*iWriteTempLog;
	bool				iDropping;
	void				dropDatabase();
	void				startThreads();

	CSLock				iBlobRefIdLock; // Lock for the BLOB ref counter.
	uint64_t				iNextBlobRefId;
	
public:

	CSSyncSortedList	iHTTPMetaDataHeaders;
	static void startUp(const char *default_http_headers);
	static void stopThreads();
	static void shutDown();

	static MSDatabase *getBackupDatabase(CSString *db_location, CSString *db_name, uint32_t db_id, bool create);
	
	static MSDatabase *getDatabase(CSString *db_name, bool create);
	static MSDatabase *getDatabase(const char *db_name, bool create);
	static MSDatabase *getDatabase(uint32_t db_id, bool missing_ok = false);
	static uint32_t getDatabaseID(CSString *db_name, bool create);
	static uint32_t getDatabaseID(const char *db_name, bool create);

	static void wakeTempLogThreads();
	static void dropDatabase(MSDatabase *doomedDatabase, const char *db_name = NULL);
	static void dropDatabase(const char *db_name);
	
	static bool convertTablePathToIDs(const char *path, uint32_t *db_id, uint32_t *tab_id, bool create);
	static bool convertTableAndDatabaseToIDs(const char *db_name, const char *tab_name, uint32_t *db_id, uint32_t *tab_id, bool create);


private:
	static CSSyncSortedList	*gDatabaseList;
	static CSSparseArray	*gDatabaseArray;
	
	
	static void removeDatabasePath(CSString *doomedDatabasePath);

	static uint32_t getDBID(CSPath *path, CSString *db_name);
	static CSPath *createDatabasePath(const char *location, CSString *db_name, uint32_t *db_id_ptr, bool *create, bool is_pbms = false);
	static MSDatabase *newDatabase(const char *db_location, CSString *db_name, uint32_t	db_id, bool create);
	static MSDatabase *loadDatabase(CSString *db_name,  bool create);
	static uint32_t fileToTableId(const char *file_name, const char *name_part = NULL);
	const char *fileToTableName(size_t size, char *tab_name, const char *file_name);

};

#endif
