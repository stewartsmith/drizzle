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

#pragma once
#ifndef __OPENTABLE_MS_H__
#define __OPENTABLE_MS_H__

#include "cslib/CSDefs.h"
#include "cslib/CSFile.h"

#include "engine_ms.h"

#include "database_ms.h"

class MSOpenTablePool;

// Keep the value of MS_OT_BUFFER_SIZE less than 0XFFFF to avoid compiler warnings.
//
// From looking at transmition speed using tcpdump it looks like 16K is a goor buffer size.
// It is very important that it be larger than the TCP frame size of about 1.5K on the 
// machine I was testing on. There did not eam to be much to gain by having a buffer size
// larger tham 16K.
//
// I wonder if performance could be gained by having 2 buffers and 2 threads per send
// so that one buffer could be being sent while the other was being filled.
#ifdef DEBUG
//#define MS_OT_BUFFER_SIZE		((uint16_t)(512))
#define MS_OT_BUFFER_SIZE		((uint16_t)(16384))
#else
//#define MS_OT_BUFFER_SIZE		((uint16_t)(0XFFF0)) 
#define MS_OT_BUFFER_SIZE		((uint16_t)(16384))  
#endif

#define MS_UB_SAME_TAB			0
#define MS_UB_NEW_HANDLE		1
#define MS_UB_NEW_BLOB			2
#define MS_UB_RETAINED			3
#define MS_UB_NEW_RETAINED		4

/*
typedef struct MSUsedBlob {
	int					ub_state;
	uint16_t				ub_col_index;
	uint32_t				ub_repo_id;
	uint64_t				ub_repo_offset;

	uint16_t				ub_head_size;
	uint64_t				ub_blob_size;
	uint64_t				ub_blob_ref_id;
	uint32_t				ub_blob_id;
	uint32_t				ub_auth_code;
	char				ub_blob_url[PBMS_BLOB_URL_SIZE];
} MSUsedBlobRec, *MSUsedBlobPtr;
*/

class MSOpenTable : public CSRefObject, public CSPooled {
public:
	bool				inUse;
	bool				isNotATable;
	MSOpenTable			*nextTable;
	MSOpenTablePool		*myPool;

	CSFile				*myTableFile;

	MSRepository		*myWriteRepo;
	MSRepoFile			*myWriteRepoFile;

	MSTempLogFile		*myTempLogFile;

	char				myOTBuffer[MS_OT_BUFFER_SIZE];

	MSOpenTable();
	virtual ~MSOpenTable();

	virtual void returnToPool();

	void close();

	void createBlob(PBMSBlobURLPtr bh, uint64_t size, char *metadata, uint16_t metadata_size, CSInputStream *stream, CloudKeyPtr cloud_key = NULL, Md5Digest *checksum = NULL);
	void createBlob(PBMSBlobIDPtr blob_id, uint64_t blob_size, char *metadata, uint16_t metadata_size);
	void sendRepoBlob(uint64_t blob_id, uint64_t req_offset, uint64_t req_size, uint32_t auth_code, bool info_only, CSHTTPOutputStream *stream);
	void useBlob(int type, uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code, uint16_t col_index, uint64_t blob_size, uint64_t blob_ref_id, PBMSBlobURLPtr ret_blob_url);

	void releaseReference(uint64_t blob_id, uint64_t blob_ref_id);
	void freeReference(uint64_t blob_id, uint64_t blob_ref_id);
	void commitReference(uint64_t blob_id, uint64_t blob_ref_id);
	void checkBlob(CSStringBuffer *buffer, uint64_t blob_id, uint32_t auth_code, uint32_t temp_log_id, uint32_t temp_log_offset);
	bool deleteReferences(uint32_t temp_log_id, uint32_t temp_log_offset, bool *must_quit);

	void openForReading();
	void closeForWriting();

	bool haveTable() { return (myPool != NULL); }
	uint32_t getTableID();
	MSTable *getDBTable();
	MSDatabase *getDB();

	void formatBlobURL(PBMSBlobURLPtr blob_url, uint64_t blob_id, uint32_t auth_code, uint64_t blob_size, uint32_t tab_id, uint64_t blob_ref_id);
	void formatBlobURL(PBMSBlobURLPtr blob_url, uint64_t blob_id, uint32_t auth_code, uint64_t blob_size, uint64_t blob_ref_id);
	void formatRepoURL(PBMSBlobURLPtr blob_url, uint32_t log_id, uint64_t log_offset, uint32_t auth_code, uint64_t blob_size);

	/* Make this object linkable: */
	virtual CSObject *getNextLink() { return iNextLink; }
	virtual CSObject *getPrevLink() { return iPrevLink; }
	virtual void setNextLink(CSObject *link) { iNextLink = link; }
	virtual void setPrevLink(CSObject *link) { iPrevLink = link; }

private:
	CSObject			*iNextLink;
	CSObject			*iPrevLink;

//	uint32_t				iUseSize;
//	uint32_t				iUseCount;
//	MSUsedBlobPtr		iUsedBlobs;
	

	void openForWriting();

public:
	static MSOpenTable *newOpenTable(MSOpenTablePool *pool);
};

class MSOpenTablePool : public CSRefObject, public CSPooled {
public:
	uint32_t			myPoolTableID;						/* Non-zero if in the ID list. */
	bool			isRemovingTP;						/* Set to true if the table pool is being removed. */
	MSTable			*myPoolTable;
	MSDatabase		*myPoolDB;

	MSOpenTablePool();
	virtual ~MSOpenTablePool();

	MSOpenTable *getPoolTable();						/* Returns NULL if there is no table in the pool. */
	void returnOpenTable(MSOpenTable *otab);

	void addOpenTable(MSOpenTable *otab);
	void removeOpenTable(MSOpenTable *otab);

	void removeOpenTablesNotInUse();
	
	virtual void returnToPool();

#ifdef DEBUG
	void check();
#endif
	
	uint32_t getSize() { return iPoolTables.getSize(); }

private:
	MSOpenTable		*iTablePool;						/* A list of tables currently not in use. */
	CSLinkedList	iPoolTables;						/* A list of all tables in this pool */

public:
	static MSOpenTablePool *newPool(uint32_t db_id, uint32_t tab_id);
};

class MSTableList : public CSObject {
public:
	MSTableList();
	~MSTableList();

	static void startUp();
	static void shutDown();

	static void debug(MSOpenTable *otab);

	static MSOpenTable *getOpenTableByID(uint32_t db_id, uint32_t tab_id);
	static MSOpenTable *getOpenTableForDB(uint32_t db_id);
	static void releaseTable(MSOpenTable *otab);

	static bool removeTablePoolIfEmpty(MSOpenTablePool *pool);
	static void removeTablePool(MSOpenTablePool *pool);
	static void removeTablePool(MSOpenTable *otab);
	static void removeDatabaseTables(MSDatabase *database);

	static MSOpenTablePool *lockTablePoolForDeletion(uint32_t db_id, uint32_t tab_id, CSString *db_name, CSString *tab_name);
	static MSOpenTablePool *lockTablePoolForDeletion(MSTable *table);
	static MSOpenTablePool *lockTablePoolForDeletion(CSString *table_url);
	static MSOpenTablePool *lockTablePoolForDeletion(MSOpenTable *otab);
	static void unlockTablePool(MSOpenTablePool *pool);

private:
	static CSSyncOrderedList	*gPoolListByID;

};

#endif
