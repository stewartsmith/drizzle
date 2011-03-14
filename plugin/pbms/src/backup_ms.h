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
 */

#pragma once
#ifndef _BACKUP_MS_H_
#define _BACKUP_MS_H_

#include <inttypes.h>

class MSDatabase;

class MSBackupInfo : public CSRefObject {
	friend class StartDumpCleanUp;
	friend class InsertRowCleanUp;
	
	private:
	static uint32_t	gMaxInfoRef;
	static CSSyncSparseArray *gBackupInfo;

	friend class MSBackupTable;
	friend class MSBackup;
	
	private:	
	uint32_t			backupRefId;
	CSString		*db_name;
	uint32_t			db_id;
	time_t			startTime;
	time_t			completionTime;
	bool			dump;
	bool			isRunning;
	CSString		*backupLocation;
	uint32_t			cloudRef;
	uint32_t			cloudBackupNo;
	
public:

	static void startUp()
	{
		new_(gBackupInfo, CSSyncSparseArray(5));
		gMaxInfoRef = 0;
	}
	
	static void shutDown()
	{
		if (gBackupInfo) {
			gBackupInfo->clear();
			gBackupInfo->release();
			gBackupInfo = NULL;
		}	
	}


	static MSBackupInfo *findBackupInfo(uint32_t in_backupRefId)
	{
		MSBackupInfo *info;
		enter_();
		
		lock_(gBackupInfo);
		
		info = (MSBackupInfo *) gBackupInfo->get(in_backupRefId);
		if (info) 
			info->retain();
		unlock_(gBackupInfo);
		return_(info);
	}
	
	static MSBackupInfo *getBackupInfo(uint32_t in_backupRefId)
	{
		MSBackupInfo *info = findBackupInfo(in_backupRefId);
		if (!info) {
			enter_();
			char msg[80];
			snprintf(msg, 80, "Backup info with reference ID %"PRIu32" not found", in_backupRefId);
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, msg);
			outer_();
		}
		return info;
	}
	
	
	MSBackupInfo(uint32_t id, const char *name, uint32_t db_id, time_t start, time_t end, bool isDump, const char *location, uint32_t cloudRef_arg, uint32_t cloudBackupNo_arg );
	~MSBackupInfo();
	
	uint32_t getBackupRefId() { return backupRefId;}
	
	const char *getName(){ return db_name->getCString(); }
	
	uint32_t getDatabaseId() { return db_id;}
	
	time_t getStart(){ return startTime;}
	
	time_t getEnd(){ return completionTime;}
	
	bool isDump(){ return dump;}
	
	bool isBackupRunning(){ return isRunning;}

	const char *getLocation() { return (backupLocation)?backupLocation->getCString():NULL; }
		
	uint32_t getcloudRef(){ return cloudRef;}
	void setcloudRef(uint32_t no){ cloudRef = no;}

	uint32_t getcloudBackupNo(){ return cloudBackupNo;}
	void setcloudBackupNo(uint32_t no){ cloudBackupNo = no;}
	
	static MSBackupInfo *startDump(MSDatabase *db, uint32_t cloud_ref, uint32_t backup_no);

	void startBackup(MSDatabase *pbms_db);
	void backupCompleted(MSDatabase *db);
	void backupTerminated(MSDatabase *db);
};


class MSDatabase;
class MSOpenSystemTable;

class MSBackup :public CSDaemon {

public:

	MSBackup();
	~MSBackup(){} // Do nothing here because 'self' will no longer be valid, use completeWork().
	
	virtual bool doWork();

	virtual void *completeWork();
	
	void startBackup(MSDatabase *src_db);
	uint64_t getBackupSize() { return bu_size;}
	uint64_t getBackupCompletedSize() { return bu_completed;}
	bool	isRunning() { return bu_BackupRunning;}
	int		getStatus() { return (bu_BackupRunning)?0:bu_State;}
	uint32_t backupID() { return bu_ID;}
	
	static MSBackup* newMSBackup(MSBackupInfo *backup_info);

	friend class StartBackupCleanUp;
private:
	void completeBackup();
	
	MSBackupInfo *bu_info;

	CSVector	*bu_BackupList;
	CSDaemon	*bu_Compactor;
	bool		bu_BackupRunning;
	enum		{BU_RUNNING = -1, BU_COMPLETED = 0, BU_TERMINATED = 1}	bu_State; 
	
	MSDatabase	*bu_SourceDatabase;	// The source database.
	MSDatabase	*bu_Database;		// The destination database.
	MSOpenSystemTable *bu_dst_dump;	// The source database's pbms_dump.
	MSOpenSystemTable *bu_src_dump;	// The source database's pbms_dump.
	uint64_t		bu_size;			// The total size of the data to be backed up.
	uint64_t		bu_completed;		// The amount of data that has been backed up so far.
	
	uint32_t		bu_ID;
	uint32_t		bu_start_time;
	
	bool		bu_TransactionManagerSuspended;
};

#endif // _BACKUP_MS_H_
