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
 * 2009-06-09
 *
 * H&G2JCtL
 *
 * PBMS transaction handling.
 *
 * PBMS uses 1 circular transaction log. All BLOB reference operations are written to this log
 * and are applied to the repository when committed. There is 1 thread dedicated to reading the 
 * transaction log and applying the changes. During an engine level backup this thread is suspended 
 * so that no transactions will be applied to the repository files as they are backed up.
 *
 */
 
#pragma once
#ifndef __TRANSLOG_MS_H__
#define __TRANSLOG_MS_H__
#include <stddef.h>

#include "cslib/CSDefs.h"
#include "cslib/CSFile.h"
#define CHECK_TIDS
#define CRASH_TEST

#ifdef CRASH_TEST
extern uint32_t	trans_test_crash_point;
#define MAX_CRASH_POINT 10
#else
#define MAX_CRASH_POINT 0
#endif

typedef uint32_t TRef;

/*
	Transaction log info:
	
	The transaction log is a circular log of fixed length records. There is assumed to be one 
	reader thread and multiple writer threads. As records are written the 'eol' (End Of Log)
	marker is advanced and as they are read the 'start' marker is advanved. When iether marker
	reaches the end of the log a wrap around is done the marker is position back to the top of
	the list.
	
	When both markers are at the same location then the log is empty. The log is full if the 
	eol marker is just behind the start marker.
	
	If an overflow occurs then the overflow flag in the log header is set and records are written 
	to the end of the log. New records will continue to be written to the end of log until the 
	reader thread has read ALL of the records in the non overflow portion of the list. When all 
	of these records have been read then the list size will be adjusted to include the overflow
	record and the start and eol markers are repositioned and the overflow flag in the 
	header is switched off.
	
	
*/
typedef struct MSDiskTransHead {
	CSDiskValue4			th_magic_4;					/* Table magic number. */
	CSDiskValue2			th_version_2;				/* The header version. */

	CSDiskValue4			th_next_txn_id_4;			/* The next valid transaction ID. */

	CSDiskValue2			th_check_point_2;			/* The frequency whith which the start/end positions are updated in the header. */
	
	CSDiskValue4			th_requested_cache_size_4;	/* The transaction cache list size in transaction. */

	CSDiskValue8			th_list_size_8;				/* The transaction log list size in records. */
	CSDiskValue8			th_requested_list_size_8;	/* The desired list size. The log will be adjusted to this size as soon as it is convenient.*/

	CSDiskValue1			th_recovered_1;				/* A flag to indicate if the log was closed properly. */

	CSDiskValue1			th_overflow_1;				/* A flag to indicate if overflow has occurred. */

	// th_start_8 and th_eol_8 are always written at the same time.
	CSDiskValue8			th_start_8;					/* The index of the first valid record. */
	CSDiskValue8			th_eol_8;					/* The index of the first unused record or End Of Log (eol). */
	CSDiskValue1			th_checksum_1;				/* The current record checksum seed. */
} MSDiskTransHeadRec, *MSDiskTransHeadPtr;


typedef struct MSTrans_tag {
	uint32_t	tr_id;			// The transaction ID
	uint8_t	tr_type;		// The transaction type. If the first bit is set then the transaction is an autocommit.
	uint32_t	tr_db_id;		// The database ID for the operation.
	uint32_t	tr_tab_id;		// The table ID for the operation.
	uint64_t	tr_blob_id;		// The blob ID for the operation.
	uint64_t	tr_blob_ref_id;	// The blob reference id.
	uint8_t	tr_check;		// The transaction record checksum.
} MSTransRec, *MSTransPtr;


typedef struct MSTransStats {
	uint64_t	ts_LogSize;			// The number of records in the transaction log.
	uint32_t	ts_PercentFull;		// The % of the desired log size in use. This can be > 100%.
	uint64_t	ts_MaxSize;			// The log size high water mark.
	uint32_t	ts_OverflowCount;	// The number of times the log has overflowen.
	bool	ts_IsOverflowing;
	
	uint32_t ts_TransCacheSize;	// The number of transactions currently in the cache.
	uint32_t ts_PercentTransCacheUsed;	// The number of transactions currently in the cache.
	uint32_t	ts_PercentCacheHit; // The % of the transactions that were cached on writing.
} MSTransStatsRec, *MSTransStatsPtr;

typedef enum {	MS_RollBackTxn = 0, 
				MS_PartialRollBackTxn,
				MS_CommitTxn, 
				MS_ReferenceTxn, 
				MS_DereferenceTxn, 
				MS_RecoveredTxn			
} MS_Txn;

typedef enum {	MS_Running = 0,
				MS_RolledBack, 
				MS_Committed, 
				MS_Recovered			
} MS_TxnState;


#define TRANS_SET_AUTOCOMMIT(t) (t |= 0X80)	
#define TRANS_IS_AUTOCOMMIT(t) (t & 0X80)	

#define TRANS_SET_START(t) (t |= 0X40)	
#define TRANS_IS_START(t) (t & 0X40)	

#define TRANS_TYPE_IS_TERMINATED(t) (((t) == MS_RollBackTxn) || ((t) == MS_CommitTxn) || ((t) == MS_RecoveredTxn))	
#define TRANS_IS_TERMINATED(t) (TRANS_TYPE_IS_TERMINATED(TRANS_TYPE(t))  || TRANS_IS_AUTOCOMMIT(t))	
#define TRANS_TYPE(t) (t & 0X0F)	

typedef bool (*CanContinueFunc)();
typedef void (*LoadFunc)(uint64_t log_position, MSTransPtr rec);

class MSTransCache;
class MSTrans : public CSSharedRefObject {

public:
	
	MSTrans();
	~MSTrans();
	
	void txn_LogTransaction(MS_Txn type, bool autocommit = false, uint32_t db_id = 0, uint32_t tab_id = 0, uint64_t blob_id = 0, uint64_t blob_ref_id = 0);

	void txn_LogPartialRollBack(uint32_t rollBackCount)
	{
		/* Partial rollbacks store the rollback count in the place of the database id. */
		txn_LogTransaction(MS_PartialRollBackTxn, false, rollBackCount);
	}
	
	void txn_SetCheckPoint(uint16_t checkpoint)
	{
		enter_();
		
		// Important lock order. Writer threads never lock the reader but the reader
		// may lock this object so always lock the reader first.
		lock_(txn_reader);
		lock_(this);
		
		txn_MaxCheckPoint = checkpoint;
		
		if (txn_MaxCheckPoint < 10)
			txn_MaxCheckPoint = 10;
			
		if (txn_MaxCheckPoint > txn_MaxRecords)
			txn_MaxCheckPoint = txn_MaxRecords/2;
		
		if (txn_MaxCheckPoint > txn_ReqestedMaxRecords)
			txn_MaxCheckPoint = txn_ReqestedMaxRecords/2;
		
		CS_SET_DISK_2(txn_DiskHeader.th_check_point_2, txn_MaxCheckPoint);
		
		txn_File->write(&(txn_DiskHeader.th_check_point_2), offsetof(MSDiskTransHeadRec, th_check_point_2), 2);
		txn_File->flush();
		txn_File->sync();
		
		unlock_(this);
		unlock_(txn_reader);
		
		exit_();
	}
	
	void txn_SetCacheSize(uint32_t new_size);
	
	// txn_SetLogSize() may not take effect immediately but will be done
	// when there is free space at the end of the log.
	void txn_SetLogSize(uint64_t new_size);
	
	void txn_Close();	
	
	uint64_t	txn_GetSize();		// Returns the size of the log in transaction records.
	
	uint64_t	txn_GetNumRecords()	// Returns the number of transactions records waiting to be processed.
	{							// This doesn't include overflow.
		uint64_t size;
		if (txn_Start == txn_EOL)
			size = 0;
		else if (txn_Start < txn_EOL) 
			size = txn_EOL - txn_Start;
		else 
			size = txn_MaxRecords - (txn_Start - txn_EOL);
			
		return size;
	}

	// While a backup is in progress the transaction thread will not be signaled 
	// about completed transactions.
	void txn_BackupStarting() 
	{
		txn_Doingbackup = true;
		txn_reader->suspend();
	}
	
	bool txn_haveNextTransaction();
	
	void txn_BackupCompleted()
	{
		txn_Doingbackup = false;
		txn_reader->resume();
	}
	
	// The following should only be called by the transaction processing thread.
	
	// txn_GetNextTransaction() gets the next completed transaction.
	// If there is none ready it waits for one.	
	void txn_GetNextTransaction(MSTransPtr tran, MS_TxnState *state); 
		
	void txn_SetReader(CSDaemon *reader) {txn_reader = reader;}
	
	// Search the transaction log for a MS_ReferenceTxn record for the given BLOB.
	bool txn_FindBlobRef(MS_TxnState *state, uint32_t db_id, uint32_t tab_id, uint64_t blob_id);
	
	// Mark all transactions for a given database as dropped. Including commited transactions.
	void txn_dropDatabase(uint32_t db_id);
	

	uint64_t txn_GetStartPosition() { return txn_Start;}
	
	const char	*txn_GetTXNLogPath() {return txn_File->myFilePath->getCString();}
private:
	friend class ReadTXNLog;
	
	uint16_t		txn_MaxCheckPoint;	// The maximum records to be written ore read before the positions in the header are updated.

	// These fields are only used by the reader thread:
	bool		txn_Doingbackup;// Is the database being backed up.
	CSDaemon	*txn_reader;	// THe transaction log reader daemon. (unreferenced)
	bool		txn_IsTxnValid;	// Is the current transaction valid.
	TRef		txn_CurrentTxn; // The current transaction.
	uint32_t		txn_TxnIndex;	// The record index into the current transaction.
	int32_t		txn_StartCheckPoint; // Counter to determin when the read position should be flushed.
	
	void txn_PerformIdleTasks();
	
	MSTransCache	*txn_TransCache;	// Transaction cache
	
	void txn_ResizeLog();
	
	void txn_NewTransaction(); // Clears the old transaction ID
	
	bool txn_IsFull()
	{
		return (txn_HaveOverflow || ((txn_GetNumRecords() +1) == txn_MaxRecords));
	}
	
	
	uint32_t				txn_BlockingTransaction; // The transaction ID the transaction thread is waiting on.

	MSDiskTransHeadRec	txn_DiskHeader;
	CSFile				*txn_File;
	
	int32_t				txn_EOLCheckPoint; // Counter to determin when the EOL position should be flushed.
	
	// The size of the transaction log can be adjusted by setting txn_ReqestedMaxRecords.
	// The log size will be adjusted as soon as there are free slots at the bottom of the list.
	uint64_t				txn_MaxRecords;			// The number of record slots in the current list.
	uint64_t				txn_ReqestedMaxRecords;	// The number of record slots requested.  	

	uint64_t				txn_HighWaterMark; // Keeps track of the log size high water mark.
	uint64_t				txn_OverflowCount; // A count of the number of times the transaction log has over flown.
#ifdef DEBUG	
public:
	void				txn_DumpLog(const char *file);
#endif
	uint32_t				txn_MaxTID;
	bool				txn_Recovered;				// Has the log been recovered.
	bool				txn_HaveOverflow;			// A flag to indicate the list has overfown.
	uint64_t				txn_Overflow;				// The index of the next overflow record. 
	uint64_t				txn_EOL;					// The index of the first unused record or End Of Log (eol). 
	uint64_t				txn_Start;					// The index of the first valid record. 

public:	
	void txn_GetStats(MSTransStatsPtr stats);		// Get the current performance statistics.
	
private:
	uint8_t				txn_Checksum;				// The current record checksum seed. 
	
	void txn_SetFile(CSFile *tr_file);		// Set the file to use for the transaction log.
	bool txn_ValidRecord(MSTransPtr rec);	// Check to see if a record is valid.
	void txn_GetRecordAt(uint64_t index, MSTransPtr rec); // Reads 1 record from the log.
	void txn_ResetReadPosition(uint64_t pos);	// Reset txn_Start
	void txn_ResetEOL();
		
	void txn_Recover();							// Recover the transaction log.
	
	void txn_ReadLog(uint64_t read_start, bool log_locked, CanContinueFunc canContinue, LoadFunc load); // A generic method for reading the log
	void txn_LoadTransactionCache(uint64_t read_start);	// Load the transactions in the log into cache.
	
	void txn_AddTransaction(uint8_t tran_type, bool autocommit = false, uint32_t db_id = 0, uint32_t tab_id = 0, uint64_t blob_id = 0, uint64_t blob_ref_id = 0);

	
public:
	static MSTrans* txn_NewMSTrans(const char *log_path, bool dump_log = false);
};

class ReadTXNLog {
	public:
	ReadTXNLog(MSTrans *txn_log): rl_log(txn_log){}
	virtual ~ReadTXNLog(){}
		
	MSTrans *rl_log;
	void rl_ReadLog(uint64_t read_start, bool log_locked);
	virtual bool rl_CanContinue();
	virtual void rl_Load(uint64_t log_position, MSTransPtr rec);
	void rl_Store(uint64_t log_position, MSTransPtr rec);
	void rl_Flush();
};

#endif //__TRANSLOG_MS_H__
