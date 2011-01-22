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
#include "cslib/CSConfig.h"

#include <stdlib.h>
#include <inttypes.h>

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSStorage.h"

#include "trans_log_ms.h"
#include "trans_cache_ms.h"

#ifdef CRASH_TEST
uint32_t	trans_test_crash_point;
#define CRASH_POINT(p) { if (p == trans_test_crash_point) { char *ptr = NULL; printf("Crash on demand at: %s(%d), start: %"PRIu64", eol: %"PRIu64"\n", __FILE__, __LINE__, txn_Start, txn_EOL); *ptr = 88;}}
#else
#define CRASH_POINT(p)
#endif

#define MS_TRANS_LOG_MAGIC			0xA6E7D7B3
#define MS_TRANS_LOG_VERSION		1
#define MS_TRANS_LOG_RECOVERED		0XA1
#define MS_TRANS_LOG_NOT_RECOVERED	0XA2
#define MS_TRANS_NO_OVERFLOW		0XB1
#define MS_TRANS_OVERFLOW			0XB2

#define DFLT_TRANS_CHECKPOINT_THRESHOLD	1024

#define	DFLT_TRANS_LOG_LIST_SIZE	(1024 * 10)
#define DFLT_TRANS_CACHE_SIZE		(500)

#define TRANS_CAN_RESIZE	((txn_MaxRecords != txn_ReqestedMaxRecords) && (txn_EOL >= txn_Start) && !txn_HaveOverflow)

typedef struct MSDiskTrans {
	CSDiskValue4	dtr_id_4;			// The transaction ID
	CSDiskValue1	dtr_type_1;			// The transaction type. If the first bit is set then the transaction is an autocommit.
	CSDiskValue1	dtr_check_1;		// The trransaction record checksum.
	CSDiskValue4	dtr_db_id_4;		// The database ID for the operation.
	CSDiskValue4	dtr_tab_id_4;		// The table ID for the operation.
	CSDiskValue8	dtr_blob_id_8;		// The blob ID for the operation.
	CSDiskValue8	dtr_blob_ref_id_8;	// The blob reference id.
} MSDiskTransRec, *MSDiskTransPtr;

#define SET_DISK_TRANSREC(d, s) { \
	CS_SET_DISK_4((d)->dtr_id_4, (s)->tr_id);\
	CS_SET_DISK_1((d)->dtr_type_1, (s)->tr_type);\
	CS_SET_DISK_1((d)->dtr_check_1, (s)->tr_check);\
	CS_SET_DISK_4((d)->dtr_db_id_4, (s)->tr_db_id);\
	CS_SET_DISK_4((d)->dtr_tab_id_4, (s)->tr_tab_id);\
	CS_SET_DISK_8((d)->dtr_blob_id_8, (s)->tr_blob_id);\
	CS_SET_DISK_8((d)->dtr_blob_ref_id_8, (s)->tr_blob_ref_id);\
}

#define GET_DISK_TRANSREC(s, d) { \
	(s)->tr_id = CS_GET_DISK_4((d)->dtr_id_4);\
	(s)->tr_type = CS_GET_DISK_1((d)->dtr_type_1);\
	(s)->tr_check = CS_GET_DISK_1((d)->dtr_check_1);\
	(s)->tr_db_id = CS_GET_DISK_4((d)->dtr_db_id_4);\
	(s)->tr_tab_id = CS_GET_DISK_4((d)->dtr_tab_id_4);\
	(s)->tr_blob_id = CS_GET_DISK_8((d)->dtr_blob_id_8);\
	(s)->tr_blob_ref_id = CS_GET_DISK_8((d)->dtr_blob_ref_id_8);\
}

static uint8_t checksum(uint8_t *data, size_t len)
{
	register uint32_t	sum = 0, g;
	uint8_t				*chk;

	chk = data + len - 1;
	while (chk > data) {
		sum = (sum << 4) + *chk;
		if ((g = sum & 0xF0000000)) {
			sum = sum ^ (g >> 24);
			sum = sum ^ g;
		}
		chk--;
	}
	return (uint8_t) (sum ^ (sum >> 24) ^ (sum >> 16) ^ (sum >> 8));
}

MSTrans::MSTrans() : 
	CSSharedRefObject(),
	txn_MaxCheckPoint(0),
	txn_Doingbackup(false),
	txn_reader(NULL),
	txn_IsTxnValid(false),
	txn_CurrentTxn(0),
	txn_TxnIndex(0),
	txn_StartCheckPoint(0),
	txn_TransCache(NULL),
	txn_BlockingTransaction(0),
	txn_File(NULL),
	txn_EOLCheckPoint(0),
	txn_MaxRecords(0),
	txn_ReqestedMaxRecords(0),
	txn_HighWaterMark(0),
	txn_OverflowCount(0),
	txn_MaxTID(0),
	txn_Recovered(false),
	txn_HaveOverflow(false),
	txn_Overflow(0),
	txn_EOL(0),
	txn_Start(0),
	txn_Checksum(0)
{
}

MSTrans::~MSTrans()
{
	txn_Close();
	if (txn_TransCache) 
		txn_TransCache->release();
		
}	

void MSTrans::txn_Close()
{
	
	if (txn_File) {		
		// Set the header to indicate that the log has not been closed properly. 
		CS_SET_DISK_4(txn_DiskHeader.th_next_txn_id_4, txn_MaxTID);
		txn_File->write(&(txn_DiskHeader.th_next_txn_id_4), offsetof(MSDiskTransHeadRec, th_next_txn_id_4), 4 );

		CS_SET_DISK_8(txn_DiskHeader.th_start_8, txn_Start);
		CS_SET_DISK_8(txn_DiskHeader.th_eol_8, txn_EOL);
		CS_SET_DISK_1(txn_DiskHeader.th_checksum_1, txn_Checksum);
		txn_File->write(&(txn_DiskHeader.th_start_8), 
							offsetof(MSDiskTransHeadRec, th_start_8), 
							sizeof(MSDiskTransHeadRec) - offsetof(MSDiskTransHeadRec, th_start_8) );
CRASH_POINT(1);
		txn_File->flush();
		txn_File->sync();

		if (txn_Recovered) {
			// Write the recovered flag seperately just incase of a crash during the write operation.
			CS_SET_DISK_1(txn_DiskHeader.th_recovered_1, MS_TRANS_LOG_RECOVERED);		
			txn_File->write(&(txn_DiskHeader.th_recovered_1), offsetof(MSDiskTransHeadRec, th_recovered_1), 1 );
			txn_File->flush();
			txn_File->sync();
		}
		
		txn_File->close();
		txn_File->release();
		txn_File = NULL;		
	}
}
void MSTrans::txn_SetFile(CSFile *tr_file)
{
	txn_File = tr_file;
}

//#define TRACE_ALL
#ifdef TRACE_ALL
static FILE *txn_debug_log;
#endif

MSTrans *MSTrans::txn_NewMSTrans(const char *log_path, bool dump_log)
{
	MSTrans *trans = NULL;
	CSPath *path = NULL;
	uint64_t log_size;
	enter_();

	(void) dump_log;
	
	new_(trans, MSTrans());
	push_(trans);

	path = CSPath::newPath(log_path);
	push_(path);	
		
try_again:
	
	
	if (!path->exists()) { // Create the transaction log.	
		// Preallocate the log space and initialize it.
		MSDiskTransRec *recs;
		off64_t offset = sizeof(MSDiskTransHeadRec);
		uint64_t num_records = DFLT_TRANS_LOG_LIST_SIZE;
		size_t size;
		CSFile *tr_file;
		
		recs = (MSDiskTransRec *) cs_calloc(1024 * sizeof(MSDiskTransRec));
		push_ptr_(recs);
		
		tr_file = path->createFile(CSFile::CREATE);
		push_(tr_file);
		
		log_size = DFLT_TRANS_LOG_LIST_SIZE * sizeof(MSDiskTransRec) + sizeof(MSDiskTransHeadRec);

		
		while (num_records) {
			if (num_records < 1024)
				size = num_records;
			else
				size = 1024;
			tr_file->write(recs, offset, size * sizeof(MSDiskTransRec));
			offset += size * sizeof(MSDiskTransRec);
			num_records -= size;
		}

		trans->txn_MaxRecords = DFLT_TRANS_LOG_LIST_SIZE;
		trans->txn_ReqestedMaxRecords = DFLT_TRANS_LOG_LIST_SIZE;
		trans->txn_MaxCheckPoint = DFLT_TRANS_CHECKPOINT_THRESHOLD;
		trans->txn_MaxTID = 1;

		// Initialize the log header.
		CS_SET_DISK_4(trans->txn_DiskHeader.th_magic_4, MS_TRANS_LOG_MAGIC);
		CS_SET_DISK_2(trans->txn_DiskHeader.th_version_2, MS_TRANS_LOG_VERSION);
		
		CS_SET_DISK_4(trans->txn_DiskHeader.th_next_txn_id_4, trans->txn_MaxTID);

		CS_SET_DISK_2(trans->txn_DiskHeader.th_check_point_2, trans->txn_MaxCheckPoint);

		CS_SET_DISK_8(trans->txn_DiskHeader.th_list_size_8, trans->txn_MaxRecords);
		CS_SET_DISK_8(trans->txn_DiskHeader.th_requested_list_size_8, trans->txn_ReqestedMaxRecords);

		CS_SET_DISK_4(trans->txn_DiskHeader.th_requested_cache_size_4, DFLT_TRANS_CACHE_SIZE);
		
		CS_SET_DISK_8(trans->txn_DiskHeader.th_start_8, 0);
		CS_SET_DISK_8(trans->txn_DiskHeader.th_eol_8, 0);
		
		CS_SET_DISK_1(trans->txn_DiskHeader.th_recovered_1, MS_TRANS_LOG_RECOVERED);
		CS_SET_DISK_1(trans->txn_DiskHeader.th_checksum_1, 1);
		CS_SET_DISK_1(trans->txn_DiskHeader.th_overflow_1, MS_TRANS_NO_OVERFLOW);
		
		tr_file->write(&(trans->txn_DiskHeader), 0, sizeof(MSDiskTransHeadRec));
		pop_(tr_file);
		trans->txn_SetFile(tr_file);
		
		trans->txn_Checksum = CS_GET_DISK_1(trans->txn_DiskHeader.th_checksum_1);
		
		trans->txn_TransCache = MSTransCache::newMSTransCache(DFLT_TRANS_CACHE_SIZE);
		
		release_(recs);

	} else { // The transaction log already exists
		bool overflow = false, recovered = false;
		
		CSFile *tr_file = path->createFile(CSFile::DEFAULT); // Open read/write
		push_(tr_file);
		
		// Read the log header:
		if (tr_file->read(&(trans->txn_DiskHeader), 0, sizeof(MSDiskTransHeadRec), 0) < sizeof(MSDiskTransHeadRec)) {
			release_(tr_file);
			path->removeFile();
			goto try_again;
		}
		
		// check the log header:		
		if (CS_GET_DISK_4(trans->txn_DiskHeader.th_magic_4) != MS_TRANS_LOG_MAGIC)
			CSException::throwFileError(CS_CONTEXT, path->getCString(), CS_ERR_BAD_HEADER_MAGIC);
		
		if (CS_GET_DISK_2(trans->txn_DiskHeader.th_version_2) != MS_TRANS_LOG_VERSION)
			CSException::throwFileError(CS_CONTEXT, path->getCString(), CS_ERR_VERSION_TOO_NEW);
			
		//----
		if (CS_GET_DISK_1(trans->txn_DiskHeader.th_overflow_1) == MS_TRANS_NO_OVERFLOW) 
			overflow = false;
		else if (CS_GET_DISK_1(trans->txn_DiskHeader.th_overflow_1) == MS_TRANS_OVERFLOW) 
			overflow = true;
		else 
			CSException::throwFileError(CS_CONTEXT, path->getCString(), CS_ERR_BAD_FILE_HEADER);
		
		//----
		if (CS_GET_DISK_1(trans->txn_DiskHeader.th_recovered_1) == MS_TRANS_LOG_NOT_RECOVERED) 
			recovered = false;
		else if (CS_GET_DISK_1(trans->txn_DiskHeader.th_recovered_1) == MS_TRANS_LOG_RECOVERED) 
			recovered = true;
		else 
			CSException::throwFileError(CS_CONTEXT, path->getCString(), CS_ERR_BAD_FILE_HEADER);

		// Check that the log is the expected size.
		log_size = CS_GET_DISK_8(trans->txn_DiskHeader.th_list_size_8) * sizeof(MSDiskTransRec) + sizeof(MSDiskTransHeadRec);
		
		if ((log_size > tr_file->getEOF()) || 
			((log_size < tr_file->getEOF()) && !overflow)){	
					
			char buffer[CS_EXC_MESSAGE_SIZE];		
			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unexpected transaction log size: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, path->getCString());
			CSException::throwException(CS_CONTEXT, CS_ERR_BAD_FILE_HEADER, buffer);
		}
		
		trans->txn_MaxTID = CS_GET_DISK_4(trans->txn_DiskHeader.th_next_txn_id_4);

		// Looks good, we will assume it is a valid log file.
		trans->txn_TransCache = MSTransCache::newMSTransCache(CS_GET_DISK_4(trans->txn_DiskHeader.th_requested_cache_size_4));

		pop_(tr_file);
		trans->txn_SetFile(tr_file);

		trans->txn_MaxCheckPoint = CS_GET_DISK_2(trans->txn_DiskHeader.th_check_point_2);
		
		trans->txn_MaxRecords = CS_GET_DISK_8(trans->txn_DiskHeader.th_list_size_8);
		trans->txn_ReqestedMaxRecords = CS_GET_DISK_8(trans->txn_DiskHeader.th_requested_list_size_8);
		
		trans->txn_Checksum = CS_GET_DISK_1(trans->txn_DiskHeader.th_checksum_1);
		trans->txn_EOL = CS_GET_DISK_8(trans->txn_DiskHeader.th_eol_8);
		trans->txn_Start = CS_GET_DISK_8(trans->txn_DiskHeader.th_start_8);
		trans->txn_HaveOverflow = overflow;
		if (overflow) 
			trans->txn_Overflow = (tr_file->getEOF() - sizeof(MSDiskTransHeadRec)) /sizeof(MSDiskTransRec); 
		else
			trans->txn_Overflow = 0;

#ifdef DEBUG
		if (overflow)
			printf("Recovering overflow log\n");
		if (dump_log) {
			char name[100];
			snprintf(name, 100, "%dms-trans-log.dump", (int)time(NULL));
			trans->txn_DumpLog(name);
		}
#endif
		// Recover the log if required.
		if (!recovered)
			trans->txn_Recover();
			
			
	}
	
	trans->txn_Recovered = true; // Any recovery required has been completed.

	// The log has been recovered so these values should be valid:
	trans->txn_EOL = CS_GET_DISK_8(trans->txn_DiskHeader.th_eol_8);
	trans->txn_Start = CS_GET_DISK_8(trans->txn_DiskHeader.th_start_8);
	
	// Set the header to indicate that the log has not been closed properly. 
	// This is reset when the log is closed during shutdown.
	CS_SET_DISK_1(trans->txn_DiskHeader.th_recovered_1, MS_TRANS_LOG_NOT_RECOVERED);
	trans->txn_File->write(&(trans->txn_DiskHeader.th_recovered_1), offsetof(MSDiskTransHeadRec, th_recovered_1), 1);
	
	// Load the transaction records into memory.
	trans->txn_TransCache->tc_StartCacheReload(true);
	trans->txn_LoadTransactionCache(trans->txn_Start);
	trans->txn_TransCache->tc_CompleteCacheReload();
	
	if (trans->txn_MaxRecords != trans->txn_ReqestedMaxRecords) 
		trans->txn_ResizeLog(); // Try to resize but it may not be possible yet.
	
	release_(path);
	pop_(trans);

#ifdef TRACE_ALL
	
	txn_debug_log = fopen("log_dump.txt", "w+");
	if (!txn_debug_log) {
		perror("log_dump.txt");
	}
#endif
	
	return_(trans);
}

bool MSTrans::txn_ValidRecord(MSTransPtr rec)
{
	uint8_t check = rec->tr_check;
	bool ok;
	
	rec->tr_check = txn_Checksum;
	ok = (checksum((uint8_t*)rec, sizeof(MSTransRec)) == check);
	rec->tr_check = check;
	return ok;
}

void MSTrans::txn_GetRecordAt(uint64_t index, MSTransPtr rec)
{
	MSDiskTransRec drec;
	off64_t offset;
	
	// Read 1 record from the log and convert it from disk format.
	offset = sizeof(MSDiskTransHeadRec) + index * sizeof(MSDiskTransRec);		
	txn_File->read(&drec, offset, sizeof(MSDiskTransRec), sizeof(MSDiskTransRec));
	GET_DISK_TRANSREC(rec, &drec);
}

// Recovery involves finding the start of the first record and the eof
// position. The positions will be found at or after the position stored
// in the header. 
void MSTrans::txn_Recover()
{
	MSTransRec rec = {0,0,0,0,0,0,0};
	uint64_t original_eol = txn_EOL;
	enter_();

#ifdef DEBUG
printf("Recovering 	transaction log!\n");
#endif

	txn_MaxTID = 0;
	// Search for the last valid record in the log starting from the last 
	// known position stored in the header.
	for (; txn_EOL < txn_MaxRecords; txn_EOL++) {
		txn_GetRecordAt(txn_EOL, &rec);
		if (! txn_ValidRecord(&rec))
			break;	
	}
	
	if (txn_EOL == txn_MaxRecords) {
		// It looks like all the records in the log are valid?
		// This is strange but could happen if the crash
		// occurred just before updating the header as the
		// eol position rolled over to the top of the log.
		txn_EOL = 0;
	}
	
	txn_MaxTID++;
	
	CS_SET_DISK_8(txn_DiskHeader.th_eol_8, txn_EOL);
	
	// If the actual eol has moved pass the recorded start position
	// then the actuall start position must be some where beyond
	// the eol.
	if (((original_eol < txn_Start) || (original_eol > txn_EOL)) && (txn_EOL >= txn_Start)) 
		txn_Start = txn_EOL +1;	

	// Position the start at the beginning of a transaction.	
	uint64_t end_search = (txn_Start < txn_EOL)? txn_EOL : txn_MaxRecords;
	for (; txn_Start < end_search; txn_Start++) {
		txn_GetRecordAt(txn_Start, &rec);
		if (TRANS_IS_START(rec.tr_type))
			break;	
	}

	if (txn_Start == end_search)
		txn_Start = txn_EOL; 
		
	CS_SET_DISK_8(txn_DiskHeader.th_start_8, txn_Start);
	
	txn_TransCache->tc_SetRecovering(true); 
	// Load the transaction records into the cache.
	txn_TransCache->tc_StartCacheReload(true);
	txn_LoadTransactionCache(txn_Start);
	txn_TransCache->tc_CompleteCacheReload();
	
	// Now go through all the transactions and add rollbacks for any
	// unterminated transactions.
	TRef ref;
	bool terminated;
	while  (txn_TransCache->tc_GetTransaction(&ref, &terminated)) {
		
		txn_MaxTID = txn_TransCache->tc_GetTransactionID(ref); // Save the TID of the last transaction.
		if (!terminated) {
			self->myTID = txn_MaxTID;
			self->myTransRef = ref;
			self->myStartTxn = false;
			txn_AddTransaction(MS_RecoveredTxn);
		}
CRASH_POINT(2);
		txn_TransCache->tc_FreeTransaction(ref);
		
		// Load the next block of transactions into the cache.
		// This needs to be done after each tc_GetTransaction() to make sure
		// that if the transaction terminator is some where in the log
		// it will get read even if the cache is completely full. 
		if (txn_TransCache->tc_ShoulReloadCache()) {
			txn_LoadTransactionCache(txn_TransCache->tc_StartCacheReload(true));
			txn_TransCache->tc_CompleteCacheReload();
		}
	}
	
	
	txn_TransCache->tc_SetRecovering(false); 
	self->myTransRef = 0;
	
	// Update the header again incase rollbacks have been added.
	CS_SET_DISK_8(txn_DiskHeader.th_eol_8, txn_EOL);
				
	exit_();
}

bool ReadTXNLog::rl_CanContinue() 
{ 
	return rl_log->txn_TransCache->tc_ContinueCacheReload();
}

void ReadTXNLog::rl_Load(uint64_t log_position, MSTransPtr rec) 
{
	rl_log->txn_TransCache->tc_AddRec(log_position, rec);
}

void ReadTXNLog::rl_Store(uint64_t log_position, MSTransPtr rec) 
{
	MSDiskTransRec drec;
	SET_DISK_TRANSREC(&drec, rec);

	rl_log->txn_File->write(&drec, sizeof(MSDiskTransHeadRec) + log_position * sizeof(MSDiskTransRec) , sizeof(MSDiskTransRec));
}

void ReadTXNLog::rl_Flush() 
{
	rl_log->txn_File->flush();
	rl_log->txn_File->sync();
}

void ReadTXNLog::rl_ReadLog(uint64_t read_start, bool log_locked)
{
	uint64_t	size, orig_size;
	bool reading_overflow = (read_start >= rl_log->txn_MaxRecords);
	enter_();
	

	// Get the number of transaction records to be loaded.
	if (reading_overflow) {
		orig_size = rl_log->txn_Overflow;
		size = rl_log->txn_Overflow - read_start;
	} else {
		orig_size = rl_log->txn_GetNumRecords();
	
		if (rl_log->txn_Start <= read_start)
			size = orig_size - (read_start - rl_log->txn_Start);
		else 
			size = rl_log->txn_EOL - read_start;
	}
	
	// load all the records
	while (size && rl_CanContinue()) {
		MSDiskTransRec diskRecords[1000];
		uint32_t read_size;
		off64_t offset;
		
		if (size > 1000) 
			read_size = 1000 ;
		else
			read_size = size ;
		
		// Check if we have reached the wrap around point in the log.
		if ((!reading_overflow) && (rl_log->txn_EOL < read_start) && ((rl_log->txn_MaxRecords - read_start) < read_size))
			read_size = rl_log->txn_MaxRecords - read_start ;

		// Read the next block of records.
		offset = sizeof(MSDiskTransHeadRec) + read_start * sizeof(MSDiskTransRec);		
		rl_log->txn_File->read(diskRecords, offset, read_size* sizeof(MSDiskTransRec), read_size* sizeof(MSDiskTransRec));
		
		// Convert the records from disk format and add them to the cache.
		for (uint32_t i = 0; i < read_size && rl_CanContinue(); i++) {
			MSTransRec rec;
			MSDiskTransPtr drec = diskRecords + i;
			GET_DISK_TRANSREC(&rec, drec);
			
			rl_Load(read_start + i, &rec); 
		}
		
		size -= read_size;
		read_start += read_size;
		if (read_start == rl_log->txn_MaxRecords)
			read_start = 0;
	}
	
	if (rl_log->txn_HaveOverflow && !reading_overflow) {
		if (rl_CanContinue()) 
			rl_ReadLog(rl_log->txn_MaxRecords, false);
		
	} else if (!log_locked) {	
		// The following is intended to prevent the case where a writer 
		// writes an txn record while the cache is full but just after 
		// the reload has completed. If the cache is not yet full we need
		// to load as many of the new records into cache as possible.
	
		uint64_t	new_size;
		lock_(rl_log);
		if (reading_overflow)
			new_size = rl_log->txn_Overflow;
		else 
			new_size = rl_log->txn_GetNumRecords();
		if (rl_CanContinue() && (orig_size != new_size)) {
			rl_ReadLog(read_start, true);
		}
		unlock_(rl_log);
	}
	

	exit_();
}

void MSTrans::txn_LoadTransactionCache(uint64_t read_start)
{
	ReadTXNLog log(this);
	enter_();
	log.rl_ReadLog(read_start, false);
	txn_TransCache->tc_UpdateCacheVersion(); // Signal writes to recheck cache for overflow txn refs.
	exit_();
}

void  MSTrans::txn_ResizeLog()
{
	enter_();
	
	lock_(this);
	if (TRANS_CAN_RESIZE) {
		// TRANS_CAN_RESIZE checks that there is no overflow and the the start position 
		// is less than eol. This implies the from eol to the end of file doesn't contain
		// and used records.
		

#ifdef DEBUG	
		uint64_t old_size = txn_MaxRecords;
#endif		
		if (txn_MaxRecords > txn_ReqestedMaxRecords) { // Shrink the log
			uint64_t max_resize = txn_MaxRecords - txn_EOL;
			
			if ( txn_Start == txn_EOL)
				max_resize = txn_MaxRecords;
			else {
				max_resize = txn_MaxRecords - txn_EOL;
				if (!txn_Start) // If start is at '0' then the EOL cannot be wrapped.
					max_resize--;
			}
			
				
			if (max_resize > (txn_MaxRecords - txn_ReqestedMaxRecords))
				max_resize = txn_MaxRecords - txn_ReqestedMaxRecords;
							
			txn_MaxRecords -= 	max_resize;
		} else
			txn_MaxRecords = txn_ReqestedMaxRecords; // Grow the log

#ifdef DEBUG			
		char buffer[CS_EXC_MESSAGE_SIZE];		
		snprintf(buffer, CS_EXC_MESSAGE_SIZE, "Resizing the Transaction log from %"PRIu64" to %"PRIu64" \n",  old_size, txn_MaxRecords);
		CSException::logException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, buffer);
#endif

		CS_SET_DISK_8(txn_DiskHeader.th_list_size_8, txn_MaxRecords);
		
		txn_File->setEOF(txn_MaxRecords * sizeof(MSDiskTransRec) + sizeof(MSDiskTransHeadRec));
		txn_File->write(&(txn_DiskHeader.th_list_size_8), offsetof(MSDiskTransHeadRec, th_list_size_8), 8);
		
		if (txn_Start == txn_EOL) {
			txn_Start = 0;
			txn_EOL = 0;
		} else if (txn_MaxRecords == txn_EOL) {
			txn_EOL = 0;
		}
		
		txn_ResetEOL();
				
	}	
	unlock_(this);
	
	exit_();
}

void  MSTrans::txn_ResetEOL()
{
	enter_();
	
	txn_EOLCheckPoint = txn_MaxCheckPoint;
	txn_StartCheckPoint = txn_MaxCheckPoint;
	
	if (!txn_EOL)
		txn_Checksum++;
	CS_SET_DISK_8(txn_DiskHeader.th_eol_8, txn_EOL);
	CS_SET_DISK_8(txn_DiskHeader.th_start_8, txn_Start);
	CS_SET_DISK_1(txn_DiskHeader.th_checksum_1, txn_Checksum);
	txn_File->write(&(txn_DiskHeader.th_start_8), 
						offsetof(MSDiskTransHeadRec, th_start_8), 
						sizeof(MSDiskTransHeadRec) - offsetof(MSDiskTransHeadRec, th_start_8) );
CRASH_POINT(5);
	txn_File->flush();
	txn_File->sync();
CRASH_POINT(10);
		
	exit_();
}

#define PRINT_TRANS(tid, a, t) 

#ifndef PRINT_TRANS
#define PRINT_TRANS(tid, a, t) printTrans(tid, a, t)
static void printTrans(uint32_t tid, bool autocommit, MS_Txn type)
{
	const char *type_name = "???";
	
	switch (type) {
		case MS_RollBackTxn:
			type_name = "Rollback";
			break;
		case MS_PartialRollBackTxn:
			type_name = "PartialRollBack";
			break;
		case MS_CommitTxn:
			type_name = "Commit";
			break;
		case MS_ReferenceTxn:
			type_name = "Reference";
			break;
		case MS_DereferenceTxn:
			type_name = "Dereference";
			break;
		case MS_RecoveredTxn:
			type_name = "Recovered";
			break;
	}
	
	fprintf(stderr, "MSTrans::txn_LogTransaction(%d, autocommit = %s, %s)\n", tid, (autocommit)?"On":"Off", type_name);

}
#endif

void MSTrans::txn_LogTransaction(MS_Txn type, bool autocommit, uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id)
{
	enter_();
	
	lock_(this);
	if (!self->myTID) {
		switch (type) {
			case MS_RollBackTxn:
			case MS_PartialRollBackTxn:
			case MS_CommitTxn: {
				unlock_(this);
				exit_();;
			}
			case MS_ReferenceTxn:
			case MS_DereferenceTxn:
			case MS_RecoveredTxn:
				break;
		}
		txn_MaxTID++;
		self->myTID = txn_MaxTID;
		self->myTransRef = TRANS_CACHE_NEW_REF;
		self->myStartTxn = true;
	}

	PRINT_TRANS(self->myTID, autocommit, type);
	
	txn_AddTransaction(type, autocommit, db_id, tab_id, blob_id, blob_ref_id);
	if (autocommit || TRANS_TYPE_IS_TERMINATED(type))
		txn_NewTransaction();
		
	unlock_(this);
	
	exit_();
}

void  MSTrans::txn_AddTransaction(uint8_t tran_type, bool autocommit, uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id)
{
	MSTransRec rec = {0,0,0,0,0,0,0}; // This must be set to zero so that the checksum will be valid. 
	MSDiskTransRec drec;
	uint64_t new_offset = txn_EOL;
	bool do_flush = true;

	enter_();

	
	// Check that the log is not already full.
	if (txn_IsFull()) {		
		if (!txn_HaveOverflow) { // The first overflow record: update the header.
			CS_SET_DISK_1(txn_DiskHeader.th_overflow_1, MS_TRANS_OVERFLOW);
			txn_File->write(&(txn_DiskHeader.th_overflow_1), offsetof(MSDiskTransHeadRec, th_overflow_1), 1);

			CS_SET_DISK_8(txn_DiskHeader.th_start_8, txn_Start);
			CS_SET_DISK_8(txn_DiskHeader.th_eol_8, txn_EOL);
			txn_File->write(&(txn_DiskHeader.th_start_8), offsetof(MSDiskTransHeadRec, th_start_8), 16);

			txn_File->flush();
			txn_File->sync();
			txn_HaveOverflow = true;
			txn_OverflowCount++;
			txn_Overflow = 	txn_MaxRecords;		
		}
		
		new_offset = txn_Overflow;
	}
		
	rec.tr_id = self->myTID ;
	rec.tr_type = tran_type;
	rec.tr_db_id = db_id;
	rec.tr_tab_id = tab_id;
	rec.tr_blob_id = blob_id;
	rec.tr_blob_ref_id = blob_ref_id;
	
	if (self->myStartTxn) {
		TRANS_SET_START(rec.tr_type);
		self->myStartTxn = false;
	}
		
	if (autocommit) {
		TRANS_SET_AUTOCOMMIT(rec.tr_type);
	}
		
#ifdef TRACE_ALL
if (txn_debug_log){
char *ttype, *cmt;
switch (TRANS_TYPE(rec.tr_type)) {
	case MS_ReferenceTxn:
		ttype = "+";
		break;
	case MS_DereferenceTxn:
		ttype = "-";
		break;
	case MS_RollBackTxn:
		ttype = "rb";
		rec.tr_blob_ref_id = 0;
		break;
	case MS_RecoveredTxn:
		ttype = "rcov";
		rec.tr_blob_ref_id = 0;
		break;
	default:
		ttype = "???";
}

if (TRANS_IS_TERMINATED(rec.tr_type))
	cmt = "c";
else
	cmt = "";
			
fprintf(txn_debug_log, "%"PRIu32" \t\t%s%s %"PRIu64" %"PRIu32" %"PRIu64" %"PRIu64"  %"PRIu64" %d\n", self->myTID, ttype, cmt, rec.tr_blob_ref_id, rec.tr_tab_id, txn_Start, txn_EOL, new_offset, txn_HaveOverflow);
}
#endif

	rec.tr_check = txn_Checksum; 
	
	// Calculate the records checksum.
	rec.tr_check = checksum((uint8_t*)&rec, sizeof(rec));
	
	// Write the record to disk.
	SET_DISK_TRANSREC(&drec, &rec);
#ifdef CRASH_TEST

	if (trans_test_crash_point == 9) { // do a partial write before crashing
		txn_File->write(&drec, sizeof(MSDiskTransHeadRec) + new_offset * sizeof(MSDiskTransRec) , sizeof(MSDiskTransRec)/2 );
		CRASH_POINT(9);
	} else
		txn_File->write(&drec, sizeof(MSDiskTransHeadRec) + new_offset * sizeof(MSDiskTransRec) , sizeof(MSDiskTransRec) );
#else	
	txn_File->write(&drec, sizeof(MSDiskTransHeadRec) + new_offset * sizeof(MSDiskTransRec) , sizeof(MSDiskTransRec) );
#endif
CRASH_POINT(3);
	// There is no need to sync if the transaction is still running.
	if (TRANS_IS_TERMINATED(tran_type)) {
		CRASH_POINT(4); // This crash will result in a verify error because the txn was committed to the log but not the database.
		txn_File->flush();
		txn_File->sync();
		do_flush = false;
	}
	
	if (!txn_HaveOverflow) { // No need to update the header if overflowing.
		uint64_t rec_offset = txn_EOL;
		
		txn_EOL = new_offset;
		txn_EOL++;
		
		if (txn_EOL == txn_MaxRecords) {
			// The eol has rolled over.
			txn_EOL = 0;		
		}

		txn_EOLCheckPoint--;
		if ((!txn_EOLCheckPoint) || !txn_EOL) {
			
			// Flush the previouse write if required before updating the header.
			// This is just in case it crashes during the sync to make sure that the
			// header information is correct for the data on disk. If the crash occurred
			// between writing the header and the record the header on disk would be wrong.
			if (do_flush) {
				txn_File->flush();
				txn_File->sync();
			}
			
			txn_ResetEOL();
		}
		
		txn_TransCache->tc_AddRec(rec_offset, &rec, self->myTransRef);
		
		if (txn_GetNumRecords() > txn_HighWaterMark)
			txn_HighWaterMark = txn_GetNumRecords();
			
	} else { // Ovewrflow
		txn_TransCache->tc_AddRec(txn_Overflow, &rec, self->myTransRef);
		txn_Overflow++;
		if (txn_Overflow > txn_HighWaterMark)
			txn_HighWaterMark = txn_Overflow;
	}
	
	ASSERT(txn_EOL < txn_MaxRecords);
	ASSERT(txn_Start < txn_MaxRecords);
	exit_();
}

uint64_t MSTrans::txn_GetSize()		
{
	return sizeof(MSDiskTransHeadRec) + txn_MaxRecords * sizeof(MSDiskTransRec);
}

//---------------
void MSTrans::txn_NewTransaction()
{ 
	enter_();

	self->myTID = 0;	// This will be assigned when the first record is written.
	
	exit_();
}

//---------------
void MSTrans::txn_PerformIdleTasks()
{
	enter_();
	
	if (txn_TransCache->tc_ShoulReloadCache()) {
		txn_LoadTransactionCache(txn_TransCache->tc_StartCacheReload());
		txn_TransCache->tc_CompleteCacheReload();
		exit_();
	}
	
	// During backup the reader is suspended. This may need to be changed
	// if we decide to actually do something here.
	txn_reader->suspendedWait(1000);
	exit_();
}

//---------------
void MSTrans::txn_ResetReadPosition(uint64_t pos)
{	
	bool rollover = (pos < txn_Start);
	enter_();
	
	if (pos >= txn_MaxRecords) { // Start of overflow
		lock_(this);
		
		// Overflow has occurred and the circular list is now empty 
		// so expand the list to include the overflow and 
		// reset txn_Start and txn_EOL
		txn_Start = txn_MaxRecords;
		txn_MaxRecords = txn_Overflow;
		txn_EOL = 0;
		txn_HaveOverflow = false;
		txn_Overflow = 0;
		
		CS_SET_DISK_1(txn_DiskHeader.th_overflow_1, MS_TRANS_NO_OVERFLOW);
		CS_SET_DISK_8(txn_DiskHeader.th_list_size_8, txn_MaxRecords);
		txn_File->write(&(txn_DiskHeader.th_overflow_1),	offsetof(MSDiskTransHeadRec, th_overflow_1),	1);
		txn_File->write(&(txn_DiskHeader.th_list_size_8),	offsetof(MSDiskTransHeadRec, th_list_size_8),	8);
				
		txn_ResetEOL();
		
		unlock_(this);
	} else
		txn_Start = pos;	
	
	ASSERT(txn_Start <= txn_MaxRecords);
	
	if (!rollover)
		txn_StartCheckPoint -= (pos - txn_Start);
	
	// Flush the header if the read position has rolled over or it is time. 
	if ( rollover || (txn_StartCheckPoint <=0)) {
		lock_(this);
		CS_SET_DISK_8(txn_DiskHeader.th_start_8, txn_Start);
		CS_SET_DISK_8(txn_DiskHeader.th_eol_8, txn_EOL);
		txn_File->write(&(txn_DiskHeader.th_start_8), offsetof(MSDiskTransHeadRec, th_start_8), 16);
CRASH_POINT(5);
		txn_File->flush();
		txn_File->sync();
		txn_StartCheckPoint = txn_MaxCheckPoint;
		unlock_(this);
	}
	
CRASH_POINT(6);
	
	if (TRANS_CAN_RESIZE) 
		txn_ResizeLog();
		
	exit_();
}
//---------------
bool MSTrans::txn_haveNextTransaction() 
{
	bool terminated = false;
	TRef ref;
	
	txn_TransCache->tc_GetTransaction(&ref, &terminated);
	
	return terminated;
}

//---------------
void MSTrans::txn_GetNextTransaction(MSTransPtr tran, MS_TxnState *state)
{
	bool terminated;
	uint64_t log_position;
	enter_();
	
	ASSERT(txn_reader == self);
	lock_(txn_reader);
	
	do {
		// Get the next completed transaction.
		// this will suspend the current thread, which is assumed
		// to be the log reader, until one is available.
		while ((!txn_IsTxnValid) && !self->myMustQuit) {

			// wait until backup has completed.
			while (txn_Doingbackup && !self->myMustQuit)
				txn_PerformIdleTasks();
	
			if (txn_TransCache->tc_GetTransaction(&txn_CurrentTxn, &terminated) && terminated) {
				txn_IsTxnValid = true;
				txn_TxnIndex = 0;
			} else
				txn_PerformIdleTasks();
		}
		
		if (self->myMustQuit)
			break;
			
		if (txn_TransCache->tc_GetRecAt(txn_CurrentTxn, txn_TxnIndex++, tran, state)) 
			break;
			
CRASH_POINT(7);
		txn_TransCache->tc_FreeTransaction(txn_CurrentTxn);
CRASH_POINT(8);
		if (txn_TransCache->tc_GetTransactionStartPosition(&log_position)) {
			txn_ResetReadPosition(log_position);
		}else{
			if (txn_TransCache->tc_ShoulReloadCache()) {
				uint64_t pos = txn_TransCache->tc_StartCacheReload();
				txn_ResetReadPosition(pos);
				txn_LoadTransactionCache(pos);
				txn_TransCache->tc_CompleteCacheReload();
			} else {
				// Lock the object to prevent writer thread updates while I check again.
				// This is to ensure that txn_EOL is not changed between the call to
				// tc_GetTransactionStartPosition() and setting the read position.
				lock_(this);
				if (txn_TransCache->tc_GetTransactionStartPosition(&log_position)) 
					txn_ResetReadPosition(log_position);
				else
					txn_ResetReadPosition(txn_EOL);
				unlock_(this);
			}
		}
		
		txn_IsTxnValid = false;
			
	} while (1);
	
	unlock_(txn_reader);
	exit_();
}


void MSTrans::txn_GetStats(MSTransStatsPtr stats)
{
	
	if (txn_HaveOverflow) {
		stats->ts_IsOverflowing = true;
		stats->ts_LogSize = txn_Overflow;
	} else {
		stats->ts_IsOverflowing = false;
		stats->ts_LogSize = txn_GetNumRecords();
	}
	stats->ts_PercentFull = (stats->ts_LogSize * 100) / CS_GET_DISK_8(txn_DiskHeader.th_requested_list_size_8);

	stats->ts_MaxSize = txn_HighWaterMark;
	stats->ts_OverflowCount = txn_OverflowCount;
	
	stats->ts_TransCacheSize = txn_TransCache->tc_GetCacheUsed();
	stats->ts_PercentTransCacheUsed = txn_TransCache->tc_GetPercentCacheUsed();
	stats->ts_PercentCacheHit = txn_TransCache->tc_GetPercentCacheHit();
}

void MSTrans::txn_SetCacheSize(uint32_t new_size)
{
	enter_();
	// Important lock order. Writer threads never lock the reader but the reader
	// may lock this object so always lock the reader first.
	lock_(txn_reader);
	lock_(this);

	CS_SET_DISK_4(txn_DiskHeader.th_requested_cache_size_4, new_size);
	
	txn_File->write(&(txn_DiskHeader.th_requested_cache_size_4), offsetof(MSDiskTransHeadRec, th_requested_cache_size_4), 4);
	txn_File->flush();
	txn_File->sync();

	txn_TransCache->tc_SetSize(new_size);

	unlock_(this);
	unlock_(txn_reader);
	exit_();
}

void MSTrans::txn_SetLogSize(uint64_t new_size)
{
	enter_();
	
	// Important lock order. Writer threads never lock the reader but the reader
	// may lock this object so always lock the reader first.
	lock_(txn_reader);
	lock_(this);
	
	txn_ReqestedMaxRecords = (new_size - sizeof(MSDiskTransHeadRec)) / sizeof(MSDiskTransRec);
	
	if (txn_ReqestedMaxRecords < 10)
		txn_ReqestedMaxRecords = 10;
	
	CS_SET_DISK_8(txn_DiskHeader.th_requested_list_size_8, txn_ReqestedMaxRecords);
	
	txn_File->write(&(txn_DiskHeader.th_requested_list_size_8), offsetof(MSDiskTransHeadRec, th_requested_list_size_8), 8);
	txn_File->flush();
	txn_File->sync();
	
	unlock_(this);
	unlock_(txn_reader);
	
	exit_();
}

// A helper class for resetting database IDs in the transaction log.
class DBSearchTXNLog : ReadTXNLog {
	public:
	DBSearchTXNLog(MSTrans *log): ReadTXNLog(log), sdb_db_id(0), sdb_isDirty(false) {}
	
	uint32_t sdb_db_id; 
	bool sdb_isDirty;
	
	virtual bool rl_CanContinue() { return true;}
	virtual void rl_Load(uint64_t log_position, MSTransPtr rec) 
	{
		if  (rec->tr_db_id == sdb_db_id) {
			sdb_isDirty = true;
			rec->tr_db_id = 0;
			rl_Store(log_position, rec);
		} 
	}
	
	void SetDataBaseIDToZero(uint32_t db_id)
	{
		sdb_db_id = db_id;
		rl_ReadLog(rl_log->txn_GetStartPosition(), false);
		if (sdb_isDirty)
			rl_Flush();
	}
};

// Dropping the database from the transaction log just involves
// scanning the log and setting the database id of any transactions 
// involving the dropped database to zero.
void MSTrans::txn_dropDatabase(uint32_t db_id)
{
	enter_();
	
	// Important lock order. Writer threads never lock the reader but the reader
	// may lock this object so always lock the reader first.
	lock_(txn_reader);
	lock_(this);
	
	// Clear any transaction records in the cache for the dropped database;
	txn_TransCache->tc_dropDatabase(db_id);
	
	// Scan the log setting the database ID for any record belonging to the
	// dropped database to zero. 
	DBSearchTXNLog searchLog(this);
	
	searchLog.SetDataBaseIDToZero(db_id);
		
	unlock_(this);
	unlock_(txn_reader);
	exit_();
}

#ifdef DEBUG	
void MSTrans::txn_DumpLog(const char *file)
{
	size_t	size, read_start = 0;
	FILE *fptr;
	enter_();
	
	fptr = fopen(file, "w+");
	if (!fptr) {
		perror(file);
		return;
	}
	
	if (txn_Overflow)
		size = txn_Overflow;
	else
		size = txn_MaxRecords;
	
	// Dump all the records
	while (size) {
		MSDiskTransRec diskRecords[1000];
		uint32_t read_size;
		off64_t offset;
		
		if (size > 1000) 
			read_size = 1000 ;
		else
			read_size = size ;
		
		// Read the next block of records.
		offset = sizeof(MSDiskTransHeadRec) + read_start * sizeof(MSDiskTransRec);		
		txn_File->read(diskRecords, offset, read_size* sizeof(MSDiskTransRec), read_size* sizeof(MSDiskTransRec));
		
		for (uint32_t i = 0; i < read_size; i++) {
			const char *ttype, *cmt;
			MSTransRec rec;
			MSDiskTransPtr drec = diskRecords + i;
			GET_DISK_TRANSREC(&rec, drec);
			
			switch (TRANS_TYPE(rec.tr_type)) {
				case MS_ReferenceTxn:
					ttype = "+";
					break;
				case MS_DereferenceTxn:
					ttype = "-";
					break;
				case MS_RollBackTxn:
					ttype = "rb";
					rec.tr_blob_ref_id = 0;
					break;
				case MS_RecoveredTxn:
					ttype = "rcov";
					rec.tr_blob_ref_id = 0;
					break;
				default:
					ttype = "???";
			}
			
			if (TRANS_IS_TERMINATED(rec.tr_type))
				cmt = "c";
			else
				cmt = "";
			
			
			fprintf(fptr, "%"PRIu32" \t\t%s%s %"PRIu64" %"PRIu32" \t %s %s %s\n", rec.tr_id, ttype, cmt, rec.tr_blob_ref_id, rec.tr_tab_id, 
				((read_start + i) == txn_Start) ? "START":"",
				((read_start + i) == txn_EOL) ? "EOL":"",
				((read_start + i) == txn_MaxRecords) ? "OverFlow":""
				);
		}
		
		size -= read_size;
		read_start += read_size;
	}
	fclose(fptr);
	exit_();
}	

#endif

