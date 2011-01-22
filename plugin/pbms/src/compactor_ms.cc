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
 * 2007-07-10
 *
 * H&G2JCtL
 *
 * Network interface.
 *
 */

#include "cslib/CSConfig.h"

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSStorage.h"

#include "compactor_ms.h"
#include "open_table_ms.h"
#include "repository_ms.h"
#include "parameters_ms.h"

/*
 * ---------------------------------------------------------------
 * COMPACTOR THREAD
 */

MSCompactorThread::MSCompactorThread(time_t wait_time, MSDatabase *db):
CSDaemon(wait_time, NULL),
iCompactorDatabase(db)
{
}

void MSCompactorThread::close()
{
}

bool MSCompactorThread::doWork()
{
	bool				complete;
	MSRepository		*src_repo, *dst_repo;
	MSRepoFile			*src_file, *dst_file;
	uint32_t			src_repo_id;
	MSBlobHeadRec		blob;
	off64_t				src_offset;
	uint16_t			head_size;
	uint64_t			blob_size, blob_data_size;
	CSStringBuffer		*head;
	MSRepoPointersRec	ptr;
	uint32_t				table_ref_count;
	uint32_t				blob_ref_count;
	int					ref_count;
	size_t				ref_size;
	CSMutex				*mylock;
	uint32_t			tab_id;
	uint64_t			blob_id;
	MSOpenTable			*otab;
	uint32_t			repo_id;
	uint64_t			repo_offset;
	uint64_t			repo_blob_size;
	uint16_t			repo_head_size;
	uint16_t			tab_index;
	uint8_t				status;

	enter_();
	retry:
	
#ifdef MS_COMPACTOR_POLLS
	if (!(src_repo = iCompactorDatabase->getRepoFullOfTrash(NULL)))
		return_(true);
#else
	myWaitTime = MS_DEFAULT_COMPACTOR_WAIT * 1000;  // Time in milli-seconds
	if (!(src_repo = iCompactorDatabase->getRepoFullOfTrash(&myWaitTime)))
		return_(true);
#endif
	frompool_(src_repo);
	src_file = src_repo->openRepoFile();
	push_(src_file);

	dst_repo = iCompactorDatabase->lockRepo(src_repo->myRepoFileSize - src_repo->myGarbageCount);
	frompool_(dst_repo);
	dst_file = dst_repo->openRepoFile();
	push_(dst_file);

	new_(head, CSStringBuffer(100));
	push_(head);

	complete = false;
	src_repo_id = src_repo->myRepoID;
	src_offset = src_repo->myRepoHeadSize;
	//printf("\nCompacting repo %"PRId32"\n\n", src_repo_id);
	// For testing:
	{
		int blockit = 0;
		if (blockit) {
			release_(head);
			release_(dst_file);
			backtopool_(dst_repo);
			release_(src_file);
			backtopool_(src_repo);

			myWaitTime = 5 * 1000;  // Time in milli-seconds
			return_(true);
		}
	}
	while (src_offset < src_repo->myRepoFileSize) {			
		retry_loop:
		suspended();

		if (myMustQuit)
			goto quit;
		retry_read:
		
		// A lock is required here because references and dereferences to the
		// BLOBs can result in the repository record being updated while 
		// it is being copied.
		mylock = &src_repo->myRepoLock[src_offset % CS_REPO_REC_LOCK_COUNT];
		lock_(mylock);
		if (src_file->read(&blob, src_offset, src_repo->myRepoBlobHeadSize, 0) < src_repo->myRepoBlobHeadSize) {
			unlock_(mylock);
			break;
		}
		ref_size = CS_GET_DISK_1(blob.rb_ref_size_1);
		ref_count = CS_GET_DISK_2(blob.rb_ref_count_2);
		head_size = CS_GET_DISK_2(blob.rb_head_size_2);
		blob_size = CS_GET_DISK_6(blob.rb_blob_repo_size_6);
		blob_data_size = CS_GET_DISK_6(blob.rb_blob_data_size_6);
		status = CS_GET_DISK_1(blob.rb_status_1);
		if ((blob_data_size == 0) || ref_count <= 0 || ref_size == 0 ||
			head_size < src_repo->myRepoBlobHeadSize + ref_count * ref_size ||
			!VALID_BLOB_STATUS(status)) {
			/* Can't be true. Assume this is garbage! */
			unlock_(mylock);
			src_offset++;
			goto retry_read;
		}
		if (IN_USE_BLOB_STATUS(status)) {
			head->setLength(head_size);
			if (src_file->read(head->getBuffer(0), src_offset, head_size, 0) != head_size) {
				unlock_(mylock);
				break;
			}

			table_ref_count = 0;
			blob_ref_count = 0;
			
			ptr.rp_chars = head->getBuffer(0) + src_repo->myRepoBlobHeadSize;
			for (int count = 0; count < ref_count; count++) {
				switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
					case MS_BLOB_FREE_REF:
						break;
					case MS_BLOB_TABLE_REF:
						/* Check the reference: */
						tab_id = CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4);
						blob_id = CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6);

						otab = MSTableList::getOpenTableByID(iCompactorDatabase->myDatabaseID, tab_id);
						if (otab) {
							frompool_(otab);
							/* Ignore the return value (it will fail because auth_code is wrong!)!! */
							uint32_t auth_code = 0;
							otab->getDBTable()->readBlobHandle(otab, blob_id, &auth_code, &repo_id, &repo_offset, &repo_blob_size, &repo_head_size, false);
							backtopool_(otab);
							if (repo_id == src_repo_id &&
								repo_offset == src_offset &&
								repo_blob_size == blob_data_size &&
								repo_head_size == head_size)
								table_ref_count++;
							else
								/* Remove the reference: */
								CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
						}
						else
							CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
						break;
					case MS_BLOB_DELETE_REF:
						/* These are temporary references from the TempLog file. */
						/* We try to prevent this from happening, but it can! */
						uint32_t			temp_log_id;
						uint32_t			temp_log_offset;
						MSTempLogFile	*temp_log;

						temp_log_id = CS_GET_DISK_4(ptr.rp_temp_ref->tp_log_id_4);
						temp_log_offset = CS_GET_DISK_4(ptr.rp_temp_ref->tp_offset_4);
						if ((temp_log = iCompactorDatabase->openTempLogFile(temp_log_id, NULL, NULL))) {
							MSTempLogItemRec	log_item;
							uint32_t				then;
							time_t				now;

							push_(temp_log);
							if (temp_log->read(&log_item, temp_log_offset, sizeof(MSTempLogItemRec), 0) == sizeof(MSTempLogItemRec)) {
								then = CS_GET_DISK_4(log_item.ti_time_4);
								now = time(NULL);
								if (now < (time_t)(then + PBMSParameters::getTempBlobTimeout())) {
									/* Wait for the BLOB to expire before we continue: */									
									release_(temp_log);
									unlock_(mylock);

									/* Go to sleep until the problem has gone away! */
									lock_(this);
									suspendedWait(MSTempLog::adjustWaitTime(then, now));
									unlock_(this);
									goto retry_loop;
								}
							}
							release_(temp_log);
						}

						/* Remove the temp reference: */
						CS_SET_DISK_2(ptr.rp_ref->rr_type_2, MS_BLOB_FREE_REF);
						break;
					default:
						tab_index = CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2);
						if (tab_index > ref_count || !tab_index) {
							/* Can't be true. Assume this is garbage! */
							unlock_(mylock);
							src_offset++;
							goto retry_read;
						}
						blob_ref_count++;
						break;
				}
				ptr.rp_chars += ref_size;
			}

			if (table_ref_count && blob_ref_count) {
				/* Check the blob references again to make sure that they
				 * refer to valid table references.
				 */
				MSRepoTableRefPtr	tab_ref;

				blob_ref_count = 0;
				ptr.rp_chars = head->getBuffer(0) + src_repo->myRepoBlobHeadSize;
				for (int count = 0; count < ref_count; count++) {
					switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
						case MS_BLOB_FREE_REF:
						case MS_BLOB_TABLE_REF:
						case MS_BLOB_DELETE_REF:
							break;
						default: // If it isn't one of the above we assume it is an blob ref. (er_table_2 can never have a value equal to one of the above REF type flags.)
								// It was already verified above that the index was with in range.
							tab_ref = (MSRepoTableRefPtr) (head->getBuffer(0) + src_repo->myRepoBlobHeadSize + (CS_GET_DISK_2(ptr.rp_blob_ref->er_table_2)-1) * ref_size);
							if (CS_GET_DISK_2(tab_ref->rr_type_2) == MS_BLOB_TABLE_REF)
								blob_ref_count++;
							break;
					}
					ptr.rp_chars += ref_size;
				}
			}

			if (blob_ref_count) {
				off64_t dst_offset;

				dst_offset = dst_repo->myRepoFileSize;

				/* Write the header. */
				dst_file->write(head->getBuffer(0), dst_offset, head_size);

				/* We have an engine reference, copy the BLOB over: */
				CSFile::transfer(RETAIN(dst_file), dst_offset + head_size, RETAIN(src_file), src_offset + head_size, blob_size, iCompactBuffer, MS_COMPACTOR_BUFFER_SIZE);

#ifdef HAVE_ALIAS_SUPPORT
				/* If the BLOB has an alias update the alias index. */
				if (CS_GET_DISK_2(blob.rb_alias_offset_2)) {
					iCompactorDatabase->moveBlobAlias( src_repo_id, src_offset, CS_GET_DISK_4(blob.rb_alias_hash_4), dst_repo->myRepoID, dst_offset);
				}
#endif				
				/* Update the references: */
				ptr.rp_chars = head->getBuffer(0) + src_repo->myRepoBlobHeadSize;
				for (int count = 0; count < ref_count; count++) {
					switch (CS_GET_DISK_2(ptr.rp_ref->rr_type_2)) {
						case MS_BLOB_FREE_REF:
							break;
						case MS_BLOB_TABLE_REF:
							tab_id = CS_GET_DISK_4(ptr.rp_tab_ref->tr_table_id_4);
							blob_id = CS_GET_DISK_6(ptr.rp_tab_ref->tr_blob_id_6);

							if ((otab = MSTableList::getOpenTableByID(iCompactorDatabase->myDatabaseID, tab_id))) {
								frompool_(otab);
								otab->getDBTable()->updateBlobHandle(otab, blob_id, dst_repo->myRepoID, dst_offset, 0);
								backtopool_(otab);
							}
							break;
						case MS_BLOB_DELETE_REF:
							break;
						default:
							break;
					}
					ptr.rp_chars += ref_size;
				}

				dst_repo->myRepoFileSize += head_size + blob_size;
			}
		}
		
		unlock_(mylock);
		src_offset += head_size + blob_size;
	}

	src_repo->mustBeDeleted = true;
	complete = true;

	quit:
	release_(head);
	release_(dst_file);
	backtopool_(dst_repo);
	release_(src_file);
	backtopool_(src_repo);

	if (complete)
		iCompactorDatabase->removeRepo(src_repo_id, &myMustQuit);

	if (!myMustQuit)
		goto retry;
	return_(true);
}

void *MSCompactorThread::completeWork()
{
	close();
	return NULL;
}

