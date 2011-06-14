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
 
#pragma once
#ifndef __TRANSCACHE_MS_H__
#define __TRANSCACHE_MS_H__

#include "cslib/CSDefs.h"
#include "trans_log_ms.h"

#define TRANS_CACHE_NEW_REF	((TRef)-1)
#define TRANS_CACHE_UNKNOWN_REF	((TRef)-2)

class MSTransCache : public CSSharedRefObject 
{
public:
	MSTransCache();
	~MSTransCache();
	
	// Prepare to add another record to a tansaction
	// This will preallocate anything required so that the call
	// to tc_AddRec() cannot fail.
	//void tc_PrepAddRec(TRef tref, uint32_t tr_id);

	// Add a transaction record to an already existing transaction
	// or possible creating a new one. Depending on the record added this may
	// also commit or rollback the transaction.
	// Returns false if the list is full.
	//void tc_AddRec(uint64_t log_position, MSTransPtr rec);
	void tc_AddRec(uint64_t log_position, MSTransPtr rec, TRef tref = TRANS_CACHE_UNKNOWN_REF);
	void tc_LoadRec(uint64_t log_position, MSTransPtr rec) { tc_AddRec(log_position, rec);}
	
	// Get the transaction ref of the first transaction in the list.
	// Sets terminated to true or false depending on if the transaction is terminated.
	// If there is no trsansaction then false is returned.
	bool tc_GetTransaction(TRef *ref, bool *terminated);	

	uint32_t tc_GetTransactionID(TRef ref);	
	
	// Get the state of the terminated transaction.
	MS_TxnState tc_TransactionState(TRef ref);	
	
	// Remove the transaction and all record associated with it.
	void tc_FreeTransaction(TRef tref);
	
	// Get the log position of the first transaction in the list.
	// Returns false if there is no transaction.
	bool tc_GetTransactionStartPosition(uint64_t *log_position);
	
	// Get the nth record for the specified transaction. A pointer to the
	// storage location is passed in. If there is no nth record 'false' is returned
	// otherwise the passed in record buffer is filled out and the pointer to
	// it is returned.
	bool tc_GetRecAt(TRef tref, size_t index, MSTransPtr rec, MS_TxnState *state);
	
	uint32_t tc_GetCacheUsed() { return tc_Used;}
	uint32_t tc_GetPercentCacheUsed() { return (tc_Used * 100)/(tc_Size-1);}
	uint32_t tc_GetPercentCacheHit() { return (tc_TotalCacheCount * 100)/tc_TotalTransCount;}

	// Notify the cache that recovery is in progress.
	// In this case transaction records are not cached.
	void tc_SetRecovering(bool recovering) {tc_Recovering = recovering;}
	
	// Methods used to update the cache from disk.
	bool tc_ShoulReloadCache();		// Test to see if the cache needs to be reloaded.
	uint64_t tc_StartCacheReload(bool startup = false);	// Initialize the cache for reload.
	bool tc_ContinueCacheReload();	// Returns false when the cache is full.
	void tc_CompleteCacheReload();	// Signal the end of the cache reload operation.

	void tc_UpdateCacheVersion() {tc_CacheVersion++;}
	
	void tc_SetSize(uint32_t cache_size); // Chang the size of the cache.
	
	void tc_dropDatabase(uint32_t db_id); // clears records from ther cache for a dropped database.

	static MSTransCache *newMSTransCache(uint32_t cache_size);
	
private:
	void tc_Initialize(uint32_t size);
	TRef tc_NewTransaction(uint32_t tid);	
	void tc_FindTXNRef(uint32_t tid, TRef *tref);

	struct TransList	*tc_List;		// The transaction list, One entry per transaction.
	struct TransList	*tc_OverFlow;	// The first transaction to overflow.
	uint32_t				tc_Size;		// The current size of the list
	uint32_t				tc_EOL;			// The index of the first unused transaction list record
	uint32_t				tc_First;		// The index of the first used transaction list record
	uint32_t				tc_Used;		// The number of used transaction list records

	uint64_t				tc_TotalTransCount;	// The number of transaction to be cached
	uint64_t				tc_TotalCacheCount;	// The number of transaction cached
	
	CSThread			*tc_ReLoadingThread; // The thread performing a reload.
	uint32_t				tc_OverFlowTID; // The transaction id of the first transaction in th reload.
	bool				tc_Full;
	uint32_t				tc_CacheVersion;
	
	bool				tc_Recovering;

#ifdef DEBUG
	uint32_t tc_ReloadCnt;
#endif
	
};


#endif // __TRANSCACHE_MS_H__
