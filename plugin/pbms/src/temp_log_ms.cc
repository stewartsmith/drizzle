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
 * 2007-07-03
 *
 * H&G2JCtL
 *
 * Network interface.
 *
 */

#include "cslib/CSConfig.h"

#include <stddef.h>

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSStorage.h"

#include "temp_log_ms.h"
#include "open_table_ms.h"
#include "trans_log_ms.h"
#include "transaction_ms.h"
#include "parameters_ms.h"


// Search the transaction log for a MS_ReferenceTxn record for the given BLOB.
// Just search the log file and not the cache. Seaching the cache may be faster but
// it would require locks that could block the writers or reader threads and in the worse
// case it will still require the reading of the log anyway.
//
// This search doesn't distinguish between transactions that are still running and
// transactions that are rolled back.
class SearchTXNLog : ReadTXNLog {
	public:
	SearchTXNLog(uint32_t db_id, MSTrans *log): ReadTXNLog(log), st_db_id(db_id) {}
	
	bool	st_found;
	bool	st_terminated;
	bool	st_commited;
	uint32_t st_tid; 
	uint32_t st_db_id; 
	uint32_t st_tab_id; 
	uint64_t st_blob_id;
	
	virtual bool rl_CanContinue() { return ((!st_found) || !st_terminated);}
	virtual void rl_Load(uint64_t log_position, MSTransPtr rec) 
	{
		(void) log_position;
		
		if ( !st_found && (TRANS_TYPE(rec->tr_type) != MS_ReferenceTxn))
			return;
		
		if (!st_found) {
			if  ((rec->tr_db_id == st_db_id) && (rec->tr_tab_id == st_tab_id) && (rec->tr_blob_id == st_blob_id)) {
				st_found = true;
				st_tid = rec->tr_id;
			} else
				return;
		}
		st_terminated = TRANS_IS_TERMINATED(rec->tr_type);
		if (st_terminated)
			st_commited = (TRANS_IS_AUTOCOMMIT(rec->tr_type) || (TRANS_TYPE(rec->tr_type) == MS_CommitTxn));
	}
	
	bool st_FindBlobRef(bool *committed, uint32_t tab_id, uint64_t blob_id)
	{
		enter_();
		st_found = st_terminated = st_commited = false;
		st_tab_id = tab_id;
		st_blob_id = blob_id;	
		
		rl_ReadLog(rl_log->txn_GetStartPosition(), false);
		*committed = st_commited;
		return_(st_found);
	}
};

MSTempLogFile::MSTempLogFile():
CSReadBufferedFile(),
myTempLogID(0),
myTempLog(NULL)
{
}

MSTempLogFile::~MSTempLogFile()
{
	close();
	if (myTempLog)
		myTempLog->release();
}

MSTempLogFile *MSTempLogFile::newTempLogFile(uint32_t id, MSTempLog *temp_log, CSFile *file)
{
	MSTempLogFile *f;
	enter_();
	
	push_(temp_log);
	push_(file);
	
	if (!(f = new MSTempLogFile())) 
		CSException::throwOSError(CS_CONTEXT, ENOMEM);

	f->myTempLogID = id;
	
	pop_(file);
	f->setFile(file);
	
	pop_(temp_log);
	f->myTempLog = temp_log;
	return_(f);
}

MSTempLog::MSTempLog(uint32_t id, MSDatabase *db, off64_t file_size):
CSRefObject(),
myLogID(id),
myTempLogSize(file_size),
myTemplogRecSize(0),
myTempLogHeadSize(0),
iLogDatabase(db),
iDeleteLog(false)
{
}

MSTempLog::~MSTempLog()
{
	enter_();
	if (iDeleteLog) {
		CSPath *path;

		path = getLogPath();
		push_(path);
		path->removeFile();
		release_(path);
	}
	exit_();
}

void MSTempLog::deleteLog()
{
	iDeleteLog = true;
}

CSPath *MSTempLog::getLogPath()
{
	char file_name[120];

	cs_strcpy(120, file_name, "bs-logs");
	cs_add_dir_char(120, file_name);
	cs_strcat(120, file_name, "temp-");
	cs_strcat(120, file_name, myLogID);
	cs_strcat(120, file_name, ".bs");
	return CSPath::newPath(RETAIN(iLogDatabase->myDatabasePath), file_name);
}

MSTempLogFile *MSTempLog::openTempLog()
{
	CSPath			*path;
	MSTempLogFile	*fh;

	enter_();
	path = getLogPath();
	retain();
	fh = MSTempLogFile::newTempLogFile(myLogID, this, CSFile::newFile(path));
	push_(fh);
	if (myTempLogSize)
		fh->open(CSFile::DEFAULT);
	else
		fh->open(CSFile::CREATE);
	if (!myTempLogHeadSize) {
		MSTempLogHeadRec	head;

		lock_(iLogDatabase->myTempLogArray);
		/* Check again after locking: */
		if (!myTempLogHeadSize) {
			size_t rem;

			if (fh->read(&head, 0, offsetof(MSTempLogHeadRec, th_reserved_4), 0) < offsetof(MSTempLogHeadRec, th_reserved_4)) {
				CS_SET_DISK_4(head.th_magic_4, MS_TEMP_LOG_MAGIC);
				CS_SET_DISK_2(head.th_version_2, MS_TEMP_LOG_VERSION);
				CS_SET_DISK_2(head.th_head_size_2, MS_TEMP_LOG_HEAD_SIZE);
				CS_SET_DISK_2(head.th_rec_size_2, sizeof(MSTempLogItemRec));
				CS_SET_DISK_4(head.th_reserved_4, 0);
				fh->write(&head, 0, sizeof(MSTempLogHeadRec));
				fh->flush();
			}
			
			/* Check the file header: */
			if (CS_GET_DISK_4(head.th_magic_4) != MS_TEMP_LOG_MAGIC)
				CSException::throwFileError(CS_CONTEXT, fh->getPathString(), CS_ERR_BAD_HEADER_MAGIC);
			if (CS_GET_DISK_2(head.th_version_2) > MS_TEMP_LOG_VERSION)
				CSException::throwFileError(CS_CONTEXT, fh->getPathString(), CS_ERR_VERSION_TOO_NEW);

			/* Load the header details: */
			myTempLogHeadSize = CS_GET_DISK_2(head.th_head_size_2);
			myTemplogRecSize = CS_GET_DISK_2(head.th_rec_size_2);

			/* File size, cannot be less than header size, adjust to correct offset: */
			if (myTempLogSize < myTempLogHeadSize)
				myTempLogSize = myTempLogHeadSize;
			if ((rem = (myTempLogSize - myTempLogHeadSize) % myTemplogRecSize))
				myTempLogSize += myTemplogRecSize - rem;
		}
		unlock_(iLogDatabase->myTempLogArray);
	}
	pop_(fh);
	return_(fh);
}

time_t MSTempLog::adjustWaitTime(time_t then, time_t now)
{
	time_t wait;

	if (now < (time_t)(then + PBMSParameters::getTempBlobTimeout())) {
		wait = ((then + PBMSParameters::getTempBlobTimeout() - now) * 1000);
		if (wait < 2000)
			wait = 2000;
		else if (wait > 120 * 1000)
			wait = 120 * 1000;
	}
	else
		wait = 1;
			
	return wait;
}

/*
 * ---------------------------------------------------------------
 * TEMP LOG THREAD
 */

MSTempLogThread::MSTempLogThread(time_t wait_time, MSDatabase *db):
CSDaemon(wait_time, NULL),
iTempLogDatabase(db),
iTempLogFile(NULL),
iLogRecSize(0),
iLogOffset(0)
{
}


void MSTempLogThread::close()
{
	if (iTempLogFile) {
		iTempLogFile->release();
		iTempLogFile = NULL;
	}
}

bool MSTempLogThread::try_ReleaseBLOBReference(CSThread *self, CSStringBuffer *buffer, uint32_t tab_id, int type, uint64_t blob_id, uint32_t auth_code)
{
	volatile bool rtc = true;
	try_(a) {
		/* Release the BLOB reference. */
		MSOpenTable *otab;

		if (type == MS_TL_REPO_REF) {
			MSRepoFile	*repo_file;

			if ((repo_file = iTempLogDatabase->getRepoFileFromPool(tab_id, true))) {
				frompool_(repo_file);
				repo_file->checkBlob(buffer, blob_id, auth_code, iTempLogFile->myTempLogID, iLogOffset);
				backtopool_(repo_file);
			}
		}
		else {
			if ((otab = MSTableList::getOpenTableByID(iTempLogDatabase->myDatabaseID, tab_id))) {
				frompool_(otab);
				if (type == MS_TL_BLOB_REF) {
					otab->checkBlob(buffer, blob_id, auth_code, iTempLogFile->myTempLogID, iLogOffset);
					backtopool_(otab);
				}
				else {
					ASSERT(type == MS_TL_TABLE_REF);
					if ((type == MS_TL_TABLE_REF) && otab->deleteReferences(iTempLogFile->myTempLogID, iLogOffset, &myMustQuit)) {
						/* Delete the file now... */
						MSTable			*tab;
						CSPath			*from_path;
						MSOpenTablePool *tab_pool;

						tab = otab->getDBTable();
						from_path = otab->getDBTable()->getTableFile();

						pop_(otab);

						push_(from_path);
						tab->retain();
						push_(tab);

						tab_pool = MSTableList::lockTablePoolForDeletion(otab); // This returns otab to the pool.
						frompool_(tab_pool);

						from_path->removeFile();
						tab->myDatabase->removeTable(tab);

						backtopool_(tab_pool); // The will unlock and close the table pool freeing all tables in it.
						pop_(tab);				// Returning the pool will have released this. (YUK!)
						release_(from_path);
					}
					else 
						backtopool_(otab);
				}
			}
		}
		
		rtc = false;
	}
	
	catch_(a);
	cont_(a);
	return rtc;
}

bool MSTempLogThread::doWork()
{
	size_t				tfer;
	MSTempLogItemRec	log_item;
	CSStringBuffer		*buffer;
	SearchTXNLog		txn_log(iTempLogDatabase->myDatabaseID, MSTransactionManager::tm_Log);

	enter_();
	new_(buffer, CSStringBuffer(20));
	push_(buffer);
	while (!myMustQuit) {
		if (!iTempLogFile) {
			size_t head_size;
			if (!(iTempLogFile = iTempLogDatabase->openTempLogFile(0, &iLogRecSize, &head_size))) {
				release_(buffer);
				return_(true);
			}
			iLogOffset = head_size;
		}

		tfer = iTempLogFile->read(&log_item, iLogOffset, sizeof(MSTempLogItemRec), 0);
		if (tfer == 0) {
			/* No more data to be read: */

			/* Check to see if there is a log after this: */
			if (iTempLogDatabase->getTempLogCount() <= 1) {
				/* The next log does not yet exist. We wait for
				 * it to be created before we delete and
				 * close the current log.
				 */
				myWaitTime = PBMSParameters::getTempBlobTimeout() * 1000;
				break;
			}

			iTempLogFile->myTempLog->deleteLog();
			iTempLogDatabase->removeTempLog(iTempLogFile->myTempLogID);
			close();
		}
		else if (tfer == sizeof(MSTempLogItemRec)) {
			/* We have a record: */
			int		type;
			uint32_t tab_id;
			uint64_t blob_id= 0;
			uint32_t auth_code;
			uint32_t then;
			time_t	now;

			/*
			 * Items in the temp log are never updated.
			 * If a temp operation is canceled then the object 
			 * records this itself and when the temp operation 
			 * is attempted it will recognize by the templog
			 * id and offset that it is no longer a valid 
			 * operation.
			 */
			tab_id = CS_GET_DISK_4(log_item.ti_table_id_4);
				
			type = CS_GET_DISK_1(log_item.ti_type_1);
			blob_id = CS_GET_DISK_6(log_item.ti_blob_id_6);
			auth_code = CS_GET_DISK_4(log_item.ti_auth_code_4);
			then = CS_GET_DISK_4(log_item.ti_time_4);

			now = time(NULL);
			if (now < (time_t)(then + PBMSParameters::getTempBlobTimeout())) {
				/* Time has not yet exired, adjust wait time: */
				myWaitTime = MSTempLog::adjustWaitTime(then, now);
				break;
			}
		
			if (try_ReleaseBLOBReference(self, buffer, tab_id, type, blob_id, auth_code)) {
				int err = self->myException.getErrorCode();
				
				if (err == MS_ERR_TABLE_LOCKED) {
					throw_();
				}
				else if (err == MS_ERR_REMOVING_REPO) {
					/* Wait for the compactor to finish: */
					myWaitTime = 2 * 1000;
					release_(buffer);
					return_(true);
				}
				else if ((err == MS_ERR_UNKNOWN_TABLE) || (err == MS_ERR_DATABASE_DELETED))
					;
				else
					self->myException.log(NULL);
			}

		}
		else {
			// Only part of the data read, don't wait very long to try again:
			myWaitTime = 2 * 1000;
			break;
		}
		iLogOffset += iLogRecSize;
	}

	release_(buffer);
	return_(true);
}

void *MSTempLogThread::completeWork()
{
	close();
	return NULL;
}

