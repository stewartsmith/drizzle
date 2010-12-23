#ifdef NOT_USED_IN_ANY_THING

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
 * Barry Leslie
 *
 * 2007-11-25
 *
 * H&G2JCtL
 *
 */

#include "cslib/CSConfig.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSLog.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSHTTPStream.h"
#include "cslib/CSStream.h"

#include "repository_ms.h"
#include "open_table_ms.h"
#include "mysql_ms.h"

//-----------------------------------------------------------------------------------------------
void PBMSGetError(void *v_bs_thread, PBMSResultPtr result)
{
	CSThread *ms_thread = (CSThread*)v_bs_thread;
	
	ASSERT(ms_thread);
	memset(result, 0, sizeof(PBMSResultRec));
	
	result->mr_code =  ms_thread->myException.getErrorCode();
	cs_strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message,  ms_thread->myException.getMessage());
}

//-----------------------------------------------------------------------------------------------
void *PBMSInitBlobStreamingThread(char *thread_name, PBMSResultPtr result)
{
	CSThread *ms_thread =  new CSThread( NULL);
	
	if (!ms_thread) {
		memset(result, 0, sizeof(PBMSResultRec));
		result->mr_code = ENOMEM;
		cs_strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, "CSThread::newThread() failed.");
		return NULL; 
	}
	
	ms_thread->pbms_api_owner = true;
	if (!CSThread::attach(ms_thread)) {
		memset(result, 0, sizeof(PBMSResultRec));
		result->mr_code =  ms_thread->myException.getErrorCode();
		cs_strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message,  ms_thread->myException.getMessage());
		ms_thread->release();
		ms_thread = NULL;
	} else
		ms_thread->threadName = CSString::newString(thread_name);
	
	return ms_thread;
}


//-----------------------------------------------------------------------------------------------
void PBMSDeinitBlobStreamingThread(void *v_bs_thread)
{
	CSThread *ms_thread = (CSThread*)v_bs_thread;
	
	ASSERT(ms_thread);

	CSThread::detach(ms_thread);
	// ms_thread->release(); Don't do this. Ownership of the thread is passed to the attach call so the thread is released when it is detached.
}

//-----------------------------------------------------------------------------------------------
bool PBMSCreateBlob(PBMSBlobIDPtr blob_id, char *database_name, uint64_t size)
{
	MSOpenTable *otab = NULL;
	CSString *iTableURI =  NULL;
	CSString *CSContenttype = NULL;
	bool done_ok = true;

	enter_();

	try_(a) {
		otab = MSTableList::getOpenTableForDB(MSDatabase::getDatabaseID(database_name, false));
		
		otab->createBlob(blob_id, size, NULL, 0);
	}
	
	catch_(a) {
		done_ok = false;
	}
	cont_(a);

	exit:
	if (otab)
		otab->returnToPool();
	
	if (CSContenttype)
		CSContenttype->release();
		
	if (iTableURI)
		iTableURI->release();
		
	return_(done_ok);
}

//-----------------------------------------------------------------------------------------------
bool PBMSWriteBlob(PBMSBlobIDPtr blob_id, char *data, size_t size, size_t offset)
{
	MSOpenTable *otab;
	MSRepoFile	*repo_file;
	bool done_ok = true;

	enter_();

	try_(a) {
		if (!(otab = MSTableList::getOpenTableForDB(blob_id->bi_db_id))) {
			char buffer[CS_EXC_MESSAGE_SIZE];
			char id_str[12];
			
			snprintf(id_str, 12, "%"PRIu32"", blob_id->bi_db_id);

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown database id #  ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, id_str);
			CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_DB, buffer);
		}
		frompool_(otab);
		repo_file = otab->getDB()->getRepoFileFromPool( blob_id->bi_tab_id, false);
		frompool_(repo_file);
		// It is assumed that at this point the blob is a repository blob and so the 
		// blob_id->bi_blob_id is actually the repository blob offset. 
		repo_file->writeBlobChunk(blob_id, blob_id->bi_blob_id, offset, size, data);
		backtopool_(repo_file);
		backtopool_(otab);

	}
	catch_(a) {
		done_ok = false;
	}
	
	cont_(a);
		
	return_(done_ok);
}

//-----------------------------------------------------------------------------------------------
bool PBMSReadBlob(PBMSBlobIDPtr blob_id, char *buffer, size_t *size, size_t offset)
{
	MSOpenTable *otab;
	MSRepoFile	*repo_file;
	bool done_ok = true, is_repository_blob;

	enter_();

	is_repository_blob = (blob_id->bi_blob_type == MS_URL_TYPE_REPO);
	try_(a) {
		if (!(otab = MSTableList::getOpenTableByID(blob_id->bi_db_id, blob_id->bi_tab_id))) {
			char buffer[CS_EXC_MESSAGE_SIZE];
			char id_str[12];
			
	
			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown database: ID # ");
			snprintf(id_str, 12, "%"PRIu32"", blob_id->bi_db_id);
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, id_str);
			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, " or table: ID #");
			snprintf(id_str, 12, "%"PRIu32"", blob_id->bi_tab_id);
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, id_str);
			CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_DB, buffer);
		}
		uint32_t repo_id;
		uint64_t rep_offset;

		
		frompool_(otab);
		if (is_repository_blob) {
			repo_id = blob_id->bi_tab_id;
			rep_offset = blob_id->bi_blob_id;
		} else {
			uint64_t blob_size;
			uint16_t header_size; 
			otab->getDBTable()->readBlobHandle(otab, blob_id->bi_blob_id, &(blob_id->bi_auth_code), &repo_id, &rep_offset, &blob_size, &header_size, true);
		}
		
		repo_file = otab->getDB()->getRepoFileFromPool( repo_id, false);
		frompool_(repo_file);
		*size = repo_file->readBlobChunk(blob_id, rep_offset, offset, *size, buffer);
		backtopool_(repo_file);
		backtopool_(otab);

	}
	catch_(a) {
		done_ok = false;
	}
	
	cont_(a);
		
	return_(done_ok);
}

//-----------------------------------------------------------------------------------------------
bool PBMSIDToURL(PBMSBlobIDPtr blob_id, PBMSBlobURLPtr url)
{	
	MSBlobURL ms_blob;

	ms_blob.bu_db_id = blob_id->bi_db_id;
	ms_blob.bu_blob_id = blob_id->bi_blob_id;
	ms_blob.bu_blob_ref_id = blob_id->bi_blob_ref_id;
	ms_blob.bu_tab_id = blob_id->bi_tab_id;
	ms_blob.bu_auth_code = blob_id->bi_auth_code;
	ms_blob.bu_type = blob_id->bi_blob_type;
	ms_blob.bu_blob_size = blob_id->bi_blob_size; 
	ms_blob.bu_server_id = ms_my_get_server_id();
	
	PBMSBlobURLTools::buildBlobURL(&ms_blob, url);
	return true;
}

//-----------------------------------------------------------------------------------------------
bool PBMSURLToID(char *url, PBMSBlobIDPtr blob_id)
{	
	MSBlobURL ms_blob;
	bool done_ok = true;
	enter_();

	try_(a) {
	
		if (!PBMSBlobURLTools::couldBeURL(url, &ms_blob)){
			char buffer[CS_EXC_MESSAGE_SIZE];

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Incorrect URL: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, url);
			CSException::throwException(CS_CONTEXT, MS_ERR_INCORRECT_URL, buffer);
		}
		
		blob_id->bi_db_id = ms_blob.bu_db_id;
		blob_id->bi_blob_id = ms_blob.bu_blob_id;
		blob_id->bi_blob_ref_id = ms_blob.bu_blob_ref_id;
		blob_id->bi_tab_id = ms_blob.bu_tab_id;
		blob_id->bi_auth_code = ms_blob.bu_auth_code;
		blob_id->bi_blob_type = ms_blob.bu_type;
		blob_id->bi_blob_size = ms_blob.bu_blob_size; 
		
	}
	catch_(a) {
		done_ok = false;
	}
	
	cont_(a);
	
	return_(done_ok);
}


#endif // NOT_USED_IN_ANY_THING
