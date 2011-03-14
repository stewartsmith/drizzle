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
#ifndef __REPOSITORY_MS_H__
#define __REPOSITORY_MS_H__
#include <stddef.h>

#include "cslib/CSDefs.h"
#include "cslib/CSFile.h"
#include "cslib/CSMd5.h"
#include "engine_ms.h"
#include "cloud_ms.h"
#include "pbmslib.h"

#define MS_BLOB_HEADER_MAGIC		0x9213BA24
#define MS_REPO_FILE_MAGIC			0x5678CDEF
#define MS_REPO_FILE_VERSION		3
#define MS_REPO_FILE_HEAD_SIZE		128

#ifdef DEBUG
#define MS_REPO_MIN_REF_COUNT		3				// Initial number of references to allow space for:(Table, Delete, Cloud)
#define MS_REPO_MIN_MATADATA		0			
#else
#define MS_REPO_MIN_REF_COUNT		6				// Initial number of references to allow space for.
#define MS_REPO_MIN_MATADATA		128				
#endif

#define BLOB_IN_REPOSITORY(t) ( t < MS_CLOUD_STORAGE)
#define BLOB_IN_CLOUD(t) ( t == MS_CLOUD_STORAGE)

// References are marked as committed or uncommitted as an aid when
// doing a backup to indicate which references were added after the
// backup began.
#define COMMIT_MASK(id)		((id) & 0X7FFFFFFFFFFFFFFFll) // The high bit is used internally to flag uncommitted references.
#define IS_COMMITTED(id)	(((id) & 0X8000000000000000ll) == 0)
#define UNCOMMITTED(id)		((id) | 0X8000000000000000ll)

class MSOpenTable;
class MSDatabase;
class MSRepository;
class CSHTTPOutputStream;

/* Repository file structure:
	MSRepoHeadRec:<BLOB_RECORDS>
	
	BLOB_RECORDS: <BLOB_RECORD> <BLOB_RECORDS>
	BLOB_RECORD: MSBlobHeadRec <BLOB_REFERENCES> BlobData
	BLOB_REFERENCES:<

*/

/*
 * In theory a database can containg repository records created with different versions of PBMS
 * which have different repository header sizes. The reallity though is that this is not really 
 * supported yet. If this is ever supported the header data will have to be processed
 * after being read from disk before it can be accessed. This will be left until it is actually needed.
 */
typedef struct MSRepoHead {
	CSDiskValue4			rh_magic_4;							/* Table magic number. */
	CSDiskValue2			rh_version_2;						/* The header version. */
	CSDiskValue2			rh_repo_head_size_2;				/* The size of this header. */
	CSDiskValue2			rh_blob_head_size_2;				/* The size of this header for each blob sizeof(MSBlobHeadRec). */
	CSDiskValue2			rh_def_ref_size_2;					/* The default size of references. */
	CSDiskValue8			rh_garbage_count_8;

	/* NOTE: Keep the next 5 fields together (and in this order)
	* they are written together in syncHead().
	 */
	CSDiskValue8			rh_recovery_offset_8;				/* The last confirmed, flushed offset (start recovery point)! */
	CSDiskValue4			rh_last_temp_time_4;				/* Time of the last temp BLOB in this log. */
	CSDiskValue4			rh_last_access_4;					/* Last access time (in seconds). */
	CSDiskValue4			rh_create_time_4;					/* Last access time (in seconds). */
	CSDiskValue4			rh_last_ref_4;						/* Last reference time (in seconds). */

	CSDiskValue4			rh_reserved_4;
} MSRepoHeadRec, *MSRepoHeadPtr;

#define MS_BLOB_ALLOCATED			1	/* The BLOB exists but is scheduled for deletion. */
#define MS_BLOB_REFERENCED			2	/* The BLOB exists and is referenced. */
#define MS_BLOB_DELETED				3	/* The BLOB has been deleted and can be cleaned up.. */
#define MS_BLOB_MOVED				4	/* The BLOB was moved while a backup was in progress and can be cleaned up when the compactor is resumed. */
// The only difference between MS_BLOB_DELETED and MS_BLOB_MOVED is that the backup process will backup BLOBs that were moved.

#define VALID_BLOB_STATUS(s) (s >= MS_BLOB_ALLOCATED && s <= MS_BLOB_MOVED)
#define IN_USE_BLOB_STATUS(s) (s >= MS_BLOB_ALLOCATED && s <= MS_BLOB_REFERENCED)

#define MS_SHORT_AUTH_CODE(ac)		((uint16_t) (((ac) & 0x0000FFFF) ^ (ac) >> 16))
/*
 * BLOB record structure: {
	 {Blob Header}		(See MSBlobHead below.)
	 {Blob references}	(An array of rb_ref_count_2 reference records each of size rb_ref_size_1)
	 {Blob Metadata}	(Null terminated string pairs of the format: <name> <value>)
	 {The BLOB!}		(Depending on the type of BLOB storage being used this may be the actual blob data or a URL to it.)
 }
 */
/*
 * The blob alias is a special metadata tag that can be used as a key to access the blob.
 * For this reason it is handled differently in that an index is defined on it.
 */
typedef struct MSBlobHead {
	/* 
	 * Important: rb_last_access_4 and rb_access_count_4 are always updated at the same time 
	 * and are assumed to be in this order.
	 */
	CSDiskValue4			rb_last_access_4;					/* Last access time (in seconds). */
	CSDiskValue4			rb_access_count_4;					/* The number of times the BLOB has been read. */
	CSDiskValue4			rb_create_time_4;					/* Creation time (in seconds). */
	CSDiskValue4			rd_magic_4;							/* BLOB magic number. */
	CSDiskValue1			rb_storage_type_1;					/* The type of BLOB storage being used. */

	CSDiskValue2			rb_ref_count_2;						/* The number of reference slots in the header. They may not all be used. */
	CSDiskValue1			rb_ref_size_1;						/* The size of references in this header. */
	CSDiskValue4			rb_mod_time_4;						/* Last access modification time (in seconds). */
	
	/* The header size may be oversize to allow for the addition of references and metadata before		*/
	/* having to relocate the blob. The references array starts at the top of the variable header space	*/
	/* and grows down while the metadata starts at the bottom and grows up. If the 2 spaces meet then	*/
	/* a new BLOB record must be allocated and the entire BLOB relocated. :(							*/
	
	CSDiskValue2			rb_head_size_2;						/* The size of the entire header. (The offset from the start of the header to the BLOB data.)*/
	CSDiskValue6			rb_blob_repo_size_6;				/* The size of the blob data sotred in the repository. For repository BLOBs this is the same as rb_blob_data_size_6 */
	CSDiskValue6			rb_blob_data_size_6;				/* The size of the actual blob. */
	Md5Digest				rb_blob_checksum_md5d;				/* The MD5 digest of the blob. */

	CSDiskValue4			rb_alias_hash_4;					/* The alias name hash value.*/
	CSDiskValue2			rb_alias_offset_2;					/* The offset from the start of the header to the BLOB metadata alias value if it exists.*/
	CSDiskValue2			rb_mdata_offset_2;					/* The offset from the start of the header to the BLOB metadata.*/
	CSDiskValue2			rb_mdata_size_2;					/* The size of the  metadata.*/

	/* 
	 * The rb_s3_key_id_4 field is used to generate a database wide
	 * unique persistent id for the BLOB that can be used as 
	 * an S3 key.
	 *
	 * This is done by combining the  rb_s3_key_id_4 with the rb_create_time_4.
	 * 
	 */
	CSDiskValue4			rb_s3_key_id_4;	

	/* 
	 * The rb_s3_cloud_ref_4 field is a reference into the pbms.pbms_cloud 
	 * table containing S3 storage information. 
	 */
	CSDiskValue4			rb_s3_cloud_ref_4;	

	/* Reserved space to allow for new header fields without 
	 * having to change the size of this header.
	 */
	 CSDiskValue4			rb_unused[2];
	
	/* These are changed when referencing/dereferencing a BLOB: */
	CSDiskValue1			rb_status_1;
	CSDiskValue4			rb_backup_id_4;						/* Used with the MS_BLOB_MOVED flag to indicate that a moved BLOB should be backed up. */
	CSDiskValue4			rb_last_ref_4;						/* Last reference time (in seconds). */
	CSDiskValue4			rb_auth_code_4;						/* Authorisation code. NOTE! Always last 4 bytes of the
																 * header of the header! */
	
} MSBlobHeadRec, *MSBlobHeadPtr;
#define MS_METADAT_OFFSET(header_size, current_metadata_size, metadata_size)		(header_size - current_metadata_size - metadata_size)
#define MS_MIN_BLOB_HEAD_SIZE		((uint16_t)(offsetof(MSBlobHeadRec, rb_auth_code_4) + 4))

#define MS_VAR_SPACE(bh)			((int32_t)((CS_GET_DISK_2(bh->rb_head_size_2) - MS_MIN_BLOB_HEAD_SIZE) -(CS_GET_DISK_2(bh->rb_ref_count_2) * CS_GET_DISK_1(bh->rb_ref_size_1)) - CS_GET_DISK_2(bh->rb_mdata_size_2)))
#define MS_CAN_ADD_REFS(bh, n)		(MS_VAR_SPACE(bh) >= (int32_t)(n * CS_GET_DISK_1(bh->rb_ref_size_1)))
#define MS_CAN_ADD_MDATA(bh, l)		(MS_VAR_SPACE(bh) >= (int32_t)l)


#define MS_BLOB_STAT_OFFS			offsetof(MSBlobHeadRec, rb_status_1)
#define MS_BLOB_META_OFFS			offsetof(MSBlobHeadRec, rb_alias_offset_2)

#define MS_BLOB_FREE_REF			0x0000						/* A free reference */
#define MS_BLOB_TABLE_REF			0xFFFF						/* A table reference */
#define MS_BLOB_DELETE_REF			0xFFFE						/* A templog deletion reference */

#define INVALID_INDEX				0xFFFF

// This is a generic reference structure that is
// compatable with MSRepoTableRef, MSRepoTempRef, and MSRepoBlobRef
typedef struct MSRepoGenericRef {
	CSDiskValue2			rr_type_2;
	CSDiskValue2			rr_reserved_2;
	uint8_t					er_unused[8];
} MSRepoGenericRefRec, *MSRepoGenericRefPtr;

// Notes on references stored in the BLOB's repository header:
//
// For every table that has a reference to the BLOB there is
// 1 table ref (MSRepoTableRefRec) in the BLOB's header.
// For every reference to the BLOB from within the database tables
// there is 1 BLOB ref (MSRepoBlobRefRec) in the BLOB's header.
// The BLOB ref points to the BLOB's table ref in the header.
//
// If the same BLOB is referenced more than once from the same table 
// there will only be one MSRepoTableRefRec for all the references but
// each reference will have its own MSRepoBlobRefRec.
//
//
// In addition there may be 1 or more temp log references used for
// performing delayed offline actions on the BLOB such as deleting
// it or moving it to a cloud.
//
// (BLOB aliases should be implimented as another type of reference.)

/* Points to a reference to the blob from a table. */
typedef struct MSRepoTableRef {
	CSDiskValue2			rr_type_2;							/* MS_BLOB_TABLE_REF */
	CSDiskValue4			tr_table_id_4;						/* Table ID (non-zero if valid). */
	CSDiskValue6			tr_blob_id_6;						/* Blob ID (non-zero if valid). (offset into the table refernce log.)*/
} MSRepoTableRefRec, *MSRepoTableRefPtr;

/* Points to a reference to the blob from a temp log. */
typedef struct MSRepoTempRef {
	CSDiskValue2			rr_type_2;							/* MS_BLOB_DELETE_REF */
	CSDiskValue2			tp_del_ref_2;						/* The index of reference to be removed. Index is 1 based.
																 * If set to INVALID_INDEX then this record is not related to a table reference. */ 
	CSDiskValue4			tp_log_id_4;						/* Temp log id. */
	CSDiskValue4			tp_offset_4;						/* Offset if temp log. */
} MSRepoTempRefRec, *MSRepoTempRefPtr;

// Barry:
// A blob reference is a backward reference from the BLOB
// back up into the table referencing it.
// 
// Historicly it could have beeen used to access 
// the referencing row via an engine callback. This is no longer supported.
// It is now used to store a unique ID for the BLOB reference. This is used
// to avoid possible multiple BLOB decrement or increment operations during
// recovery. They could also be used to locate the record referencing to the BLOB 
// in the table. 
// 
// There is a 1:1 relationship between the number of blob references in
// a BLOB's header and the number of times that BLOB exists in  tables in the
// database.
typedef struct MSRepoBlobRef {
	CSDiskValue2			er_table_2;			/* Index of the table reference (a MS_BLOB_TABLE_REF record) Index is 1 based. Can be -1 */
	CSDiskValue2			er_col_index_2;		/* The column index of the BLOB. */
	CSDiskValue8			er_blob_ref_id_8;	/* The unique ID of the BLOB reference.*/
} MSRepoBlobRefRec, *MSRepoBlobRefPtr;

typedef union MSRepoPointers {
	char					*rp_chars;
	uint8_t					*rp_bytes;
	MSBlobHeadPtr			rp_head;
	MSRepoGenericRefPtr		rp_ref;								
	MSRepoTableRefPtr		rp_tab_ref;
	MSRepoTempRefPtr		rp_temp_ref;
	MSRepoBlobRefPtr		rp_blob_ref;
} MSRepoPointersRec, *MSRepoPointersPtr;

#define MS_BLOB_KEY_SIZE	17

class MSRepoFile : public CSFile, public CSPooled {
public:
	MSRepository	*myRepo;
	bool			isFileInUse;
	MSRepoFile		*nextFile;									/* Next file available in the pool */

	MSRepoFile();
	virtual ~MSRepoFile();

	uint64_t readBlobChunk(PBMSBlobIDPtr blob_id, uint64_t rep_offset, uint64_t blob_offset, uint64_t buffer_size, char *buffer);
	void writeBlobChunk(PBMSBlobIDPtr blob_id, uint64_t rep_offset, uint64_t blob_offset, uint64_t data_size, char *data);
	//void sendBlob(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint64_t size, CSHTTPOutputStream *stream);
	void sendBlob(MSOpenTable *otab, uint64_t offset, uint64_t req_offset, uint64_t req_size, uint32_t auth_code, bool with_auth_code, bool info_only, CSHTTPOutputStream *stream);
	void referenceBlob(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id, uint32_t auth_code, uint16_t col_index);
	void setBlobMetaData(MSOpenTable *otab, uint64_t offset, const char *meta_data, uint16_t meta_data_len, bool reset_alias, const char  *alias);
	void releaseBlob(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id, uint32_t auth_code);
	void commitBlob(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id, uint32_t auth_code);
private:
	bool getBlobRefSpace(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id, 
						uint32_t auth_code, MSRepoTableRefPtr *tab_ref, MSRepoGenericRefPtr *free_ref, uint16_t	*tab_ref_cnt, uint64_t	*blob_size);
	void realFreeBlob(MSOpenTable *otab, char *buffer, uint32_t auth_code, uint64_t offset, uint16_t head_size, uint64_t blob_size, size_t ref_size);
public:
	void freeTableReference(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code);
	void checkBlob(CSStringBuffer *buffer, uint64_t offset, uint32_t auth_code, uint32_t temp_log_id, uint32_t temp_log_offset);

	void updateAccess(MSBlobHeadPtr blob, uint64_t rep_offset);	
	virtual void returnToPool();

	virtual CSObject *getNextLink() { return iNextLink; }
	virtual CSObject *getPrevLink() { return iPrevLink; }
	virtual void setNextLink(CSObject *link) { iNextLink = link; }
	virtual void setPrevLink(CSObject *link) { iPrevLink = link; }

	friend class MSRepository;

private:
	
	CSObject		*iNextLink;
	CSObject		*iPrevLink;

	void update_blob_header(MSOpenTable *otab, uint64_t offset, uint64_t blob_size, uint16_t head_size, uint16_t new_head_size);
	void removeBlob(MSOpenTable *otab, uint32_t tab_id, uint64_t blob_id, uint64_t offset, uint32_t auth_code);
	static MSRepoFile *newRepoFile(MSRepository *repo, CSPath *path);
	
	void updateGarbage(uint64_t size);
	
public:
	static void getBlobKey(MSBlobHeadPtr blob, CloudKeyPtr key)
	{
		key->creation_time = CS_GET_DISK_4(blob->rb_create_time_4);
		key->ref_index = CS_GET_DISK_4(blob->rb_s3_key_id_4);
		key->cloud_ref = CS_GET_DISK_4(blob->rb_s3_cloud_ref_4);
	}

};

#define CS_REPO_REC_LOCK_COUNT			31

typedef enum RepoLockStates { // These states are actually bit masks
	REPO_UNLOCKED = 0,		// Repository is not locked by anyone.
	REPO_COMPACTING = 1,	// Repository is locked by the compactor thread.
	REPO_WRITE = 2,			// Repository is locked for writing a new BLOB to it.
	REPO_BACKUP = 4			// Repository is locked for backup.
	} RepoLockState;

// The REPO_COMPACTING and REPO_WRITE states are mutualy exclusive but REPO_BACKUP is not.


// It is possible that when a repository is scheduled for backup it is already locked by the compactor thread
// or it is locked because a new BLOB is being written to it. In the cases where it is locked by the compactor,
// the compactore is suspended until the repository is backed up. In the case where a BLOB is being written
// to it both threads are allowed access to it and the resetting of the lock state is handled in returnToPool().
// It is safe to allow the backup thread to access the repository at the same time as other threads because
// backup is a read only operation.  
class MSRepository : public CSSharedRefObject, public CSPooled {
public:
	uint32_t			myRepoID;
	off64_t			myRepoFileSize;
	uint32_t			myRepoLockState;	// Bit mask of RepoLockStates						
	bool			isRemovingFP;								/* Set to true if the file pool is being removed. */
	CSMutex			myRepoLock[CS_REPO_REC_LOCK_COUNT];
	CSMutex			myRepoWriteLock;		// Writing requires it's own lock. 
	MSDatabase		*myRepoDatabase;
	off64_t			myGarbageCount;
	size_t			myRepoHeadSize;
	int				myRepoDefRefSize;
	size_t			myRepoBlobHeadSize;
	
	off64_t			myRecoveryOffset;							/* The starting point for the next recovery. */
	time_t			myLastTempTime;
	time_t			myLastAccessTime;
	time_t			myLastCreateTime;
	time_t			myLastRefTime;
	
	bool			mustBeDeleted;								/* Set to true if the repository should be deleted when freed. */

	MSRepository(uint32_t id, MSDatabase *db, off64_t file_size);
	~MSRepository();

	/* TODO: Check recovery after crash after each phase below. */
	void openRepoFileForWriting(MSOpenTable *otab);
	uint64_t receiveBlob(MSOpenTable *otab, uint16_t head_size, uint64_t blob_size, Md5Digest *checksum = NULL, CSInputStream *stream = NULL);
	uint64_t copyBlob(MSOpenTable *otab, uint64_t size, CSInputStream *stream); // Makes a copy of the complete BLOB with header.
	void writeBlobHead(MSOpenTable *otab, uint64_t offset, uint8_t ref_size, uint16_t head_size, uint64_t size, Md5Digest *checksum, char *metadata, uint16_t metadata_size, uint64_t blob_id, uint32_t auth_code, uint32_t log_id, uint32_t log_offset, uint8_t blob_type, CloudKeyPtr cloud_key);
	//void writeBlobHead(MSOpenTable *otab, uint64_t offset, uint32_t access_time, uint32_t create_time, uint8_t ref_size, uint16_t head_size, uint64_t blob_size, Md5Digest *checksum, uint16_t metadata_size, uint64_t blob_id, uint32_t auth_code, uint16_t col_index, PBMSEngineRefPtr eng_ref);
	void setRepoFileSize(MSOpenTable *otab, off64_t offset);
	void syncHead(MSRepoFile *fh);
	MSRepoFile *openRepoFile();

	virtual void returnToPool();

	MSRepoFile *getRepoFile();
	void addRepoFile(MSRepoFile *file);
	void removeRepoFile(MSRepoFile *file);
	void returnRepoFile(MSRepoFile *file);

	bool removeRepoFilesNotInUse();								/* Return true if all files have been removed. */
	
	uint16_t getDefaultHeaderSize(uint16_t metadata_size) { return myRepoBlobHeadSize + ((metadata_size)?metadata_size:MS_REPO_MIN_MATADATA)  + myRepoDefRefSize * MS_REPO_MIN_REF_COUNT;}
	off64_t getRepoFileSize();
	size_t getRepoHeadSize();
	size_t getRepoBlobHeadSize();
	CSMutex *getRepoLock(off64_t offset);
	uint32_t getRepoID();
	uint32_t getGarbageLevel();

	uint32_t initBackup();
	bool lockedForBackup();
	void backupCompleted();
	bool isRepoLocked() { return myRepoXLock;}
	void lockRepo(RepoLockState state);
	void unlockRepo(RepoLockState state);
	
	friend class MSRepoFile;

private:
	bool			myRepoXLock;
	/* The read file pool: */
	MSRepoFile		*iFilePool;									/* A list of files currently not in use. THIS LIST DOESN'T COUNT AS A REFERENCE! YUK!!*/
	CSLinkedList	iPoolFiles;									/* A list of all files in this pool */

	CSPath *getRepoFilePath();
	void signalCompactor();

};

#endif
