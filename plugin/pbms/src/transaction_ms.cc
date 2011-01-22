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
 * 2009-07-09
 *
 * H&G2JCtL
 *
 * PBMS transaction daemon.
 *
 *
 */

#include "cslib/CSConfig.h"

#include <inttypes.h>

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSLog.h"

#include "mysql_ms.h"
#include "open_table_ms.h"
#include "trans_log_ms.h"
#include "transaction_ms.h"
#include "pbmsdaemon_ms.h"

/*
 * The pbms_ functions are utility functions supplied by ha_pbms.cc
 */
void	pbms_take_part_in_transaction(void *thread);

MSTrans *MSTransactionManager::tm_Log;
MSTransactionThread *MSTransactionManager::tm_Reader;


typedef  struct {
	CSDiskValue4	lr_time_4;		// The database ID for the operation.
	CSDiskValue1	lr_state_1;		// The transaction state. 
	CSDiskValue1	lr_type_1;		// The transaction type. If the first bit is set then the transaction is an autocommit.
	CSDiskValue4	lr_db_id_4;		// The database ID for the operation.
	CSDiskValue4	lr_tab_id_4;	// The table ID for the operation.
	CSDiskValue8	lr_blob_id_8;	// The blob ID for the operation.
	CSDiskValue8	lr_blob_ref_id_8;// The blob reference id.
} MSDiskLostRec, *MSDiskLostPtr;

/*
 * ---------------------------------------------------------------
 * The transaction reader thread 
 */

class MSTransactionThread : public CSDaemon {
public:
	MSTransactionThread(MSTrans *txn_log);
	
	virtual ~MSTransactionThread(){} // Do nothing here because 'self' will no longer be valid, use completeWork().


	void close();

	virtual bool doWork();

	virtual void *completeWork();
	
	void flush();
	
	bool trt_is_ready;
private:
	void reportLostReference(MSTransPtr rec, MS_TxnState state);
	void dereference(MSTransPtr rec, MS_TxnState state);
	void commitReference(MSTransPtr rec, MS_TxnState state);
	
	MSTrans *trt_log;
	CSFile	*trt_lostLog;

};

MSTransactionThread::MSTransactionThread(MSTrans *txn_log):
CSDaemon(0, NULL),
trt_is_ready(false),
trt_log(txn_log),
trt_lostLog(NULL)
{
	trt_log->txn_SetReader(this);
}

void MSTransactionThread::close()
{
	if (trt_lostLog)
		trt_lostLog->close();
}

void MSTransactionThread::reportLostReference(MSTransPtr rec, MS_TxnState state)
{
	MSDiskLostRec lrec;
	const char *t_txt, *s_txt;
	char b1[16], b2[16], msg[100];
	MSDatabase *db;
	MSTable *tab;
	
	//if (PBMSDaemon::isDaemonState(PBMSDaemon::DaemonStartUp) == true)
		//return;

	enter_();
	// Do not report errors caused by missing databases or tables.
	// This can happen if the transaction log is reread after a crash
	// and transactions are found that belonged to dropped databases
	// or tables.
	db = MSDatabase::getDatabase(rec->tr_db_id, true);
	if (!db)
		goto dont_worry_about_it;
		
	push_(db);
	tab = db->getTable(rec->tr_tab_id, true);
	release_(db);
	if (!tab)
		goto dont_worry_about_it;
	tab->release();
	
	switch (state) {
		case MS_Committed:
			s_txt = "Commit";
			break;
		case MS_RolledBack:
			s_txt = "RolledBack";
			break;
		case MS_Recovered:
			s_txt = "Recovered";
			break;
		case MS_Running:
			s_txt = "Running";
			break;
		default:
			snprintf(b1, 16, "(%d)?", state);
			s_txt = b1;
	}

	switch (TRANS_TYPE(rec->tr_type)) {
		case MS_DereferenceTxn:
			t_txt = "Dereference";
			break;
		case MS_ReferenceTxn:
			t_txt = "Reference";
			break;
		default:
			snprintf(b2, 16, "(%x)?", rec->tr_type);
			t_txt = b2;
	}

	snprintf(msg, 100, "Lost PBMS record: %s %s db_id: %"PRIu32" tab_id: %"PRIu32" blob_id: %"PRIu64"", s_txt, t_txt, rec->tr_db_id, rec->tr_tab_id, rec->tr_blob_id);
	CSL.logLine(self, CSLog::Warning, msg);

	CS_SET_DISK_4(lrec.lr_time_4, time(NULL));
	CS_SET_DISK_1(lrec.lr_state_1, state);
	CS_SET_DISK_1(lrec.lr_type_1, rec->tr_type);
	CS_SET_DISK_4(lrec.lr_db_id_4, rec->tr_db_id);
	CS_SET_DISK_4(lrec.lr_tab_id_4, rec->tr_tab_id);
	CS_SET_DISK_8(lrec.lr_blob_id_8, rec->tr_blob_id);
	CS_SET_DISK_8(lrec.lr_blob_ref_id_8, rec->tr_blob_ref_id);
	
	if (!trt_lostLog) {
		CSPath *path;
		char *str = cs_strdup(trt_log->txn_GetTXNLogPath());
		cs_remove_last_name_of_path(str);
		
		path = CSPath::newPath(str, "pbms_lost_txn.dat");
		cs_free(str);
		
		trt_lostLog = CSFile::newFile(path);
		trt_lostLog->open(CSFile::CREATE);
	}
	trt_lostLog->write(&lrec, trt_lostLog->getEOF(), sizeof(MSDiskLostRec));
	trt_lostLog->sync();
	
dont_worry_about_it:
	exit_();
	
}

void MSTransactionThread::dereference(MSTransPtr rec, MS_TxnState state)
{
	enter_();
	
	try_(a) {
		MSOpenTable		*otab;
		otab = MSTableList::getOpenTableByID(rec->tr_db_id, rec->tr_tab_id);
		frompool_(otab);
		otab->freeReference(rec->tr_blob_id, rec->tr_blob_ref_id);
		backtopool_(otab);
	}
	
	catch_(a) {
		reportLostReference(rec, state);
	}
	
	cont_(a);	
	exit_();
}

void MSTransactionThread::commitReference(MSTransPtr rec, MS_TxnState state)
{
	enter_();
	
	try_(a) {
		MSOpenTable		*otab;
		otab = MSTableList::getOpenTableByID(rec->tr_db_id, rec->tr_tab_id);
		frompool_(otab);
		otab->commitReference(rec->tr_blob_id, rec->tr_blob_ref_id);
		backtopool_(otab);
	}
	
	catch_(a) {
		reportLostReference(rec, state);
	}
	
	cont_(a);	
	exit_();
}

void MSTransactionThread::flush()
{
	enter_();
	
	// For now I just wait until the transaction queue is empty or
	// the transaction at the head of the queue has not yet been
	// committed.
	//
	// What needs to be done is for the transaction log to scan 
	// past the non commited transaction to see if there are any
	// other committed transaction in the log and apply them if found.
	
	wakeup(); // Incase the reader is sleeping.
	while (trt_log->txn_haveNextTransaction() && !isSuspend() && self->myMustQuit)
		self->sleep(10);		
	exit_();
}

bool MSTransactionThread::doWork()
{
	enter_();
	
	try_(a) {
		MSTransRec rec = {0,0,0,0,0,0,0};
		MS_TxnState state;
		while (!myMustQuit) {
			// This will sleep while waiting for the next 
			// completed transaction.
			trt_log->txn_GetNextTransaction(&rec, &state); 
			if (myMustQuit)
				break;
				
			if (rec.tr_db_id == 0) // The database was dropped.
				continue;
				
			if (state == MS_Committed){
				if (TRANS_TYPE(rec.tr_type) == MS_DereferenceTxn) 
					dereference(&rec, state);
				else if (TRANS_TYPE(rec.tr_type) == MS_ReferenceTxn)  
					commitReference(&rec, state);

			} else if (state == MS_RolledBack) { 
				// There is nothing to do on rollback of a dereference.
				
				if (TRANS_TYPE(rec.tr_type) == MS_ReferenceTxn)  
					dereference(&rec, state);
					
			} else if (state == MS_Recovered) { 
				if ((TRANS_TYPE(rec.tr_type) == MS_ReferenceTxn) || (TRANS_TYPE(rec.tr_type) == MS_DereferenceTxn))
					reportLostReference(&rec, state); // Report these even though they may not be lost.
				
				// Because of the 2 phase commit issue with other engines I cannot
				// just roll back the transaction because it may have been committed 
				// on the master engine. So to be safe I will always err on the side
				// of having unreference BLOBs in the repository rather than risking
				// deleting a BLOB that was referenced. To this end I will commit references
				// while ignoring (rolling back) dereferences.
				if (TRANS_TYPE(rec.tr_type) == MS_ReferenceTxn)  
					commitReference(&rec, state);
				
			}
		}
	}
	
	catch_(a) {
		self->logException();
		CSL.logLine(NULL, CSLog::Error, "!!!!!!!! THE PBMS TRANSACTION LOG READER DIED! !!!!!!!!!!!");
	}
	cont_(a);
	return_(true);
}

void *MSTransactionThread::completeWork()
{
	close();
	
	if (trt_log)
		trt_log->release();
		
	if (trt_lostLog)
		trt_lostLog->release();
	return NULL;
}

/*
 * ---------------------------------------------------------------
 * The transaction log manager 
 */
void MSTransactionManager::startUpReader()
{
	char pbms_path[PATH_MAX];
	enter_();
	
	cs_strcpy(PATH_MAX, pbms_path, PBMSDaemon::getPBMSDir()); 
	cs_add_name_to_path(PATH_MAX, pbms_path, "ms-trans-log.dat");
	
	tm_Log = MSTrans::txn_NewMSTrans(pbms_path);
	new_(tm_Reader, MSTransactionThread(RETAIN(tm_Log)));

	tm_Reader->start();
	
	// Wait for the transaction reader to recover any old transaction:
	tm_Reader->flush();
		
	exit_();
}

void MSTransactionManager::startUp()
{
	CSPath *path = NULL;
	enter_();
	
	// Do not start the reader if the pbms dir doesn't exist.
	path = CSPath::newPath(PBMSDaemon::getPBMSDir());
	push_(path);
	if (path->exists()) {
		startUpReader();
	}
	release_(path);
	
	exit_();
}

void MSTransactionManager::shutDown()
{
	if (tm_Reader) {
		tm_Reader->stop();
		tm_Reader->release();
		tm_Reader = NULL;
	}
	if (tm_Log) {
		tm_Log->release();
		tm_Log = NULL;
	}
}

void MSTransactionManager::flush()
{
	if (tm_Reader) 
		tm_Reader->flush();
}

void MSTransactionManager::suspend(bool do_flush)
{
	enter_();
	
	if (do_flush) 
		flush();
		
	if (tm_Reader) {
		tm_Reader->suspend();
	}	
	exit_();
}

void MSTransactionManager::resume()
{
	enter_();
	if (tm_Reader) {
		tm_Reader->resume();
	}	
	exit_();
}

void MSTransactionManager::commit()
{
	enter_();
	
	if (!tm_Log)
		startUpReader();
		
	self->myStmtCount = 0;
	self->myStartStmt = 0;

	tm_Log->txn_LogTransaction(MS_CommitTxn);
	

	exit_();
}

void MSTransactionManager::rollback()
{
	enter_();
	
	if (!tm_Log)
		startUpReader();
		
	self->myStmtCount = 0;
	self->myStartStmt = 0;

	tm_Log->txn_LogTransaction(MS_RollBackTxn);

	exit_();
}

class MSTransactionCheckPoint: public CSString
{
	public:
	MSTransactionCheckPoint(const char *name, uint32_t stmtCount ):CSString(name)
	{
		position = stmtCount;
	}
	
	uint32_t position;
};

#ifdef DRIZZLED
void MSTransactionManager::setSavepoint(const char *savePoint)
{
	MSTransactionCheckPoint *checkPoint;
	enter_();
	
	new_(checkPoint, MSTransactionCheckPoint(savePoint, self->myStmtCount));
	
	push_(checkPoint);
	self->mySavePoints.add(checkPoint);
	pop_(checkPoint);
	
	exit_();
}

void MSTransactionManager::releaseSavepoint(const char *savePoint)
{
	MSTransactionCheckPoint *checkPoint;
	CSString *name;
	enter_();
	
	name = CSString::newString(savePoint);
	push_(name);

	checkPoint = (MSTransactionCheckPoint*) self->mySavePoints.find(name);
	release_(name);
	
	if (checkPoint) 		
		self->mySavePoints.remove(checkPoint);
		
	exit_();
}

void MSTransactionManager::rollbackTo(const char *savePoint)
{
	MSTransactionCheckPoint *checkPoint;
	CSString *name;
	enter_();
	
	name = CSString::newString(savePoint);
	push_(name);

	checkPoint = (MSTransactionCheckPoint*) self->mySavePoints.find(name);
	release_(name);
	
	if (checkPoint) {
		uint32_t position = checkPoint->position;
		
		self->mySavePoints.remove(checkPoint);
		rollbackToPosition(position);
	}
		
	exit_();
}
#endif

void MSTransactionManager::rollbackToPosition(uint32_t position)
{
	enter_();

	ASSERT(self->myStmtCount > position);
	
	if (!tm_Log)
		startUpReader();
	tm_Log->txn_LogPartialRollBack(position);
	
	exit_();
}

void MSTransactionManager::dropDatabase(uint32_t db_id)
{
	enter_();

	if (!tm_Log)
		startUpReader();
	
	tm_Log->txn_dropDatabase(db_id);

	exit_();
}

void MSTransactionManager::logTransaction(bool ref, uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id)
{
	enter_();
	
	if (!tm_Log)
		startUpReader();

	if (!self->myTID) {
		bool autocommit = false;
		autocommit = ms_is_autocommit();
#ifndef DRIZZLED
		if (!autocommit)
			pbms_take_part_in_transaction(ms_my_get_thread());
#endif
			
		self->myIsAutoCommit = autocommit;
	}
	
	// PBMS always explicitly commits
	tm_Log->txn_LogTransaction((ref)?MS_ReferenceTxn:MS_DereferenceTxn, false /*autocommit*/, db_id, tab_id, blob_id, blob_ref_id);

	self->myStmtCount++;
	
	exit_();
}


