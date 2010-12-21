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
 * 2009-06-10
 *
 * H&G2JCtL
 *
 * PBMS transaction cache.
 *
 */

#include "cslib/CSConfig.h"
#include <inttypes.h>

#include "cslib/CSGlobal.h"

#include "trans_cache_ms.h"

#define LIST_INC_SIZE 256	// If the list starts to grow it is probably because a backup is in progress so it could get quite large.
#define MIN_LIST_SIZE 32	// A list size that should be able to handle a normal transaction load.
#define MIN_CACHE_RECORDS 2	

typedef struct myTrans {
	uint8_t	tc_type;		// The transaction type. If the first bit is set then the transaction is an autocommit.
	uint32_t	tc_db_id;		// The database ID for the operation.
	uint32_t	tc_tab_id;		// The table ID for the operation.
	bool	tc_rolled_back; // 'true' if this action has been rolled back.
	uint64_t	tc_blob_id;		// The blob ID for the operation.
	uint64_t	tc_blob_ref_id;	// The blob reference id.
	uint64_t	tc_position;	// The log position of the record.
} myTransRec, *myTransPtr;

#define BAD_LOG_POSITION ((uint64_t) -1)
typedef struct TransList {
#ifdef DEBUG
	uint32_t		old_tid;
#endif
	uint32_t		tid;
	uint64_t		log_position;	// The transaction log position of the start of the transaction.
	MS_TxnState	terminated;		// 
	size_t		size;			// The allocated size of the list.
	size_t		len;			// The number of records in the list that are being used.
	myTransPtr	list;
} TransListRec, *TransListPtr;

MSTransCache::MSTransCache(): CSSharedRefObject(),
	tc_List(NULL),
	tc_OverFlow(NULL),
	tc_Size(0),
	tc_EOL(0),
	tc_First(0),
	tc_Used(0),
	tc_TotalTransCount(0),
	tc_TotalCacheCount(0),
	tc_ReLoadingThread(NULL),
	tc_OverFlowTID(0),
	tc_Full(false),
	tc_CacheVersion(0),
	tc_Recovering(false)
	{}

MSTransCache::~MSTransCache() 
{
	if (tc_List) {
		for (uint32_t i = 0; i < tc_Size; i++) {
			if (tc_List[i].list)
				cs_free(tc_List[i].list);
		}
		cs_free(tc_List);
	}
}

MSTransCache *MSTransCache::newMSTransCache(uint32_t min_size)
{
	MSTransCache *tl = NULL;
	enter_();
	
	new_(tl, MSTransCache());
	push_(tl);
	
	if (MIN_LIST_SIZE > min_size)
		min_size = MIN_LIST_SIZE;
		
	tl->tc_Initialize(min_size);
		
	pop_(tl);
	
	return_(tl);
}
	

void MSTransCache::tc_Initialize(uint32_t size)
{
	enter_();
		
	tc_Size = size;
	size++; // Add an extra for the overflow
	tc_List = (TransListPtr) cs_malloc(size * sizeof(TransListRec));
	
	// Give each new transaction list record a short list of transaction records
	for (uint32_t i = 0; i < tc_Size; i++) {
		tc_List[i].list = (myTransPtr) cs_malloc(MIN_CACHE_RECORDS * sizeof(myTransRec));
		tc_List[i].size = MIN_CACHE_RECORDS;
		tc_List[i].len = 0;
		tc_List[i].tid = 0;
		tc_List[i].log_position = 0;
		tc_List[i].terminated = MS_Running;
	}

	tc_OverFlow = tc_List + tc_Size;
	
	tc_OverFlow->list = NULL;
	tc_OverFlow->size = 0;
	tc_OverFlow->len = 0;
	tc_OverFlow->tid = 0;
	tc_OverFlow->log_position = 0;
	tc_OverFlow->terminated = MS_Running;
	exit_();
}

//--------------------
void MSTransCache::tc_SetSize(uint32_t cache_size)
{
	enter_();

	lock_(this);
	
	if (cache_size < MIN_LIST_SIZE)
		cache_size = MIN_LIST_SIZE;
	
	// If the cache is being reduced then free the record 
	// lists if the transactions about to be removed.
	for (uint32_t i = cache_size +1; i < tc_Size; i++) {
		if (tc_List[i].list)
			cs_free(tc_List[i].list);
	}

	// Add one to cache_size for overflow.	
	cs_realloc((void **) &tc_List, (cache_size +1) * sizeof(TransListRec));
	
	if (cache_size > tc_Size) {
		// Move the overflow record.
		memcpy(tc_List + cache_size, tc_List + tc_Size, sizeof(TransListRec));
		
		for (uint32_t i = tc_Size; i < cache_size; i++) {
			tc_List[i].list = (myTransPtr) cs_malloc(MIN_CACHE_RECORDS * sizeof(myTransRec));
			tc_List[i].size = MIN_CACHE_RECORDS;
			tc_List[i].len = 0;
			tc_List[i].tid = 0;
			tc_List[i].log_position = 0;
			tc_List[i].terminated = MS_Running;
		}
		
	}
	
	
	tc_Size = cache_size;
	tc_OverFlow = tc_List + tc_Size;
	
	unlock_(this);

	exit_();
}

bool MSTransCache::tc_ShoulReloadCache()
{
	return (((tc_Used +1) < tc_Size) && tc_Full);
}

uint64_t MSTransCache::tc_StartCacheReload(bool startup)
{
	enter_();
	
	(void) startup;
	
	ASSERT((startup) || tc_Full);
	tc_ReLoadingThread = self;
	tc_OverFlowTID = tc_OverFlow->tid;
	
	self->myTID = 0;
	self->myTransRef = 0;
#ifdef DEBUG
		tc_ReloadCnt =0;
#endif		

	return_(tc_OverFlow->log_position);
}

bool MSTransCache::tc_ContinueCacheReload()
{
	// Reload should continue until the list is full again and the termination records 
	// for the first and overflow transactions have been found.
	//
	// It is assumed the reload will also stop if there are no more records to
	// be read in from the log.
	
	return ((tc_List[tc_First].terminated == MS_Running) || // Keep searching for the terminator for the first txn.
			(tc_OverFlow->tid == tc_OverFlowTID) || // The old overflow txn has not yet been loaded.
			(tc_OverFlow->terminated == MS_Running)	// If the overflow tnx is terminated then the cache is also full. 
			);
}


void MSTransCache::tc_CompleteCacheReload()
{
	enter_();
	
	tc_ReLoadingThread = NULL;
	if (tc_OverFlowTID) { // Clear the overflow condition;
		tc_OverFlow->tid = 0;
		tc_OverFlowTID = 0;
		tc_Full = false;
	}
	
	exit_();
}

#define OVERFLOW_TREF (tc_Size)
#define MAX_TREF (OVERFLOW_TREF +1)

// Create a new transaction record for the specified 
// transaction.
TRef MSTransCache::tc_NewTransaction(uint32_t tid)
{
	TRef ref;
	enter_();
	
	ASSERT(tid);
	
	if (self != tc_ReLoadingThread) {
		tc_TotalTransCount++;			
	}
		
	// Once we have entered an overflow state we remain in it until
	// the cache has been reloaded even if there is now space in the cache.
	// This is to ensure that the transactions are loaded into the cache
	// in the correct order.
	// While reloading, make sure that any attempt to add a transaction by any thread
	// other than tc_ReLoadingThread recieves an overflow condition. 
	
	if (tc_Full) {
		if (tc_ReLoadingThread != self) {
			ref = MAX_TREF;
			goto done;
		}

#ifdef DEBUG
		if (!tc_ReloadCnt) {
			ASSERT(tc_OverFlow->tid == tid); // The first txn reloaded should be the overflow txn
		}
		tc_ReloadCnt++;
#endif	
		if (tid == tc_OverFlowTID) {
#ifdef DEBUG
			tc_OverFlow->old_tid = tid;
#endif	
			tc_OverFlow->tid = 0;
			tc_OverFlow->terminated = MS_Running;
			ASSERT((tc_Used +1) < tc_Size); // There should be room in the list for the old everflow txn.
		} else if (tc_OverFlowTID == 0) {
			// We are seaching for the end of the overflow txn
			// and found the start of another txn.
			ref = MAX_TREF;
			goto done;			
		}
	}

	if ((tc_Used +1) == tc_Size){ 
		// The cache is full.
		tc_OverFlowTID = 0;
		tc_OverFlow->tid = tid; // save the tid of the first transaction to overflow.
		tc_OverFlow->log_position = BAD_LOG_POSITION;
		tc_OverFlow->len = 0; 
		tc_OverFlow->terminated = MS_Running; 
		ref = OVERFLOW_TREF;
		tc_Full = true;
#ifdef DEBUG
		tc_ReloadCnt++;
#endif	
				
		goto done;
	}
	
	if (self != tc_ReLoadingThread) {
		tc_TotalCacheCount++;			
	}

	ref = tc_EOL;
	
#ifdef CHECK_TIDS
{
static uint32_t last_tid = 0;
static bool last_state = false;
if (tc_Recovering != last_state)
	last_tid = 0;
	
last_state = tc_Recovering;
if (!( ((last_tid + 1) == tid) || !last_tid))
	printf("Expected tid %"PRIu32"\n", last_tid + 1);
ASSERT( ((last_tid + 1) == tid) || !last_tid);
last_tid = tid;
}
#endif
		
	tc_List[ref].tid = tid;
	tc_List[ref].len = 0;
	tc_List[ref].log_position = BAD_LOG_POSITION;
	tc_List[ref].terminated = MS_Running;

	// Update these after initializing the structure because
	// the reader thread may read it as soon as tc_EOL is updated.
	tc_Used++;
	tc_EOL++;

	if (tc_EOL == tc_Size)
		tc_EOL = 0;

done:	
	self->myTID = tid;
	self->myTransRef = ref;
	self->myCacheVersion = tc_CacheVersion;
	return_(ref);
}

void MSTransCache::tc_FindTXNRef(uint32_t tid, TRef *tref)
{
	uint32_t i = tc_First;
	enter_();
	
	// Search for the record
	if (tc_First > tc_EOL) {
		for (; i < OVERFLOW_TREF && *tref >= MAX_TREF; i++) {
			if (tc_List[i].tid == tid)
				*tref = i;
		}
		i = 0;
	}
	
	for (; i < tc_EOL && *tref >= MAX_TREF; i++) {
		if (tc_List[i].tid == tid)
			*tref = i;
	}

	// Do not return the overflow reference if the tid = tc_OverFlowTID.
	// This may seem a bit strange but it is needed so that the overflow txn
	// will get a new non-overflow cache slot when it is reloaded.  
	if ((*tref >= MAX_TREF) && (tid == tc_OverFlow->tid) && (tid != tc_OverFlowTID))
		*tref = OVERFLOW_TREF;
		
	self->myTID = tid;
	self->myTransRef = *tref;
	self->myCacheVersion = tc_CacheVersion;
	exit_();
}

// Add a transaction record to an already existing transaction
// or possible creating a new one. Depending on the record added this may
// also commit or rollback the transaction.
void MSTransCache::tc_AddRec(uint64_t log_position, MSTransPtr rec, TRef tref)
{
	TransListPtr lrec;
	enter_();
	
	lock_(this);

	//---------
	if (tref == TRANS_CACHE_UNKNOWN_REF) { // It is coming from a reload
		ASSERT(tc_ReLoadingThread == self); // Sanity check here

		if ((self->myTID == rec->tr_id) && (self->myTransRef != TRANS_CACHE_UNKNOWN_REF))
			tref = self->myTransRef;
		else {
			tc_FindTXNRef(rec->tr_id, &tref);
			if (tref == TRANS_CACHE_UNKNOWN_REF) {
				if (!TRANS_IS_START(rec->tr_type)) 
					goto done; // Ignore partial tansaction reloads.
					
				tref = tc_NewTransaction(rec->tr_id);
			}
		}
	}
	
	ASSERT((tref <= MAX_TREF) || (tref == TRANS_CACHE_NEW_REF));
	ASSERT(self->myTID == rec->tr_id);
	
	//---------
	if (tref >= OVERFLOW_TREF) {
		if (tref == TRANS_CACHE_NEW_REF) {
			ASSERT(TRANS_IS_START(rec->tr_type));
			tref = tc_NewTransaction(rec->tr_id);
		} else if (self->myCacheVersion != tc_CacheVersion) {
			// Check to see if the transaction if now in the cache
			tc_FindTXNRef(rec->tr_id, &tref);
		}
		
		if (tref >= OVERFLOW_TREF){ // Overflow.
			if (tref == OVERFLOW_TREF) {
				if (!tc_OverFlow->len)
					tc_OverFlow->log_position = log_position;
					
				tc_OverFlow->len++;
				if (TRANS_IS_TERMINATED(rec->tr_type)) {
					if (rec->tr_type == MS_RollBackTxn)
						tc_OverFlow->terminated = MS_RolledBack;
					else if (rec->tr_type == MS_RecoveredTxn)
						tc_OverFlow->terminated = MS_Recovered;
					else
						tc_OverFlow->terminated = MS_Committed;
				}
			}
			
			goto done;
		}
	}

	lrec = tc_List + tref;
	
	ASSERT(lrec->tid);
	ASSERT(lrec->tid == rec->tr_id);
	
	if (!lrec->len) { // The first record in the transaction
		lrec->log_position = log_position;
	} else if (( (TRANS_TYPE(rec->tr_type) == MS_ReferenceTxn) || (TRANS_TYPE(rec->tr_type) == MS_DereferenceTxn)) && !tc_Recovering) { 
		// Make sure the record isn't already in the list.
		// This can happen during cache reload.
		for (uint32_t i = 0; i < lrec->len; i++) {
			if (lrec->list[i].tc_position == log_position)
				goto done;
		}
	}
	
	// During recovery there is no need to cache the records.
	if (!tc_Recovering) {
		switch (TRANS_TYPE(rec->tr_type)) {
			case MS_RollBackTxn:
			case MS_Committed:
			case MS_RecoveredTxn:
				// This is handled below;
				break;
				
			case MS_PartialRollBackTxn:
			{
				// The rollback position is stored in the place for the database id.
				for (uint32_t i = rec->tr_db_id;i < lrec->len; i++)
					lrec->list[i].tc_rolled_back = true;
					
				break;
			}

			case MS_ReferenceTxn:
			case MS_DereferenceTxn:
			{
				myTransPtr my_rec;
				
				if (lrec->len == lrec->size) { //Grow the list if required
					cs_realloc((void **) &(lrec->list), (lrec->size + 10)* sizeof(myTransRec));
					lrec->size += 10;		
				}
			
				my_rec = lrec->list + lrec->len;
				my_rec->tc_type = rec->tr_type;
				my_rec->tc_db_id = rec->tr_db_id;
				my_rec->tc_tab_id = rec->tr_tab_id;
				my_rec->tc_blob_id = rec->tr_blob_id;
				my_rec->tc_blob_ref_id = rec->tr_blob_ref_id;
				my_rec->tc_position = log_position;
				my_rec->tc_rolled_back = false;
				
				lrec->len++;				
				break;
			}
			
		}
	} else if ( (TRANS_TYPE(rec->tr_type) == MS_ReferenceTxn) || (TRANS_TYPE(rec->tr_type) == MS_DereferenceTxn))
		lrec->len++;
	
	
	// Check to see if this is a commit or rollback 
	// Do this last because as soon as it is marked as terminated
	// the reader thread may start processing it.
	if (TRANS_IS_TERMINATED(rec->tr_type)) {
		if (rec->tr_type == MS_RollBackTxn)
			lrec->terminated = MS_RolledBack;
		else if (rec->tr_type == MS_RecoveredTxn)
			lrec->terminated = MS_Recovered;
		else
			lrec->terminated = MS_Committed;
	}
	
done:
	unlock_(this);		
	exit_();
}

// Get the transaction ref of the first transaction in the list.
// Sets committed to true or false depending on if the transaction is terminated.
// If there is no trsansaction then false is returned.
bool MSTransCache::tc_GetTransaction(TRef *ref, bool *terminated)
{
	if (!tc_Used)
		return false;
	
	ASSERT(tc_List[tc_First].tid);
	
	*ref = 	tc_First;
	*terminated = (tc_List[tc_First].terminated != MS_Running);
	
	return true;
}	

//----------
bool MSTransCache::tc_GetTransactionStartPosition(uint64_t *log_position)
{
	if ((!tc_Used) || (tc_List[tc_First].len == 0))
		return false;
		
	*log_position = tc_List[tc_First].log_position;
	return true;
}

//----------
MS_TxnState MSTransCache::tc_TransactionState(TRef ref)
{
	ASSERT((ref < tc_Size) && tc_List[ref].tid);
	
	return tc_List[ref].terminated;
}	

uint32_t MSTransCache::tc_GetTransactionID(TRef ref)
{
	ASSERT((ref < tc_Size) && tc_List[ref].tid);
	
	return (tc_List[ref].tid);
}	
	
// Remove the transaction and all record associated with it.
void MSTransCache::tc_FreeTransaction(TRef tref)
{
	TransListPtr lrec;
	enter_();
	ASSERT(tc_Used && (tref < tc_Size) && tc_List[tref].tid);
	
#ifdef CHECK_TIDS
{
static uint32_t last_tid = 0;
static bool last_state = false;
if (tc_Recovering != last_state)
	last_tid = 0;
	
last_state = tc_Recovering;
ASSERT( ((last_tid + 1) == tc_List[tref].tid) || !last_tid);
last_tid = tc_List[tref].tid;
}
#endif

	lrec = tc_List + tref;
#ifdef DEBUG
	lrec->old_tid = lrec->tid;
#endif
	lrec->tid = 0;
	lrec->len = 0;
	
	if (lrec->size > 10) { // Free up some excess records.
		cs_realloc((void **) &(lrec->list), 10* sizeof(myTransRec));
		lrec->size = 10;		
	}

	lock_(this);
	tc_Used--;
	
	if (tref == tc_First) { // Reset the start of the list.
		TRef eol = tc_EOL; // cache this incase it changes 
		
		// Skip any unused records indicated by a zero tid.
		if (tc_First > eol) {
			for (; tc_First < tc_Size && !tc_List[tc_First].tid; tc_First++) ;
			
			if (tc_First == tc_Size)
				tc_First = 0;
		}
		
		for (; tc_First < eol && !tc_List[tc_First].tid; tc_First++) ;
	}
	
	ASSERT( (tc_Used == 0 && tc_First == tc_EOL) || (tc_Used != 0 && tc_First != tc_EOL)); 

	unlock_(this);

	exit_();
}

//--------------------
bool MSTransCache::tc_GetRecAt(TRef tref, size_t index, MSTransPtr rec, MS_TxnState *state)
{
	TransListPtr lrec;
	bool found = false;

	ASSERT(tc_Used && (tref < tc_Size) && tc_List[tref].tid);
#ifdef CHECK_TIDS
{
	static uint32_t last_tid = 0;
	ASSERT( ((last_tid + 1) == tc_List[tref].tid) || (last_tid  == tc_List[tref].tid) || !last_tid);
	last_tid = tc_List[tref].tid;
}
#endif
	
	lrec = tc_List + tref;
	if (index < lrec->len) {
		myTransPtr my_rec = lrec->list + index;
		
		rec->tr_type = my_rec->tc_type;
		rec->tr_db_id = my_rec->tc_db_id;
		rec->tr_tab_id = my_rec->tc_tab_id;
		rec->tr_blob_id = my_rec->tc_blob_id;
		rec->tr_blob_ref_id = my_rec->tc_blob_ref_id;
		rec->tr_id = lrec->tid;
		rec->tr_check = 0;
		if (my_rec->tc_rolled_back)
			*state = MS_RolledBack;
		else
			*state = lrec->terminated;
			
		found = true;
	}
	
	return found;
}

//--------------------
void MSTransCache::tc_dropDatabase(uint32_t db_id)
{
	enter_();
	lock_(this);
	if (tc_List) {
		for (uint32_t i = 0; i < tc_Size; i++) {
			myTransPtr rec = tc_List[i].list;
			if (rec) {
				uint32_t list_len = tc_List[i].len;			
				while (list_len) {
					if (rec->tc_db_id == db_id)
						rec->tc_db_id = 0;
					list_len--; 
					rec++;
				}
			}
		}		
	}
	
	unlock_(this);
	exit_();
}
