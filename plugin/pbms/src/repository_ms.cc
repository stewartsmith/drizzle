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
 * 2007-05-25
 *
 * H&G2JCtL
 *
 * Network interface.
 *
 */

#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/session.h>
#include <drizzled/sql_lex.h>
#endif

#include "cslib/CSConfig.h"
#include <inttypes.h>

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSLog.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSHTTPStream.h"
#include "cslib/CSStream.h"

#include "repository_ms.h"
#include "open_table_ms.h"
#include "connection_handler_ms.h"
#include "metadata_ms.h"
#include "parameters_ms.h"
#include "pbmsdaemon_ms.h"

/*
 * ---------------------------------------------------------------
 * REPOSITORY FILE
 */

MSRepoFile::MSRepoFile():
CSFile(),
myRepo(NULL),
isFileInUse(false),
nextFile(NULL),
iNextLink(NULL),
iPrevLink(NULL)
{
}

MSRepoFile::~MSRepoFile()
{
	close();
}

void MSRepoFile::updateGarbage(uint64_t size) 
{
	MSRepoHeadRec	repo_head;
	enter_();

	lock_(myRepo);
	myRepo->myGarbageCount += size;
	CS_SET_DISK_8(repo_head.rh_garbage_count_8, myRepo->myGarbageCount);
	ASSERT(myRepo->myGarbageCount <= myRepo->myRepoFileSize);
	write(&repo_head.rh_garbage_count_8, offsetof(MSRepoHeadRec, rh_garbage_count_8), 8);
	unlock_(myRepo);
	if (!myRepo->myRepoXLock) 
		myRepo->signalCompactor();

	exit_();
}

void MSRepoFile::updateAccess(MSBlobHeadPtr blob, uint64_t rep_offset) 
{
	time_t	now = time(NULL);
	uint32_t count = CS_GET_DISK_4(blob->rb_access_count_4) +1;

	CS_SET_DISK_4(blob->rb_last_access_4, now);
	CS_SET_DISK_4(blob->rb_access_count_4, count);
	write(&blob->rb_last_access_4, rep_offset + offsetof(MSBlobHeadRec, rb_last_access_4), 8);
	myRepo->myLastAccessTime = now;
}

uint64_t MSRepoFile::readBlobChunk(PBMSBlobIDPtr blob_id, uint64_t rep_offset, uint64_t blob_offset, uint64_t buffer_size, char *buffer)
{
	MSBlobHeadRec		blob_head;
	size_t				tfer;
	uint16_t				head_size;
	uint64_t				blob_size;
	uint32_t				ac;
	uint64_t				offset, blob_read =0;

	enter_();

	read(&blob_head, rep_offset, sizeof(MSBlobHeadRec), sizeof(MSBlobHeadRec));
	if (CS_GET_DISK_4(blob_head.rd_magic_4) != MS_BLOB_HEADER_MAGIC)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Invalid BLOB identifier");
		
	blob_size = CS_GET_DISK_6(blob_head.rb_blob_repo_size_6);
	head_size = CS_GET_DISK_2(blob_head.rb_head_size_2);
	ac = CS_GET_DISK_4(blob_head.rb_auth_code_4);
	if (blob_id->bi_auth_code != ac)
		CSException::throwException(CS_CONTEXT, MS_ERR_AUTH_FAILED, "Invalid BLOB identifier");

	offset = rep_offset + blob_offset + head_size;
	
	if (blob_offset > blob_size)
		goto done;
	
	if ((blob_offset + buffer_size) > blob_size)
		buffer_size = blob_size - blob_offset;
		
	while (buffer_size > 0) {
		if (buffer_size <= (uint64_t) (SSIZE_MAX))
			tfer = (size_t) buffer_size;
		else
			tfer = SSIZE_MAX;
			
		read(buffer, offset, tfer, tfer);
		offset += (uint64_t) tfer;
		buffer += (uint64_t) tfer;
		buffer_size -= (uint64_t) tfer;
		blob_read += (uint64_t) tfer;
	}

	/* Only update the access timestamp when reading the first block: */
	if (!blob_offset) 
		updateAccess(&blob_head, rep_offset);
	
done:
	return_(blob_read);
}

void MSRepoFile::writeBlobChunk(PBMSBlobIDPtr blob_id, uint64_t rep_offset, uint64_t blob_offset, uint64_t data_size, char *data)
{
	size_t				tfer;
	off64_t				offset;
	MSBlobHeadRec		blob_head;
	uint16_t				head_size;
	uint64_t				blob_size;
	uint32_t				ac;

	enter_();

	read(&blob_head, rep_offset, sizeof(MSBlobHeadRec), sizeof(MSBlobHeadRec));
	if (CS_GET_DISK_4(blob_head.rd_magic_4) != MS_BLOB_HEADER_MAGIC)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Invalid BLOB identifier");

	blob_size = CS_GET_DISK_6(blob_head.rb_blob_repo_size_6);
	head_size = CS_GET_DISK_2(blob_head.rb_head_size_2);
	ac = CS_GET_DISK_4(blob_head.rb_auth_code_4);
	if (blob_id->bi_auth_code != ac)
		CSException::throwException(CS_CONTEXT, MS_ERR_AUTH_FAILED, "Invalid BLOB identifier");

	if ((blob_offset + data_size) > blob_size) 
		CSException::throwException(CS_CONTEXT, MS_ERR_AUTH_FAILED, "Invalid BLOB write size or offset");

	offset = (uint64_t) head_size + rep_offset + blob_offset;
		
	while (data_size > 0) {
		if (data_size <= (uint64_t) (SSIZE_MAX))
			tfer = (size_t) data_size;
		else
			tfer = SSIZE_MAX;
			
		write(data, offset, tfer);
		data += (uint64_t) tfer;
		offset += (uint64_t) tfer;
		data_size -= (uint64_t) tfer;
	}

	exit_();
}

void MSRepoFile::sendBlob(MSOpenTable *otab, uint64_t offset, uint64_t req_offset, uint64_t req_size, uint32_t auth_code, bool with_auth_code, bool info_only, CSHTTPOutputStream *stream)
{
	MSConnectionHandler	*me;
	size_t				tfer;
	off64_t				start_offset = offset;
	MSBlobHeadRec		blob_head;
	uint8_t				storage_type;
	uint16_t				head_size, meta_size;
	uint64_t				blob_data_size, local_blob_size, meta_offset;
	uint32_t				ac;
	char				num_str[CS_WIDTH_INT_64];
	bool				redirect = false;
	

	enter_();
	me = (MSConnectionHandler *) self;

	read(&blob_head, start_offset, sizeof(MSBlobHeadRec), sizeof(MSBlobHeadRec));
	local_blob_size = CS_GET_DISK_6(blob_head.rb_blob_repo_size_6);	// This is the size of the BLOB data in the repository. Can be 0 if the BLOB is stored some where else.
	blob_data_size = CS_GET_DISK_6(blob_head.rb_blob_data_size_6);// This is the actual size of the BLOB.
	head_size = CS_GET_DISK_2(blob_head.rb_head_size_2);
	meta_size = CS_GET_DISK_2(blob_head.rb_mdata_size_2);
	meta_offset = start_offset + CS_GET_DISK_2(blob_head.rb_mdata_offset_2);
	ac = CS_GET_DISK_4(blob_head.rb_auth_code_4);
	if ((with_auth_code && auth_code != ac) || (CS_GET_DISK_4(blob_head.rd_magic_4) != MS_BLOB_HEADER_MAGIC))
		CSException::throwException(CS_CONTEXT, MS_ERR_AUTH_FAILED, "Invalid BLOB identifier");

	storage_type = CS_GET_DISK_1(blob_head.rb_storage_type_1);
	
	if ((!info_only) && BLOB_IN_CLOUD(storage_type)) {
		CSString *redirect_url = NULL;
		CloudKeyRec key;
		getBlobKey(&blob_head, &key);
		redirect_url = 	otab->getDB()->myBlobCloud->cl_getDataURL(&key);	
		push_(redirect_url);
		stream->setStatus(301);
		stream->addHeader("Location", redirect_url->getCString());
		release_(redirect_url);
		redirect = true;
	} else
		stream->setStatus(200);

	if (storage_type == MS_STANDARD_STORAGE) {
		char hex_checksum[33];
		cs_bin_to_hex(33, hex_checksum, 16, blob_head.rb_blob_checksum_md5d.val);
		stream->addHeader(MS_CHECKSUM_TAG, hex_checksum);
	}
	
	snprintf(num_str, CS_WIDTH_INT_64, "%"PRIu64"", blob_data_size);
	stream->addHeader(MS_BLOB_SIZE, num_str);
		
	snprintf(num_str, CS_WIDTH_INT_64, "%"PRIu32"", CS_GET_DISK_4(blob_head.rb_last_access_4));
	stream->addHeader(MS_LAST_ACCESS, num_str);

	snprintf(num_str, CS_WIDTH_INT_64, "%"PRIu32"", CS_GET_DISK_4(blob_head.rb_access_count_4));
	stream->addHeader(MS_ACCESS_COUNT, num_str);
	
	snprintf(num_str, CS_WIDTH_INT_64, "%"PRIu32"", CS_GET_DISK_4(blob_head.rb_create_time_4));
	stream->addHeader(MS_CREATION_TIME, num_str);
	
	snprintf(num_str, CS_WIDTH_INT_64, "%"PRIu32"", storage_type);
	stream->addHeader(MS_BLOB_TYPE, num_str);
	
	
	// Add the meta data headers.
	if (meta_size) {
		MetaData metadata;
		char *name, *value;
		
		read(otab->myOTBuffer, meta_offset, meta_size, meta_size);
		metadata.use_data(otab->myOTBuffer, meta_size);
		while ((name = metadata.findNext(&value))) {
			stream->addHeader(name, value);
		}
		
	}
		
  offset += (uint64_t) head_size + req_offset;
  local_blob_size -= req_offset;
  if (local_blob_size > req_size)
    local_blob_size = req_size;

	stream->setContentLength((redirect || info_only)?0:local_blob_size);
	stream->writeHead();
	me->replyPending = false;

	if ((!redirect) && !info_only) {
    
		while (local_blob_size > 0) {
			if (local_blob_size <=  MS_OT_BUFFER_SIZE)
				tfer = (size_t) local_blob_size;
			else
				tfer = MS_OT_BUFFER_SIZE;
			read(otab->myOTBuffer, offset, tfer, tfer);
			stream->write(otab->myOTBuffer, tfer);
			offset += (uint64_t) tfer;
			local_blob_size -= (uint64_t) tfer;
		}
	}
	stream->flush();

	if (!info_only) {
		// Should the time stamp be updated if only the BLOB info was requested?
		/* Update the access timestamp: */
		updateAccess(&blob_head, start_offset);
	}
	
	exit_();
}

void MSRepoFile::update_blob_header(MSOpenTable *otab, uint64_t offset, uint64_t blob_size, uint16_t head_size, uint16_t new_head_size)
{
	uint16_t	w_offset =  offsetof(MSBlobHeadRec, rb_ref_count_2);
	MSRepoPointersRec	ptr;
	enter_();

	ptr.rp_chars = otab->myOTBuffer;
	CS_SET_DISK_4(ptr.rp_head->rb_mod_time_4, time(NULL));
	
	if (head_size == new_head_size) {
		w_offset =  offsetof(MSBlobHeadRec, rb_ref_count_2);
		write(otab->myOTBuffer + w_offset, offset + w_offset, head_size - w_offset);
	} else {
		/* Copy to a new space, free the old: */
		off64_t			dst_offset;
		CSStringBuffer	*buffer;
		uint16_t ref_count, ref_size;
		uint32_t tab_id;
		uint64_t blob_id;
		
		myRepo->myRepoDatabase->openWriteRepo(otab);
		dst_offset = otab->myWriteRepo->myRepoFileSize;

		/* Write the header. */
		otab->myWriteRepoFile->write(otab->myOTBuffer, dst_offset, new_head_size);

		/* We have an engine reference, copy the BLOB over: */
		new_(buffer, CSStringBuffer());
		push_(buffer);
		buffer->setLength(MS_COMPACTOR_BUFFER_SIZE);
		CSFile::transfer(RETAIN(otab->myWriteRepoFile), dst_offset + new_head_size, RETAIN(this), offset + head_size, blob_size, buffer->getBuffer(0), MS_COMPACTOR_BUFFER_SIZE);
		release_(buffer);

#ifdef HAVE_ALIAS_SUPPORT
		/* Update the BLOB alias if required. */
		
		if (CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2)) {
			uint32_t alias_hash = CS_GET_DISK_4(ptr.rp_head->rb_alias_hash_4);
			myRepo->myRepoDatabase->moveBlobAlias(myRepo->myRepoID, offset, alias_hash, myRepo->myRepoID, dst_offset);
		}
#endif

		/* Update the references: */
		ref_size = CS_GET_DISK_1(ptr.rp_head->rb_ref_size_1);
		ref_count = CS_GET_DISK_2(ptr.rp_head->rb_ref_count_2);
		ptr.rp_chars += myRepo->myRepoBlobHeadSize;
		
		while (ref_count) {
			switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
				case MS_BLOB_FREE_REF:
					break;
				case MS_BLOB_TABLE_REF:
					tab_id = CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4);
					blob_id = CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6);

					if ((otab->haveTable()) && (otab->getDBTable()->myTableID == tab_id))
						otab->getDBTable()->updateBlobHandle(otab, blob_id, otab->myWriteRepo->myRepoID, dst_offset, new_head_size);
					else {
						MSOpenTable *ref_otab;

						ref_otab = MSTableList::getOpenTableByID(myRepo->myRepoDatabase->myDatabaseID, tab_id);
						frompool_(ref_otab);
						ref_otab->getDBTable()->updateBlobHandle(ref_otab, blob_id, otab->myWriteRepo->myRepoID, dst_offset, new_head_size);
						backtopool_(ref_otab);
					}
					break;
				case MS_BLOB_DELETE_REF:
					break;
				default:
					break;
			}
			ptr.rp_chars += ref_size;
			ref_count--; 
		}

		otab->myWriteRepo->myRepoFileSize += new_head_size + blob_size;

		/* Free the old head: */
		ptr.rp_chars = otab->myOTBuffer;
		if (myRepo->lockedForBackup()) {
			// This is done to tell the backup process that this BLOB was moved
			// after the backup had started and needs to be backed up also. 
			// (The moved BLOB doesn't though because the change took place after the backup had begone.)
			CS_SET_DISK_1(ptr.rp_head->rb_status_1, MS_BLOB_MOVED);
			CS_SET_DISK_4(ptr.rp_head->rb_backup_id_4, myRepo->myRepoDatabase->backupID());
		} else
			CS_SET_DISK_1(ptr.rp_head->rb_status_1, MS_BLOB_DELETED);
		
		write(ptr.rp_chars + MS_BLOB_STAT_OFFS, offset + MS_BLOB_STAT_OFFS, head_size - MS_BLOB_STAT_OFFS);
			
#ifdef DO_NOT_WIPE_BLOB		
		// Why is the BLOB header data being wiped here?
		// The data may be needed for backup.
		ptr.rp_chars += myRepo->myRepoBlobHeadSize;
		memset(ptr.rp_chars, 0, head_size - myRepo->myRepoBlobHeadSize);
		
		w_offset =  offsetof(MSBlobHeadRec, rb_alias_hash_4);
		write(otab->myOTBuffer + w_offset, offset + w_offset, head_size - w_offset);
#endif

		/* Increment the garbage count: */
		updateGarbage(head_size + blob_size);
		
	}
	exit_();
}

void MSRepoFile::referenceBlob(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id, uint32_t auth_code, uint16_t col_index)
{
	CSMutex				*myLock;
	MSRepoPointersRec	ptr;
	uint32_t				size, ref_count;
	size_t				ref_size, read_size;
	MSRepoBlobRefPtr	free_ref = NULL;
	MSRepoBlobRefPtr	free2_ref = NULL;
	MSRepoTableRefPtr	tab_ref = NULL;
	uint16_t				new_head_size;
#ifdef HAVE_ALIAS_SUPPORT
	bool				reset_alias_index = false;
	char				blob_alias[BLOB_ALIAS_LENGTH];
#endif
	uint64_t				blob_size;
	
	enter_();
	/* Lock the BLOB: */
	myLock = &myRepo->myRepoLock[offset % CS_REPO_REC_LOCK_COUNT];
	lock_(myLock);
	/* Read the header: */
	if (head_size > MS_OT_BUFFER_SIZE) {
		CSException::throwAssertion(CS_CONTEXT, "BLOB header overflow");
	}
	
	read_size = read(otab->myOTBuffer, offset, head_size, 0);
	ptr.rp_chars = otab->myOTBuffer;
	if (CS_GET_DISK_4(ptr.rp_head->rd_magic_4) != MS_BLOB_HEADER_MAGIC)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Invalid BLOB identifier");
	if (read_size < myRepo->myRepoBlobHeadSize)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB header incomplete");
	if ( ! IN_USE_BLOB_STATUS(CS_GET_DISK_1(ptr.rp_head->rb_status_1)))
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB has already been deleted");
	if (CS_GET_DISK_4(ptr.rp_bytes + myRepo->myRepoBlobHeadSize - 4) != auth_code)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB data does not match reference");
	/* Assume that what is here is correct: */
	if (head_size != CS_GET_DISK_2(ptr.rp_head->rb_head_size_2)) {
		head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
		if (head_size > MS_OT_BUFFER_SIZE) { // Could happen if the header was creatd with a different version of PBMS.
			CSException::throwAssertion(CS_CONTEXT, "BLOB header overflow");
		}
		read_size = read(otab->myOTBuffer, offset, head_size, myRepo->myRepoBlobHeadSize);
	}
	head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
	blob_size = CS_GET_DISK_6(ptr.rp_head->rb_blob_repo_size_6);
	if (read_size < head_size) {
		/* This should not happen, because the file has been recovered,
		 * which should have already adjusted the head and blob
		 * size.
		 * If this happens then the file must have been truncated an the BLOB has been
		 * lost so we set the blob size to zero.
		 */
		head_size = read_size;
		blob_size = 0;
		
	}
	ref_size = CS_GET_DISK_1(ptr.rp_head->rb_ref_size_1);
	ref_count = CS_GET_DISK_2(ptr.rp_head->rb_ref_count_2);
	
#ifdef HAVE_ALIAS_SUPPORT
	if (CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2)) {
		reset_alias_index = true;
		strcpy(blob_alias, otab->myOTBuffer + CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2));
	}
#endif
	
	size = head_size - myRepo->myRepoBlobHeadSize;
	if (size > ref_size * ref_count)
		size = ref_size * ref_count;
	CS_SET_DISK_4(ptr.rp_head->rb_last_ref_4, (uint32_t) time(NULL)); // Set the reference time
	CS_SET_DISK_1(ptr.rp_head->rb_status_1, MS_BLOB_REFERENCED); 
	ptr.rp_chars += myRepo->myRepoBlobHeadSize;
	while (size >= ref_size) {
		switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
			case MS_BLOB_FREE_REF:
				if (!free_ref)
					free_ref = ptr.rp_blob_ref;
				else if (!free2_ref)
					free2_ref = ptr.rp_blob_ref;
				break;
			case MS_BLOB_TABLE_REF:
#ifdef HAVE_ALIAS_SUPPORT
				reset_alias_index = false; // No need to reset the index if the BLOB is already referenced. (We don't care what table references it.)
#endif
				if (CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4) == tab_id &&
					CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6) == blob_id)
					tab_ref = ptr.rp_tab_ref;
				break;
			case MS_BLOB_DELETE_REF: {
				uint32_t tab_index;

				tab_index = CS_GET_DISK_2(ptr.rp_temp_ref->tp_del_ref_2);
				if (tab_index && tab_index < ref_count) {
					MSRepoTableRefPtr tr;

					tab_index--;
					tr = (MSRepoTableRefPtr) (otab->myOTBuffer + myRepo->getRepoBlobHeadSize() + tab_index * ref_size);
					if (CS_GET_DISK_4(tr->tr_table_id_4) == tab_id &&
						CS_GET_DISK_6(tr->tr_blob_id_6) == blob_id) {
						CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
						if (free_ref)
							free2_ref = free_ref;
						free_ref = ptr.rp_blob_ref;
					}
				}
				else if (tab_index == INVALID_INDEX) {
					/* The is a reference from the temporary log only!! */
					if (free_ref)
						free2_ref = free_ref;
					free_ref = ptr.rp_blob_ref;
				}
				break;
			}
			default: { // Must be a blob REF, check that the BLOB reference doesn't already exist.
				uint32_t tab_index;
				tab_index = CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2);
				
				if (tab_index && tab_index < ref_count) {
					MSRepoTableRefPtr tr;

					tab_index--;
					tr = (MSRepoTableRefPtr) (otab->myOTBuffer + myRepo->getRepoBlobHeadSize() + tab_index * ref_size);
					if (CS_GET_DISK_4(tr->tr_table_id_4) == tab_id &&
						CS_GET_DISK_6(tr->tr_blob_id_6) == blob_id) {
						if (COMMIT_MASK(CS_GET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8)) ==  blob_ref_id) {
							char message[100];
							snprintf(message, 100, "Duplicate BLOB reference: db_id: %"PRIu32", tab_id:%"PRIu32", blob_ref_id: %"PRIu64"\n", myRepo->myRepoDatabase->myDatabaseID, tab_id, blob_ref_id);
							/* The reference already exists so there is nothing to do... */
							self->myException.log(self, message);
							goto done;
						}
					}
				}
				break;
			}
		}
		ptr.rp_chars += ref_size;
		size -= ref_size;
	}

	// A BLOB reference needs to be added and if there is not
	// already a table reference then a table reference must be added
	// also.
	if (!free_ref || (!tab_ref && !free2_ref)) {
		size_t new_refs = (tab_ref)?1:2;
		ptr.rp_chars = otab->myOTBuffer;
		size_t sp = MS_VAR_SPACE(ptr.rp_head);
		
		if (sp > (new_refs * CS_GET_DISK_1(ptr.rp_head->rb_ref_size_1))) {
			sp = MS_MIN_BLOB_HEAD_SIZE;
		}
		
		if (MS_CAN_ADD_REFS(ptr.rp_head, new_refs)) {
			new_head_size = head_size;

		} else { // The header must be grown
			size_t new_size, max_refs;

			if (ref_count < 2)
				max_refs = 4;
			else if (ref_count > 32)
				max_refs = ref_count + 32;
			else
				max_refs = 2 * ref_count;
				
			if (max_refs > (MS_OT_BUFFER_SIZE/ref_size))
				max_refs = (MS_OT_BUFFER_SIZE/ref_size);
				
			if (max_refs < (ref_count + new_refs))
				CSException::throwAssertion(CS_CONTEXT, "BLOB reference header overflow");

			new_size = head_size + ref_size * max_refs;

			//Shift the metadata in the header
			if (CS_GET_DISK_2(ptr.rp_head->rb_mdata_size_2)) {
				uint16_t  mdata_size, mdata_offset, alias_offset, shift;
				
				shift = new_size - head_size;
				mdata_size = CS_GET_DISK_2(ptr.rp_head->rb_mdata_size_2);
				mdata_offset = CS_GET_DISK_2(ptr.rp_head->rb_mdata_offset_2);
				alias_offset = CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2);
				
				memmove(ptr.rp_chars + mdata_offset + shift, ptr.rp_chars + mdata_offset, shift);
				memset(ptr.rp_chars + mdata_offset, 0, shift);
				mdata_offset += shift;
				alias_offset += shift;
				
				CS_SET_DISK_2(ptr.rp_head->rb_mdata_offset_2, mdata_offset);
				CS_SET_DISK_2(ptr.rp_head->rb_alias_offset_2, alias_offset);
				
			} else
				memset(ptr.rp_chars + head_size, 0, new_size - head_size);
			
			new_head_size = new_size;
		}
		CS_SET_DISK_2(ptr.rp_head->rb_head_size_2, new_head_size);
		CS_SET_DISK_2(ptr.rp_head->rb_ref_count_2, ref_count + new_refs);
		ptr.rp_chars += myRepo->myRepoBlobHeadSize + ref_count * ref_size;
		
		if (!free_ref) {
			free_ref = ptr.rp_blob_ref;
			memset(free_ref, 0, ref_size);
			ptr.rp_chars += ref_size;
		}

		if (!tab_ref) {
			free2_ref = ptr.rp_blob_ref;
			memset(free2_ref, 0, ref_size);	
		}
		
		ref_count += new_refs;
	}
	else
		new_head_size = head_size;

	if (!tab_ref) {
		tab_ref = (MSRepoTableRefPtr) free2_ref;

		CS_SET_DISK_2(tab_ref->rr_type_2, MS_BLOB_TABLE_REF);
		CS_SET_DISK_4(tab_ref->tr_table_id_4, tab_id);
		CS_SET_DISK_6(tab_ref->tr_blob_id_6, blob_id);
	}

	size_t tab_idx;

	tab_idx = (((char *) tab_ref - otab->myOTBuffer) - myRepo->myRepoBlobHeadSize) / ref_size;

	CS_SET_DISK_2(free_ref->er_table_2, tab_idx+1);
	CS_SET_DISK_2(free_ref->er_col_index_2, col_index);
	CS_SET_DISK_8(free_ref->er_blob_ref_id_8, UNCOMMITTED(blob_ref_id));

	update_blob_header(otab, offset, blob_size, head_size, new_head_size);
#ifdef HAVE_ALIAS_SUPPORT
	if (reset_alias_index) 
		myRepo->myRepoDatabase->registerBlobAlias(myRepo->myRepoID, offset, blob_alias);
#endif

done:
	
	unlock_(myLock);
	exit_();
}

void MSRepoFile::setBlobMetaData(MSOpenTable *otab, uint64_t offset, const char *meta_data, uint16_t meta_data_len, bool reset_alias, const char  *alias)
{
	CSMutex				*mylock;
	MSRepoPointersRec	ptr;
	size_t				read_size;
	uint16_t				new_head_size;
	uint64_t				blob_size;
	uint16_t				head_size, mdata_size, mdata_offset, alias_offset = 0;
	MSBlobHeadRec		blob;
	
	enter_();
	/* Lock the BLOB: */
	mylock = &myRepo->myRepoLock[offset % CS_REPO_REC_LOCK_COUNT];
	lock_(mylock);

	/* Read the header: */
	if (read(&blob, offset, sizeof(MSBlobHeadRec), 0) < sizeof(MSBlobHeadRec)) {
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB header incomplete");
	}

	head_size = CS_GET_DISK_2(blob.rb_head_size_2);

	if (head_size > MS_OT_BUFFER_SIZE) {
		CSException::throwAssertion(CS_CONTEXT, "BLOB header overflow");
	}
	
	read_size = read(otab->myOTBuffer, offset, head_size, 0);
	ptr.rp_chars = otab->myOTBuffer;
	if (CS_GET_DISK_4(ptr.rp_head->rd_magic_4) != MS_BLOB_HEADER_MAGIC)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Invalid BLOB identifier");
	if (read_size < myRepo->myRepoBlobHeadSize)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB header incomplete");
	if (! IN_USE_BLOB_STATUS(CS_GET_DISK_1(ptr.rp_head->rb_status_1)))
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB has already been deleted");


	blob_size = CS_GET_DISK_6(ptr.rp_head->rb_blob_repo_size_6);
	if (read_size < head_size) {
		/* This should not happen, because the file has been recovered,
		 * which should have already adjusted the head and blob
		 * size.
		 * If this happens then the file must have been truncated an the BLOB has been
		 * lost so we set the blob size to zero.
		 */
		head_size = read_size;
		blob_size = 0; 		
	}
	mdata_size = CS_GET_DISK_2(ptr.rp_head->rb_mdata_size_2);

	if ((meta_data_len < mdata_size) || MS_CAN_ADD_MDATA(ptr.rp_head, meta_data_len - mdata_size)) 
		new_head_size = head_size;
	else { // The header must be grown

		new_head_size = head_size + meta_data_len - mdata_size;
		if (new_head_size > MS_OT_BUFFER_SIZE)
			CSException::throwAssertion(CS_CONTEXT, "BLOB reference header overflow");

		memset(ptr.rp_chars + head_size, 0, new_head_size - head_size);
		CS_SET_DISK_2(ptr.rp_head->rb_head_size_2, new_head_size);
		
	}	
				
	// Meta data is placed at the end of the header.
	if (meta_data_len)
		mdata_offset = new_head_size - meta_data_len;
	else
		mdata_offset = 0;
	mdata_size	= meta_data_len;
		
	
	CS_SET_DISK_2(ptr.rp_head->rb_mdata_size_2, mdata_size);
	CS_SET_DISK_2(ptr.rp_head->rb_mdata_offset_2, mdata_offset);	
#ifdef HAVE_ALIAS_SUPPORT
	uint32_t alias_hash = INVALID_ALIAS_HASH;
	if (alias) {
		alias_hash = CS_GET_DISK_4(ptr.rp_head->rb_alias_hash_4);
		alias_offset = CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2);
		if (reset_alias) {
			if (alias_offset)
				alias_hash = myRepo->myRepoDatabase->updateBlobAlias(myRepo->myRepoID, offset, alias_hash, alias);
			else {
				alias_hash = myRepo->myRepoDatabase->registerBlobAlias(myRepo->myRepoID, offset, alias);
			}
		}
		alias_offset = mdata_offset + (alias - meta_data);
		
	} else if (reset_alias && CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2)) {
		alias_offset = CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2);
		myRepo->myRepoDatabase->deleteBlobAlias(myRepo->myRepoID, offset, CS_GET_DISK_4(ptr.rp_head->rb_alias_hash_4));
		alias_offset = 0;
	}
#else
	uint32_t alias_hash = ((uint32_t)-1);
	if (alias || reset_alias) {
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_IMPLEMENTED, "No BLOB alias support.");
	}
#endif

	CS_SET_DISK_2(ptr.rp_head->rb_alias_offset_2, alias_offset);
	CS_SET_DISK_4(ptr.rp_head->rb_alias_hash_4, alias_hash);
	
	memcpy(ptr.rp_chars + mdata_offset, meta_data, meta_data_len);
		
	update_blob_header(otab, offset, blob_size, head_size, new_head_size);
		
	unlock_(mylock);
	exit_();

}


void MSRepoFile::releaseBlob(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id, uint32_t auth_code)
{
	CSMutex				*mylock;
	MSRepoPointersRec	ptr;
	uint32_t			table_ref_count = 0;
	uint32_t			size;
	size_t				ref_size, ref_count, read_size;
	MSRepoTempRefPtr	temp_ref = NULL;
	uint16_t			tab_index = 0;
	MSRepoTableRefPtr	tab_ref;
	uint16_t			alias_offset;
	uint32_t			alias_hash;

	enter_();
	/* Lock the BLOB: */
	mylock = &myRepo->myRepoLock[offset % CS_REPO_REC_LOCK_COUNT];
	lock_(mylock);
	/* Read the header: */
	ASSERT(head_size <= MS_OT_BUFFER_SIZE);
	read_size = read(otab->myOTBuffer, offset, head_size, 0);
	ptr.rp_chars = otab->myOTBuffer;
	if (CS_GET_DISK_4(ptr.rp_head->rd_magic_4) != MS_BLOB_HEADER_MAGIC)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Invalid BLOB identifier");
	if (read_size < myRepo->myRepoBlobHeadSize) {
		removeBlob(otab, tab_id, blob_id, offset, auth_code);
		goto exit;
	}
	if ((! IN_USE_BLOB_STATUS(CS_GET_DISK_1(ptr.rp_head->rb_status_1))) ||
		CS_GET_DISK_4(ptr.rp_bytes + myRepo->myRepoBlobHeadSize - 4) != auth_code) {
		removeBlob(otab, tab_id, blob_id, offset, auth_code);
		goto exit;
	}
	
	/* Assume that what is here is correct: */
	if (head_size != CS_GET_DISK_2(ptr.rp_head->rb_head_size_2)) {
		head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
		read_size = read(otab->myOTBuffer, offset, head_size, myRepo->myRepoBlobHeadSize);
	}
	head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
	if (read_size < head_size) {
		/* This should not happen, because the file has been recovered,
		 * which should have already adjusted the head and blob
		 * size.
		 * If this happens then the file must have been truncated an the BLOB has been
		 * lost so we set the blob size to zero.
		 */
		head_size = read_size;
	}
	ref_size = CS_GET_DISK_1(ptr.rp_head->rb_ref_size_1);
	ref_count = CS_GET_DISK_2(ptr.rp_head->rb_ref_count_2);

	alias_offset = CS_GET_DISK_2(ptr.rp_head->rb_alias_offset_2);
	alias_hash = CS_GET_DISK_4(ptr.rp_head->rb_alias_hash_4);

	size = head_size - myRepo->myRepoBlobHeadSize;
	if (size > ref_size * ref_count)
		size = ref_size * ref_count;
	ptr.rp_chars += myRepo->myRepoBlobHeadSize;
	while (size >= ref_size) {
		switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
			case MS_BLOB_FREE_REF:
			case MS_BLOB_TABLE_REF:
				break;
			case MS_BLOB_DELETE_REF: {
				uint32_t tabi;

				tabi = CS_GET_DISK_2(ptr.rp_temp_ref->tp_del_ref_2);
				if (tabi && tabi < ref_count) {
					tabi--;
					tab_ref = (MSRepoTableRefPtr) (otab->myOTBuffer + myRepo->myRepoBlobHeadSize + tabi * ref_size);
					if (CS_GET_DISK_4(tab_ref->tr_table_id_4) == tab_id &&
						CS_GET_DISK_6(tab_ref->tr_blob_id_6) == blob_id) {
						/* This is an old free, take it out. */
						// Barry: What happens to the record in the temp log associated with this ref
						// that is waiting to free the BLOB?
						// Answer: It will find that there is MS_BLOB_DELETE_REF record with the BLOB
						// or if there is one it will be for a different free in a different temp log
						// or with a different temp log offset.
						CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
					}
				}
				break;
			}
			default:  // Must be a blob REF
				tab_ref = (MSRepoTableRefPtr) (otab->myOTBuffer + myRepo->myRepoBlobHeadSize + (CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2)-1) * ref_size);
				if (CS_GET_DISK_4(tab_ref->tr_table_id_4) == tab_id &&
					CS_GET_DISK_6(tab_ref->tr_blob_id_6) == blob_id) {
					if (COMMIT_MASK(CS_GET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8)) ==  blob_ref_id) {
						/* Found the reference, remove it... */
						tab_index = CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2)-1;
						temp_ref = ptr.rp_temp_ref;
						//temp_ref = (MSRepoTempRefPtr) tab_ref; // Set temp ref to the table ref so that it will be removed if there are no more references to it.
						CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);						
					}
					else
						table_ref_count++;
				}
				break;
		}
		ptr.rp_chars += ref_size;
		size -= ref_size;
	}

	// If the refernce was found and there are no 
	// table references then the BLOB can be scheduled for deletion.
	if ((!table_ref_count) && temp_ref) {
		uint32_t	log_id;
		uint32_t log_offset;
		uint32_t temp_time;
#ifdef HAVE_ALIAS_SUPPORT
		MSDiskAliasRec aliasDiskRec;
		MSDiskAliasPtr aliasDiskPtr = NULL;
		
		if (alias_offset) {
			CS_SET_DISK_4(aliasDiskRec.ar_repo_id_4, myRepo->myRepoID);	
			CS_SET_DISK_8(aliasDiskRec.ar_offset_8, offset);	
			CS_SET_DISK_4(aliasDiskRec.ar_hash_4, alias_hash);
			aliasDiskPtr = &aliasDiskRec;
		}
		
		myRepo->myRepoDatabase->queueForDeletion(otab, MS_TL_BLOB_REF, tab_id, blob_id, auth_code, &log_id, &log_offset, &temp_time, aliasDiskPtr);
#else
		myRepo->myRepoDatabase->queueForDeletion(otab, MS_TL_BLOB_REF, tab_id, blob_id, auth_code, &log_id, &log_offset, &temp_time);
#endif
		myRepo->myLastTempTime = temp_time;
		CS_SET_DISK_2(temp_ref->rr_type_2, MS_BLOB_DELETE_REF);
		CS_SET_DISK_2(temp_ref->tp_del_ref_2, tab_index+1);
		CS_SET_DISK_4(temp_ref->tp_log_id_4, log_id);
		CS_SET_DISK_4(temp_ref->tp_offset_4, log_offset);
		
		CS_SET_DISK_1(ptr.rp_head->rb_status_1, MS_BLOB_ALLOCATED); // The BLOB is allocated but no longer referenced
	}
	if (temp_ref) {
		/* The reason I do not write the header of the header, is because
		 * I want to handle the rb_last_access_4 being set at the
		 * same time!
		 */
		write(otab->myOTBuffer + MS_BLOB_STAT_OFFS, offset + MS_BLOB_STAT_OFFS, head_size - MS_BLOB_STAT_OFFS);
	} else if (PBMSDaemon::isDaemonState(PBMSDaemon::DaemonStartUp) == false) {
		char message[100];
		snprintf(message, 100, "BLOB reference not found: db_id: %"PRIu32", tab_id:%"PRIu32", blob_ref_id: %"PRIu64"\n", myRepo->myRepoDatabase->myDatabaseID, tab_id, blob_ref_id);
		/* The reference already exists so there is nothing to do... */
		self->myException.log(self, message);
	}

	exit:
	unlock_(mylock);
	exit_();
}

void MSRepoFile::commitBlob(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id, uint32_t auth_code)
{
	CSMutex				*mylock;
	MSRepoPointersRec	ptr;
	uint32_t				size;
	size_t				ref_size, ref_count, read_size;
	MSRepoTableRefPtr	tab_ref;

	enter_();
	/* Lock the BLOB: */
	mylock = &myRepo->myRepoLock[offset % CS_REPO_REC_LOCK_COUNT];
	lock_(mylock);
	/* Read the header: */
	ASSERT(head_size <= MS_OT_BUFFER_SIZE);
	read_size = read(otab->myOTBuffer, offset, head_size, 0);
	ptr.rp_chars = otab->myOTBuffer;
	if (CS_GET_DISK_4(ptr.rp_head->rd_magic_4) != MS_BLOB_HEADER_MAGIC)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Invalid BLOB identifier");


	if (read_size < myRepo->myRepoBlobHeadSize)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB header incomplete");
	if ( ! IN_USE_BLOB_STATUS(CS_GET_DISK_1(ptr.rp_head->rb_status_1)))
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB has already been deleted");
	if (auth_code && CS_GET_DISK_4(ptr.rp_bytes + myRepo->myRepoBlobHeadSize - 4) != auth_code)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB data does not match reference");

	
	/* Assume that what is here is correct: */
	if (head_size != CS_GET_DISK_2(ptr.rp_head->rb_head_size_2)) {
		head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
		read_size = read(otab->myOTBuffer, offset, head_size, myRepo->myRepoBlobHeadSize);
	}
	
	head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
	if (read_size < head_size) {
		/* This should not happen, because the file has been recovered,
		 * which should have already adjusted the head and blob
		 * size.
		 * If this happens then the file must have been truncated an the BLOB has been
		 * lost so we set the blob size to zero.
		 */
		head_size = read_size;
	}
	ref_size = CS_GET_DISK_1(ptr.rp_head->rb_ref_size_1);
	ref_count = CS_GET_DISK_2(ptr.rp_head->rb_ref_count_2);

	size = head_size - myRepo->myRepoBlobHeadSize;
	if (size > ref_size * ref_count)
		size = ref_size * ref_count;
	ptr.rp_chars += myRepo->myRepoBlobHeadSize;
	while (size >= ref_size) {
		switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
			case MS_BLOB_FREE_REF:
			case MS_BLOB_TABLE_REF:
				break;
			case MS_BLOB_DELETE_REF: {
				break;
			}
			default:  // Must be a blob REF
				tab_ref = (MSRepoTableRefPtr) (otab->myOTBuffer + myRepo->myRepoBlobHeadSize + (CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2)-1) * ref_size);
				if (CS_GET_DISK_4(tab_ref->tr_table_id_4) == tab_id &&
					CS_GET_DISK_6(tab_ref->tr_blob_id_6) == blob_id) {
					uint64_t ref_id = CS_GET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8);
					if (COMMIT_MASK(ref_id) ==  blob_ref_id) {
						/* Found the reference, mark it as committed... */
						CS_SET_DISK_8(ptr.rp_blob_ref->er_blob_ref_id_8, blob_ref_id);
						offset += 	(ptr.rp_chars - otab->myOTBuffer) + offsetof(MSRepoBlobRefRec, er_blob_ref_id_8);
						write(&(ptr.rp_blob_ref->er_blob_ref_id_8), offset, 8);					
						goto exit;
					}
				}
				break;
		}
		ptr.rp_chars += ref_size;
		size -= ref_size;
	}

	if (PBMSDaemon::isDaemonState(PBMSDaemon::DaemonStartUp) == false) {
		char message[100];
		snprintf(message, 100, "BLOB reference not found: db_id: %"PRIu32", tab_id:%"PRIu32", blob_ref_id: %"PRIu64"\n", myRepo->myRepoDatabase->myDatabaseID, tab_id, blob_ref_id);
		self->myException.log(self, message);
	}
	
	exit:
	unlock_(mylock);
	exit_();
}

void MSRepoFile::realFreeBlob(MSOpenTable *otab, char *buffer, uint32_t auth_code, uint64_t offset, uint16_t head_size, uint64_t blob_size, size_t ref_size)
{
	uint32_t				tab_id;
	uint64_t				blob_id;
	size_t				size;
	MSRepoPointersRec	ptr;
	enter_();
	
	ptr.rp_chars = buffer;
	
	if (BLOB_IN_CLOUD(CS_GET_DISK_1(ptr.rp_head->rb_storage_type_1))) {
		CloudKeyRec key;
		getBlobKey(ptr.rp_head, &key);
		if (!myRepo->myRepoDatabase->myBlobCloud)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Deleting cloud BLOB without cloud.");
			
		myRepo->myRepoDatabase->myBlobCloud->cl_deleteData(&key); 
	}
		
#ifdef HAVE_ALIAS_SUPPORT
	uint32_t				alias_hash;
	alias_hash = CS_GET_DISK_4(ptr.rp_head->rb_alias_hash_4);
	if (alias_hash != INVALID_ALIAS_HASH)
		myRepo->myRepoDatabase->deleteBlobAlias(myRepo->myRepoID, offset, alias_hash);
#endif

	// Assuming the BLOB is still locked:
	CS_SET_DISK_1(ptr.rp_head->rb_status_1, MS_BLOB_DELETED);
	write(ptr.rp_chars + MS_BLOB_STAT_OFFS, offset + MS_BLOB_STAT_OFFS, head_size - MS_BLOB_STAT_OFFS);

	/* Update garbage count: */
	updateGarbage(head_size + blob_size);

	/* Remove all table references (should not be any)! */
	size = head_size - myRepo->myRepoBlobHeadSize;
	ptr.rp_chars += myRepo->myRepoBlobHeadSize;
	while (size >= ref_size) {
		if (CS_GET_DISK_2(ptr.rp_ref->rr_type_2) == MS_BLOB_TABLE_REF) {
			tab_id = CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4);
			blob_id = CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6);				
			removeBlob(otab, tab_id, blob_id, offset, auth_code);
		}
		ptr.rp_chars += ref_size;
		size -= ref_size;
	}
	exit_();
}

/* This function will free the BLOB reference, if the record is invalid. */
void MSRepoFile::freeTableReference(MSOpenTable *otab, uint64_t offset, uint16_t head_size, uint32_t tab_id, uint64_t blob_id, uint32_t auth_code)
{
	CSMutex				*mylock;
	MSRepoPointersRec	ptr;
	uint32_t				blob_ref_count = 0;
	uint32_t				table_ref_count = 0;
	bool				modified = false;
	uint32_t				size;
	size_t				ref_size, ref_count, read_size;
	MSRepoTableRefPtr	tab_ref = NULL;
	uint64_t				blob_size;

	enter_();
	/* Lock the BLOB: */
	mylock = &myRepo->myRepoLock[offset % CS_REPO_REC_LOCK_COUNT];
	lock_(mylock);
	/* Read the header: */
	ASSERT(head_size <= MS_OT_BUFFER_SIZE);
	read_size = read(otab->myOTBuffer, offset, head_size, 0);
	ptr.rp_chars = otab->myOTBuffer;
	if (CS_GET_DISK_4(ptr.rp_head->rd_magic_4) != MS_BLOB_HEADER_MAGIC)
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Invalid BLOB identifier");
	if (read_size < myRepo->myRepoBlobHeadSize) {
		removeBlob(otab, tab_id, blob_id, offset, auth_code);
		goto exit;
	}
	if ((! IN_USE_BLOB_STATUS(CS_GET_DISK_1(ptr.rp_head->rb_status_1))) ||
		CS_GET_DISK_4(ptr.rp_bytes + myRepo->myRepoBlobHeadSize - 4) != auth_code) {
		removeBlob(otab, tab_id, blob_id, offset, auth_code);
		goto exit;
	}
	
	/* Assume that what is here is correct: */
	if (head_size != CS_GET_DISK_2(ptr.rp_head->rb_head_size_2)) {
		head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
		read_size = read(otab->myOTBuffer, offset, head_size, myRepo->myRepoBlobHeadSize);
	}
	head_size = CS_GET_DISK_2(ptr.rp_head->rb_head_size_2);
	blob_size = CS_GET_DISK_6(ptr.rp_head->rb_blob_repo_size_6);
	if (read_size < head_size) {
		/* This should not happen, because the file has been recovered,
		 * which should have already adjusted the head and blob
		 * size.
		 * If this happens then the file must have been truncated an the BLOB has been
		 * lost so we set the blob size to zero.
		 */
		head_size = read_size;
		blob_size = 0; 
		
	}
	ref_size = CS_GET_DISK_1(ptr.rp_head->rb_ref_size_1);
	ref_count = CS_GET_DISK_2(ptr.rp_head->rb_ref_count_2);
	size = head_size - myRepo->myRepoBlobHeadSize;
	if (size > ref_size * ref_count)
		size = ref_size * ref_count;
	ptr.rp_chars += myRepo->myRepoBlobHeadSize;
	while (size >= ref_size) {
		switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
			case MS_BLOB_FREE_REF:
				break;
			case MS_BLOB_TABLE_REF:
				if (CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4) == tab_id &&
					CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6) == blob_id)
					tab_ref = ptr.rp_tab_ref;
				break;
			case MS_BLOB_DELETE_REF:
			break;
			default:
				MSRepoTableRefPtr tr;

				tr = (MSRepoTableRefPtr) (otab->myOTBuffer + myRepo->myRepoBlobHeadSize + (CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2)-1) * ref_size);
				if (CS_GET_DISK_2(tr->rr_type_2) == MS_BLOB_TABLE_REF) {
					/* I am deleting all references of a table. So I am here to
					 * also delete the blob references that refer to the
					 * table reference!!!
					 */
					if (CS_GET_DISK_4(tr->tr_table_id_4) == tab_id && CS_GET_DISK_6(tr->tr_blob_id_6) == blob_id) {
						/* Free the blob reference: */
						CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
						modified = true;
					}
					else
						blob_ref_count++;
				
				}
				break;
		}
		ptr.rp_chars += ref_size;
		size -= ref_size;
	}

	if (!table_ref_count && tab_ref) {
		CS_SET_DISK_2(tab_ref->rr_type_2, MS_BLOB_FREE_REF);
		modified = true;
	}
	
	
	if (!blob_ref_count) {
		realFreeBlob(otab, otab->myOTBuffer, auth_code, offset, head_size, blob_size, ref_size);
	} else if (modified)
		/* The reason I do not write the header of the header, is because
		 * I want to handle the rb_last_access_4 being set at the
		 * same time!
		 */
		write(otab->myOTBuffer + MS_BLOB_STAT_OFFS, offset + MS_BLOB_STAT_OFFS, head_size - MS_BLOB_STAT_OFFS);

	unlock_(mylock);

	if (!table_ref_count || !tab_ref)
		/* Free the table reference, if there are no more
		 * blob references, reference the table reference,
		 * or if the table reference was not found in the
		 * BLOB at all!
		 */
		removeBlob(otab, tab_id, blob_id, offset, auth_code);

	exit_();

	exit:
	unlock_(mylock);
	exit_();
}

void MSRepoFile::checkBlob(CSStringBuffer *buffer, uint64_t offset, uint32_t auth_code, uint32_t temp_log_id, uint32_t temp_log_offset)
{
	CSMutex				*mylock;
	MSBlobHeadRec		blob;
	MSRepoPointersRec	ptr;
	uint32_t				blob_ref_count = 0;
	bool				modified = false;
	uint32_t				size;
	size_t				ref_size, ref_count, read_size;
	uint8_t				status;
	uint16_t				head_size;
	uint64_t				blob_size;
	MSRepoTempRefPtr	my_ref = NULL;
	uint16_t				ref_type = MS_BLOB_FREE_REF;
	enter_();
	
	/* Lock the BLOB: */
	mylock = &myRepo->myRepoLock[offset % CS_REPO_REC_LOCK_COUNT];
	lock_(mylock);

	/* Read the head of the header: */
	if (read(&blob, offset, sizeof(MSBlobHeadRec), 0) < sizeof(MSBlobHeadRec)) 
		goto exit;

	// Because the temp log will be replayed from the start when the server
	// is restarted it is likely that it will have references to BLOBs that
	// no longer exist. So it is not an error if the BLOB ref doesn't point to
	// a valid BLOB. 
	//
	// At some point this should probably be rethought because you cannot
	// tell the diference between a bad ref because of old data and a bad 
	// ref because of a BUG.
	if (CS_GET_DISK_4(blob.rd_magic_4) != MS_BLOB_HEADER_MAGIC) 
		goto exit;
	
	head_size = CS_GET_DISK_2(blob.rb_head_size_2);
	blob_size = CS_GET_DISK_6(blob.rb_blob_repo_size_6);
	ref_size = CS_GET_DISK_1(blob.rb_ref_size_1);
	ref_count = CS_GET_DISK_2(blob.rb_ref_count_2);
	status = CS_GET_DISK_1(blob.rb_status_1);
	if (! IN_USE_BLOB_STATUS(status))
		goto exit;

	/* Read the entire header: */
	buffer->setLength(head_size);
	ptr.rp_chars = buffer->getBuffer(0);
	read_size = read(ptr.rp_chars, offset, head_size, 0);
	if (read_size < myRepo->myRepoBlobHeadSize)
		goto exit;
	if (CS_GET_DISK_4(ptr.rp_bytes + myRepo->myRepoBlobHeadSize - 4) != auth_code)
		goto exit;
	if (read_size < head_size) {
		/* This should not happen, because the file has been recovered,
		 * which should have already adjusted the head and blob
		 * size.
		 * If this happens then the file must have been truncated an the BLOB has been
		 * lost so we set the blob size to zero.
		 */
		head_size = read_size;
		blob_size = 0; 
	}
	size = head_size - myRepo->myRepoBlobHeadSize;
	if (size > ref_size * ref_count)
		size = ref_size * ref_count;

	
	/* Search through all references: */
	ptr.rp_chars += myRepo->myRepoBlobHeadSize;
	while (size >= ref_size) {
		switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
			case MS_BLOB_FREE_REF:
				break;
			case MS_BLOB_TABLE_REF:
				break;
			case MS_BLOB_DELETE_REF:
				if (CS_GET_DISK_4(ptr.rp_temp_ref->tp_log_id_4) == temp_log_id &&
					CS_GET_DISK_4(ptr.rp_temp_ref->tp_offset_4) == temp_log_offset) {
					ref_type = CS_GET_DISK_2(ptr.rp_ref->rr_type_2);
					my_ref = ptr.rp_temp_ref;
					CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
					modified = true;
				}
				break;
			default:
				MSRepoTableRefPtr	tr;
				uint32_t				tabi;

				tabi = CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2);
				if (tabi < ref_count) {
					tr = (MSRepoTableRefPtr) (buffer->getBuffer(0) + myRepo->myRepoBlobHeadSize + (tabi-1) * ref_size);
					if (CS_GET_DISK_2(tr->rr_type_2) == MS_BLOB_TABLE_REF)
						blob_ref_count++;
				}
				break;
		}
		ptr.rp_chars += ref_size;
		size -= ref_size;
	}

	if ((ref_type == (uint16_t)MS_BLOB_DELETE_REF) && !blob_ref_count) {
		realFreeBlob(NULL, buffer->getBuffer(0), auth_code, offset, head_size, blob_size, ref_size);
	}
	
	exit:
	unlock_(mylock);
	exit_();
}

void MSRepoFile::returnToPool()
{
	myRepo->myRepoDatabase->returnRepoFileToPool(this);
}

void MSRepoFile::removeBlob(MSOpenTable *otab, uint32_t tab_id, uint64_t blob_id, uint64_t offset, uint32_t auth_code)
{
	enter_();
	if (otab && otab->getDBTable()->myTableID == tab_id)
		otab->getDBTable()->freeBlobHandle(otab, blob_id, myRepo->myRepoID, offset, auth_code);
	else {
		MSOpenTable *tmp_otab;

		if ((tmp_otab = MSTableList::getOpenTableByID(myRepo->myRepoDatabase->myDatabaseID, tab_id))) {
			frompool_(tmp_otab);
			tmp_otab->getDBTable()->freeBlobHandle(tmp_otab, blob_id, myRepo->myRepoID, offset, auth_code);
			backtopool_(tmp_otab);
		}
	}
	exit_();
}

MSRepoFile *MSRepoFile::newRepoFile(MSRepository *repo, CSPath *path)
{
	MSRepoFile *f;
	
	if (!(f = new MSRepoFile())) {
		path->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	f->myRepo = repo;
	f->myFilePath = path;
	return f;
}

/*
 * ---------------------------------------------------------------
 * REPOSITORY
 */

MSRepository::MSRepository(uint32_t id, MSDatabase *db, off64_t file_size):
CSSharedRefObject(),
myRepoID(id),
myRepoFileSize(file_size),
myRepoLockState(REPO_UNLOCKED),
isRemovingFP(false),
myRepoDatabase(db),
myGarbageCount(0),
myRepoHeadSize(0),
myRepoDefRefSize(0),
myRepoBlobHeadSize(0),
myRecoveryOffset(0),
myLastTempTime(0),
myLastAccessTime(0),
myLastCreateTime(0),
myLastRefTime(0),
mustBeDeleted(false),
myRepoXLock(false),
iFilePool(NULL)
{
}

MSRepository::~MSRepository()
{
	CSPath *path = NULL;

	enter_();
	if (mustBeDeleted) {
		path = getRepoFilePath();
		push_(path);
	}

	isRemovingFP = true;
	removeRepoFilesNotInUse();
	/* With this, I also delete those that are in use!: */
	iPoolFiles.clear();

	if (path) {
		path->removeFile();
		release_(path);
	}
	exit_();
}

void MSRepository::openRepoFileForWriting(MSOpenTable *otab)
{
	if (!otab->myWriteRepoFile)
		otab->myWriteRepoFile = openRepoFile();
}

uint64_t MSRepository::receiveBlob(MSOpenTable *otab, uint16_t head_size, uint64_t blob_size, Md5Digest *checksum, CSInputStream *stream)
{
	off64_t	offset;
	size_t	tfer;

	enter_();
		
	offset = myRepoFileSize;

	offset += head_size;
	
	ASSERT(myRepoDatabase->myBlobType == MS_STANDARD_STORAGE);
	if (stream) {
		CSMd5 md5;
		push_(stream);
		md5.md5_init();
		while (blob_size > 0) {
			if (blob_size <=  MS_OT_BUFFER_SIZE)
				tfer = (size_t) blob_size;
			else
				tfer = MS_OT_BUFFER_SIZE;
			tfer = stream->read(otab->myOTBuffer, tfer);
			if (!tfer)
				CSException::throwOSError(CS_CONTEXT, EPIPE);
			if (checksum) md5.md5_append((const u_char *)(otab->myOTBuffer), tfer);
			otab->myWriteRepoFile->write(otab->myOTBuffer, offset, tfer);
			offset += (uint64_t) tfer;
			blob_size -= (uint64_t) tfer;
		}
		if (checksum) md5.md5_get_digest(checksum);
		release_(stream);
	} else {
		// Write 1 byte to the end to reserver the space.
		otab->myWriteRepoFile->write("x" , offset + blob_size -1, 1);
	}

	return_( myRepoFileSize);
}

// copyBlob() copies the BLOB and its header.
uint64_t MSRepository::copyBlob(MSOpenTable *otab, uint64_t size, CSInputStream *stream)
{
	off64_t	offset = myRepoFileSize;
	size_t	tfer;

	while (size > 0) {
		if (size <= MS_OT_BUFFER_SIZE)
			tfer = (size_t) size;
		else
			tfer = MS_OT_BUFFER_SIZE;
		tfer = stream->read(otab->myOTBuffer, tfer);
		if (!tfer)
			CSException::throwOSError(CS_CONTEXT, EPIPE);
		otab->myWriteRepoFile->write(otab->myOTBuffer, offset, tfer);
		offset += (uint64_t) tfer;
		size -= (uint64_t) tfer;
	}
	
	return myRepoFileSize;
}

void MSRepository::writeBlobHead(MSOpenTable *otab, uint64_t offset, uint8_t ref_size, uint16_t head_size, uint64_t blob_size, Md5Digest *checksum, char *metadata, uint16_t metadata_size, uint64_t blob_id, uint32_t auth_code, uint32_t log_id, uint32_t log_offset, uint8_t blob_type, CloudKeyPtr cloud_key)
{
	MSBlobHeadPtr		blob ;
	MSRepoTableRefPtr	tab_ref;
	MSRepoTempRefPtr	temp_ref;
	time_t				now;
	uint16_t				tab_idx, max_ref_count = (head_size - myRepoBlobHeadSize - metadata_size) / ref_size;
	size_t				size;
	
	if (max_ref_count > MS_REPO_MIN_REF_COUNT)
		max_ref_count = MS_REPO_MIN_REF_COUNT;
		
	ASSERT(max_ref_count > 1);
	
	if (blob_type == MS_CLOUD_STORAGE) 
		now = cloud_key->creation_time;
	else
		now = time(NULL);
	
	blob = (MSBlobHeadPtr) otab->myOTBuffer;
	CS_SET_DISK_4(blob->rb_last_access_4, now);
	CS_SET_DISK_4(blob->rb_mod_time_4, now);
	CS_SET_DISK_4(blob->rb_access_count_4, 0);
	CS_SET_DISK_4(blob->rb_backup_id_4, 0);
	CS_SET_DISK_4(blob->rb_create_time_4, now);
	CS_SET_DISK_4(blob->rd_magic_4, MS_BLOB_HEADER_MAGIC);
	CS_SET_DISK_2(blob->rb_head_size_2, head_size);
	CS_SET_DISK_6(blob->rb_blob_data_size_6, blob_size);
	CS_SET_DISK_1(blob->rb_status_1, MS_BLOB_ALLOCATED);
	CS_SET_DISK_1(blob->rb_ref_size_1, ref_size);
	CS_SET_DISK_2(blob->rb_ref_count_2, max_ref_count);
	CS_SET_DISK_4(blob->rb_last_ref_4, 0);
	CS_SET_DISK_4(otab->myOTBuffer + myRepoBlobHeadSize - 4, auth_code);
	if (checksum)
		memcpy(&(blob->rb_blob_checksum_md5d), checksum, sizeof(Md5Digest));

	CS_SET_DISK_2(blob->rb_mdata_size_2, metadata_size);
	if (metadata_size) {
		uint16_t metadata_offset = head_size - metadata_size;
		
		CS_SET_DISK_2(blob->rb_mdata_offset_2, metadata_offset);
		memcpy(otab->myOTBuffer + metadata_offset, metadata, metadata_size);
		
#ifdef HAVE_ALIAS_SUPPORT
		MetaData md;	
		md.use_data(metadata, metadata_size);
		const char *alias;	
		alias = md.findAlias();
		if (alias) {
			uint32_t alias_hash;
			uint16_t alias_offset = metadata_offset + (uint16_t) (alias - metadata);
			CS_SET_DISK_2(blob->rb_alias_offset_2, alias_offset);
			alias_hash = myRepoDatabase->registerBlobAlias(myRepoID, offset, alias);
			CS_SET_DISK_4(blob->rb_alias_hash_4, alias_hash);
		} else {
			CS_SET_DISK_2(blob->rb_alias_offset_2, 0);
		}
#else
		CS_SET_DISK_2(blob->rb_alias_offset_2, 0);
#endif
		
	} else {
		CS_SET_DISK_2(blob->rb_mdata_offset_2, 0);
		CS_SET_DISK_2(blob->rb_alias_offset_2, 0);
	}
	

	if (blob_id) {
		tab_ref = (MSRepoTableRefPtr) (otab->myOTBuffer + myRepoBlobHeadSize);
		CS_SET_DISK_2(tab_ref->rr_type_2, MS_BLOB_TABLE_REF);
		CS_SET_DISK_4(tab_ref->tr_table_id_4, otab->getDBTable()->myTableID);
		CS_SET_DISK_6(tab_ref->tr_blob_id_6, blob_id);
		temp_ref = (MSRepoTempRefPtr) (otab->myOTBuffer + myRepoBlobHeadSize + ref_size);
		tab_idx = 1;  // This is the index of the blob table ref in the repository record.
		size = myRepoBlobHeadSize + ref_size + ref_size;
	}
	else {
		temp_ref = (MSRepoTempRefPtr) (otab->myOTBuffer + myRepoBlobHeadSize);
		tab_idx = INVALID_INDEX;  // Means not used
		size = myRepoBlobHeadSize + ref_size;
	}

	CS_SET_DISK_2(temp_ref->rr_type_2, MS_BLOB_DELETE_REF);
	CS_SET_DISK_2(temp_ref->tp_del_ref_2, tab_idx);
	CS_SET_DISK_4(temp_ref->tp_log_id_4, log_id);
	CS_SET_DISK_4(temp_ref->tp_offset_4, log_offset);
	
	if (blob_type == MS_CLOUD_STORAGE) { // The data is stored in the cloud and not in the repository.
		CS_SET_DISK_4(blob->rb_s3_key_id_4, cloud_key->ref_index);
		CS_SET_DISK_4(blob->rb_s3_cloud_ref_4, cloud_key->cloud_ref);
		blob_size = 0; // The blob is not stored in the repository so the blob storage size in the repository is zero
	}

	memset(otab->myOTBuffer + size, 0, head_size - size - metadata_size);
	
	CS_SET_DISK_1(blob->rb_storage_type_1, blob_type);
	CS_SET_DISK_6(blob->rb_blob_repo_size_6, blob_size);
	otab->myWriteRepoFile->write(blob, offset, head_size);
	
	setRepoFileSize(otab, offset + head_size + blob_size);
}

void MSRepository::setRepoFileSize(MSOpenTable *otab, off64_t offset)
{
	myRepoFileSize = offset;
	if (myRepoFileSize >= PBMSParameters::getRepoThreshold()
		/**/ || getGarbageLevel() >= PBMSParameters::getGarbageThreshold())
		otab->closeForWriting();
}

void MSRepository::syncHead(MSRepoFile *fh)
{
	MSRepoHeadRec head;

	fh->sync();
	myRecoveryOffset = myRepoFileSize;
	CS_SET_DISK_8(head.rh_recovery_offset_8, myRecoveryOffset);
	CS_SET_DISK_4(head.rh_last_temp_time_4, myLastTempTime);
	CS_SET_DISK_4(head.rh_last_access_4, myLastAccessTime);
	CS_SET_DISK_4(head.rh_create_time_4, myLastCreateTime);
	CS_SET_DISK_4(head.rh_last_ref_4, myLastRefTime);

	fh->write(&head.rh_recovery_offset_8, offsetof(MSRepoHeadRec, rh_recovery_offset_8), 24);
	fh->sync();
}

MSRepoFile *MSRepository::openRepoFile()
{
	MSRepoFile	*fh;

	enter_();
	fh = MSRepoFile::newRepoFile(this, getRepoFilePath());
	push_(fh);
	if (myRepoFileSize)
		fh->open(CSFile::DEFAULT);
	else
		fh->open(CSFile::CREATE);
	if (!myRepoHeadSize) {
		MSRepoHeadRec	head;
		MSBlobHeadRec	blob;
		size_t			size;
		int				status;
		int				ref_size;
		uint16_t		head_size;
		uint64_t		blob_size;

		lock_(this);
		/* Check again after locking: */
		if (!myRepoHeadSize) {
			if (fh->read(&head, 0, offsetof(MSRepoHeadRec, rh_reserved_4), 0) < offsetof(MSRepoHeadRec, rh_reserved_4)) {
				CS_SET_DISK_4(head.rh_magic_4, MS_REPO_FILE_MAGIC);
				CS_SET_DISK_2(head.rh_version_2, MS_REPO_FILE_VERSION);
				CS_SET_DISK_2(head.rh_repo_head_size_2, MS_REPO_FILE_HEAD_SIZE);
				CS_SET_DISK_2(head.rh_blob_head_size_2, sizeof(MSBlobHeadRec));
				CS_SET_DISK_2(head.rh_def_ref_size_2, sizeof(MSRepoGenericRefRec));
				CS_SET_DISK_8(head.rh_recovery_offset_8, MS_REPO_FILE_HEAD_SIZE);
				CS_SET_DISK_8(head.rh_garbage_count_8, 0);
				CS_SET_DISK_4(head.rh_last_temp_time_4, 0);
				CS_SET_DISK_4(head.rh_last_access_4, 0);
				CS_SET_DISK_4(head.rh_create_time_4, 0);
				CS_SET_DISK_4(head.rh_last_ref_4, 0);
				CS_SET_DISK_4(head.rh_reserved_4, 0);
				fh->write(&head, 0, sizeof(MSRepoHeadRec));
			}
			
			/* Check the file header: */
			if (CS_GET_DISK_4(head.rh_magic_4) != MS_REPO_FILE_MAGIC)
				CSException::throwFileError(CS_CONTEXT, fh->getPathString(), CS_ERR_BAD_HEADER_MAGIC);
			if (CS_GET_DISK_2(head.rh_version_2) > MS_REPO_FILE_VERSION)
				CSException::throwFileError(CS_CONTEXT, fh->getPathString(), CS_ERR_VERSION_TOO_NEW);

			/* Load the header details: */
			myRepoHeadSize = CS_GET_DISK_2(head.rh_repo_head_size_2);
			myRepoDefRefSize = CS_GET_DISK_2(head.rh_def_ref_size_2);
			myRepoBlobHeadSize = CS_GET_DISK_2(head.rh_blob_head_size_2);
			myRecoveryOffset = CS_GET_DISK_8(head.rh_recovery_offset_8);
			myGarbageCount = CS_GET_DISK_8(head.rh_garbage_count_8);
			myLastTempTime = CS_GET_DISK_4(head.rh_last_temp_time_4);
			myLastAccessTime = CS_GET_DISK_4(head.rh_last_access_4);
			myLastCreateTime = CS_GET_DISK_4(head.rh_create_time_4);
			myLastRefTime = CS_GET_DISK_4(head.rh_last_ref_4);

			/* File size, cannot be less than header size: */
			if (myRepoFileSize < myRepoHeadSize)
				myRepoFileSize = myRepoHeadSize;

			ASSERT(myGarbageCount <= myRepoFileSize);

			/* Recover the file: */
			while (myRecoveryOffset < myRepoFileSize) {
				if ((size = fh->read(&blob, myRecoveryOffset, MS_MIN_BLOB_HEAD_SIZE, 0)) < MS_MIN_BLOB_HEAD_SIZE) {
					if (size != 0) {
						myRepoFileSize = myRecoveryOffset;
						fh->setEOF(myRepoFileSize);
					}
					break;
				}
				uint16_t ref_count, mdata_size, mdata_offset;
				
				status = CS_GET_DISK_1(blob.rb_status_1);
				ref_size = CS_GET_DISK_1(blob.rb_ref_size_1);
				ref_count = CS_GET_DISK_2(blob.rb_ref_count_2);
				head_size = CS_GET_DISK_2(blob.rb_head_size_2);
				mdata_size = CS_GET_DISK_2(blob.rb_mdata_size_2);
				mdata_offset = CS_GET_DISK_2(blob.rb_mdata_offset_2);
				blob_size = CS_GET_DISK_6(blob.rb_blob_repo_size_6);
				if ((CS_GET_DISK_4(blob.rd_magic_4) != MS_BLOB_HEADER_MAGIC) ||
					(! IN_USE_BLOB_STATUS(status)) ||
					head_size < (myRepoBlobHeadSize + ref_size * MS_REPO_MIN_REF_COUNT) ||
					head_size < (mdata_offset + mdata_size) ||
					((blob_size == 0) && (BLOB_IN_REPOSITORY(CS_GET_DISK_1(blob.rb_storage_type_1)))) ||
					myRecoveryOffset + head_size + blob_size > myRepoFileSize) {
					myRepoFileSize = myRecoveryOffset;
					fh->setEOF(myRepoFileSize);
					break;
				}
				myRecoveryOffset += head_size + blob_size;
			}

			fh->sync();
			myRecoveryOffset = myRepoFileSize;
			CS_SET_DISK_8(head.rh_recovery_offset_8, myRecoveryOffset);
			fh->write(&head, offsetof(MSRepoHeadRec, rh_recovery_offset_8), 8);
			fh->sync();
		}
		unlock_(this);
	}
	pop_(fh);
	return_(fh);
}

void MSRepository::lockRepo(RepoLockState state)
{
	CSMutex	*myLock;
	enter_();
	
	myLock = &myRepoWriteLock;
	lock_(myLock);
	
	ASSERT(!myRepoXLock);
	
	myRepoLockState = state;
	myRepoXLock = true;
		
	unlock_(myLock);
	exit_();
}

void MSRepository::signalCompactor()
{
#ifndef MS_COMPACTOR_POLLS
	if (!mustBeDeleted) {
		if (getGarbageLevel() >= PBMSParameters::getGarbageThreshold()) {
			if (myRepoDatabase->myCompactorThread)
				myRepoDatabase->myCompactorThread->wakeup();
		}
	}
#endif
}

void MSRepository::unlockRepo(RepoLockState state)
{
	CSMutex	*myLock;
	enter_();
	myLock = &myRepoWriteLock;
	lock_(myLock);
	
	ASSERT(myRepoLockState & state);
	
	myRepoLockState &= ~state;
	if (myRepoLockState == REPO_UNLOCKED) {
		myRepoXLock = false;
		signalCompactor();
	}
	unlock_(myLock);
	
	exit_();
}

// Repositories are not removed from the pool when 
// scheduled for backup so the REPO_BACKUP flag is
// not handled here.
void MSRepository::returnToPool()
{
	CSMutex	*myLock;
	enter_();
	myLock = &myRepoWriteLock;
	lock_(myLock);
	this->myRepoLockState &= ~(REPO_COMPACTING | REPO_WRITE);
	if ( this->myRepoLockState == REPO_UNLOCKED) {
		myRepoXLock = false;
		signalCompactor();
	}
	unlock_(myLock);
	
	this->release();
	exit_();
}

void MSRepository::backupCompleted()
{
	CSMutex	*myLock;
	enter_();
	myLock = &myRepoWriteLock;
	lock_(myLock);
	
	
	this->myRepoLockState &= ~REPO_BACKUP;
	if ( this->myRepoLockState == REPO_UNLOCKED) {
		myRepoXLock = false;
		signalCompactor();
	}
		
	unlock_(myLock);
	exit_();
}

bool MSRepository::lockedForBackup() { return ((myRepoLockState & REPO_BACKUP) == REPO_BACKUP);}

uint32_t MSRepository::initBackup()
{
	CSMutex	*myLock;
	uint32_t state;
	enter_();
	
	myLock = &myRepoWriteLock;
	lock_(myLock);
	state = this->myRepoLockState;
	this->myRepoLockState |= REPO_BACKUP;
	if (this->myRepoLockState == REPO_BACKUP) 
		this->myRepoXLock = true;
		
	unlock_(myLock);
	return_(state);
}

MSRepoFile *MSRepository::getRepoFile()
{
	MSRepoFile *file;

	if ((file = iFilePool)) {
		iFilePool = file->nextFile;
		file->nextFile = NULL;
		file->isFileInUse = true;
		file->retain();
	}
	return file;
}

void MSRepository::addRepoFile(MSRepoFile *file)
{
	iPoolFiles.addFront(file);
}

void MSRepository::removeRepoFile(MSRepoFile *file)
{
	iPoolFiles.remove(file);
}

void MSRepository::returnRepoFile(MSRepoFile *file)
{
	file->isFileInUse = false;
	file->nextFile = iFilePool;
	iFilePool = file;
}

bool MSRepository::removeRepoFilesNotInUse()
{
	MSRepoFile *file, *curr_file;

	iFilePool = NULL;
	/* Remove all files that are not in use: */
	if ((file = (MSRepoFile *) iPoolFiles.getBack())) {
		do {
			curr_file = file;
			file = (MSRepoFile *) file->getNextLink();
			if (!curr_file->isFileInUse)
				iPoolFiles.remove(curr_file);
		} while (file);
	}
	return iPoolFiles.getSize() == 0;
}

off64_t MSRepository::getRepoFileSize()
{
	return myRepoFileSize;
}

size_t MSRepository::getRepoHeadSize()
{
	return myRepoHeadSize;
}

size_t MSRepository::getRepoBlobHeadSize()
{
	return myRepoBlobHeadSize;
}

CSMutex *MSRepository::getRepoLock(off64_t offset)
{
	return &myRepoLock[offset % CS_REPO_REC_LOCK_COUNT];
}

uint32_t MSRepository::getRepoID()
{
	return myRepoID;
}

uint32_t MSRepository::getGarbageLevel()
{
	if (myRepoFileSize <= myRepoHeadSize)
		return 0;
	return myGarbageCount * 100 / (myRepoFileSize - myRepoHeadSize);
}

CSPath *MSRepository::getRepoFilePath()
{
	char file_name[120];

	cs_strcpy(120, file_name, "bs-repository");
	cs_add_dir_char(120, file_name);
	cs_strcat(120, file_name, "repo-");
	cs_strcat(120, file_name, myRepoID);
	cs_strcat(120, file_name, ".bs");

	if (myRepoDatabase && myRepoDatabase->myDatabasePath) {
		return CSPath::newPath(RETAIN(myRepoDatabase->myDatabasePath), file_name);
	}
	return NULL;
}

