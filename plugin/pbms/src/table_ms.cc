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
 * Contains the information about a table.
 *
 */

#include "cslib/CSConfig.h"

#include <stdlib.h>
#include <stddef.h>

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSLog.h"
#include "cslib/CSPath.h"
#include "cslib/CSStrUtil.h"

#include "database_ms.h"
#include "open_table_ms.h"

MSTable::MSTable():
CSSharedRefObject(),
myTableName(NULL),
myTableID(0),
myDatabase(NULL),
iTableFileSize(0),
iTableHeadSize(0),
iFreeList(0),
iToDelete(0),
iTabDeleteTime(0),
iTabTempLogID(0),
iTabTempLogOffset(0)
{
}

MSTable::~MSTable()
{
	if (myTableName)
		myTableName->release();
}

#define MS_DELETE_MARK		"#DEL#"
#define MS_DELETE_MARK_LEN	5

CSPath *MSTable::getTableFile(const char *table_name, bool to_delete)
{
	char file_name[MS_TABLE_NAME_SIZE + 50];

	if ((table_name && to_delete) || iToDelete) {
		cs_strcpy(MS_TABLE_NAME_SIZE + 50, file_name, "bs-logs");
		// Make sure it exists
		cs_add_dir_char(MS_TABLE_NAME_SIZE + 50, file_name);
	}
	else
		*file_name = 0;
	if (table_name)
		cs_strcat(MS_TABLE_NAME_SIZE + 50, file_name, table_name);
	else {
		cs_strcat(MS_TABLE_NAME_SIZE + 50, file_name, myTableName->getCString());
		if (iToDelete) {
			char *str = file_name + strlen(file_name) - MS_DELETE_MARK_LEN;
			
			while (str > file_name) {
				if (strncmp(str, MS_DELETE_MARK, MS_DELETE_MARK_LEN) == 0) {
					*str = 0;
					break;
				}
				str--;
			}
		}
	}
	cs_strcat(MS_TABLE_NAME_SIZE + 50, file_name, "-");
	cs_strcat(MS_TABLE_NAME_SIZE + 50, file_name, myTableID);
	cs_strcat(MS_TABLE_NAME_SIZE + 50, file_name, ".bst");

	return CSPath::newPath(RETAIN(myDatabase->myDatabasePath), file_name);
}

CSPath *MSTable::getTableFile()
{
	return getTableFile(NULL, false);
}

CSFile *MSTable::openTableFile()
{
	CSPath	*path;
	CSFile	*fh;

	enter_();
	path = getTableFile();
	push_(path);
	fh = iTableFileSize ? path->openFile(CSFile::DEFAULT) : path->openFile(CSFile::CREATE);
	push_(fh);
	if (!iTableHeadSize) {
		MSTableHeadRec tab_head;

		lock_(this);
		/* Check again after locking: */
		if (!iTableHeadSize) {
			size_t rem;

			if (fh->read(&tab_head, 0, offsetof(MSTableHeadRec, th_reserved_4), 0) < offsetof(MSTableHeadRec, th_reserved_4)) {
				CS_SET_DISK_4(tab_head.th_magic_4, MS_TABLE_FILE_MAGIC);
				CS_SET_DISK_2(tab_head.th_version_2, MS_TABLE_FILE_VERSION);
				CS_SET_DISK_2(tab_head.th_head_size_2, MS_TABLE_FILE_HEAD_SIZE);
				CS_SET_DISK_8(tab_head.th_free_list_8, 0);
				CS_SET_DISK_4(tab_head.th_del_time_4, 0);
				CS_SET_DISK_4(tab_head.th_temp_log_id_4, 0);
				CS_SET_DISK_4(tab_head.th_temp_log_offset_4, 0);
				CS_SET_DISK_4(tab_head.th_reserved_4, 0);
				fh->write(&tab_head, 0, sizeof(MSTableHeadRec));
			}
			
			/* Check the file header: */
			if (CS_GET_DISK_4(tab_head.th_magic_4) != MS_TABLE_FILE_MAGIC)
				CSException::throwFileError(CS_CONTEXT, path->getString(), CS_ERR_BAD_HEADER_MAGIC);
			if (CS_GET_DISK_2(tab_head.th_version_2) > MS_TABLE_FILE_VERSION)
				CSException::throwFileError(CS_CONTEXT, path->getString(), CS_ERR_VERSION_TOO_NEW);

			/* Load the header details: */
			iFreeList = CS_GET_DISK_8(tab_head.th_free_list_8);
			iTableHeadSize = CS_GET_DISK_2(tab_head.th_head_size_2);
			iTabDeleteTime = CS_GET_DISK_4(tab_head.th_del_time_4);
			iTabTempLogID = CS_GET_DISK_4(tab_head.th_temp_log_id_4);
			iTabTempLogOffset = CS_GET_DISK_4(tab_head.th_temp_log_offset_4);

			/* Round file size up to a header boundary: */
			if (iTableFileSize < iTableHeadSize)
				iTableFileSize = iTableHeadSize;
			if ((rem = (iTableFileSize - iTableHeadSize) % sizeof(MSTableBlobRec)))
				iTableFileSize += sizeof(MSTableBlobRec) - rem;
		}
		unlock_(this);
	}
	pop_(fh);
	release_(path);
	return_(fh);
}

void MSTable::prepareToDelete()
{
	MSOpenTable	*otab;
	uint32_t		delete_time = 0;

	enter_();
	iToDelete = true;
	otab = MSOpenTable::newOpenTable(NULL);
	push_(otab);
	otab->myTableFile = openTableFile();
	if (iTabTempLogID) {
		MSTempLogFile		*log;
		MSTempLogItemRec	log_item;

		log = myDatabase->openTempLogFile(iTabTempLogID, NULL, NULL);
		if (log) {
			push_(log);
			if (log->read(&log_item, iTabTempLogOffset, sizeof(MSTempLogItemRec), 0) == sizeof(MSTempLogItemRec)) {
				delete_time = CS_GET_DISK_4(log_item.ti_time_4);
				if (delete_time != iTabDeleteTime)
					delete_time = 0;
			}
			release_(log);
		}
	}

	if (!delete_time) {
		MSTableHeadRec tab_head;

		myDatabase->queueForDeletion(otab, MS_TL_TABLE_REF, myTableID, 0, 0, &iTabTempLogID, &iTabTempLogOffset, &iTabDeleteTime);
		CS_SET_DISK_4(tab_head.th_del_time_4, iTabDeleteTime);
		CS_SET_DISK_4(tab_head.th_temp_log_id_4, iTabTempLogID);
		CS_SET_DISK_4(tab_head.th_temp_log_offset_4, iTabTempLogOffset);
		otab->myTableFile->write(&tab_head.th_del_time_4, offsetof(MSTableHeadRec, th_del_time_4), 12);
	}

	release_(otab);
	exit_();
}

uint64_t MSTable::findBlobHandle(MSOpenTable *otab, uint32_t repo_id, uint64_t file_offset, uint64_t size, uint16_t head_size, uint32_t auth_code)
{
	uint64_t			blob_id = 0;
	off64_t			offset = iTableHeadSize;
	MSTableBlobRec	blob;
	enter_();
	lock_(this);

	while (offset < iTableFileSize && !blob_id) {
		otab->myTableFile->read(&blob, offset, sizeof(MSTableBlobRec), sizeof(MSTableBlobRec));
		 if (	(CS_GET_DISK_1(blob.tb_status_1) == 1) &&
				(CS_GET_DISK_3(blob.tb_repo_id_3) == repo_id) &&
				(CS_GET_DISK_6(blob.tb_offset_6) == file_offset) &&
				(CS_GET_DISK_6(blob.tb_size_6) == size) &&
				(CS_GET_DISK_2(blob.tb_header_size_2) == head_size) &&
				(CS_GET_DISK_4(blob.tb_auth_code_4) == auth_code)	) {
				
			blob_id = offset;
		}
		offset += sizeof(MSTableBlobRec);
	}
	unlock_(this);
	return_(blob_id);
}

uint64_t MSTable::createBlobHandle(MSOpenTable *otab, uint32_t repo_id, uint64_t file_offset, uint64_t size, uint16_t head_size, uint32_t auth_code)
{
	uint64_t			blob_id;
	MSTableBlobRec	blob;

	enter_();
	lock_(this);
	if (iFreeList) {
		MSTableFreeBlobRec	freeb;
		MSTableHeadRec		tab_head;

		blob_id = iFreeList;
		otab->myTableFile->read(&freeb, iFreeList, sizeof(MSTableFreeBlobRec), sizeof(MSTableFreeBlobRec));
		iFreeList = CS_GET_DISK_6(freeb.tf_next_6);
		CS_SET_DISK_8(tab_head.th_free_list_8, iFreeList);
		otab->myTableFile->write(&tab_head.th_free_list_8, offsetof(MSTableHeadRec, th_free_list_8), 8);
	}
	else {
		blob_id = iTableFileSize;
		iTableFileSize += sizeof(MSTableBlobRec);
	}
	unlock_(this);

	CS_SET_DISK_1(blob.tb_status_1, 1);
	CS_SET_DISK_3(blob.tb_repo_id_3, repo_id);
	CS_SET_DISK_6(blob.tb_offset_6, file_offset);
	CS_SET_DISK_6(blob.tb_size_6, size);
	CS_SET_DISK_2(blob.tb_header_size_2, head_size);
	CS_SET_DISK_4(blob.tb_auth_code_4, auth_code);
	otab->myTableFile->write(&blob, blob_id, sizeof(MSTableBlobRec));

	return_(blob_id);
}

void MSTable::setBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t repo_id, uint64_t file_offset, uint64_t size, uint16_t head_size, uint32_t auth_code)
{
	MSTableBlobRec	blob;

	if (!otab->myTableFile && !otab->isNotATable)
		otab->myTableFile = openTableFile();
		
	CS_SET_DISK_1(blob.tb_status_1, 1);
	CS_SET_DISK_3(blob.tb_repo_id_3, repo_id);
	CS_SET_DISK_6(blob.tb_offset_6, file_offset);
	CS_SET_DISK_6(blob.tb_size_6, size);
	CS_SET_DISK_2(blob.tb_header_size_2, head_size);
	CS_SET_DISK_4(blob.tb_auth_code_4, auth_code);
	otab->myTableFile->write(&blob, blob_id, sizeof(MSTableBlobRec));
}

void MSTable::updateBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t repo_id, uint64_t offset, uint16_t head_size)
{
	MSTableBlobRec	blob;

	if (!otab->myTableFile && !otab->isNotATable)
		otab->myTableFile = openTableFile();
		
	CS_SET_DISK_3(blob.tb_repo_id_3, repo_id);
	CS_SET_DISK_6(blob.tb_offset_6, offset);
	if (head_size) {
		CS_SET_DISK_2(blob.tb_header_size_2, head_size);
		otab->myTableFile->write(&blob.tb_repo_id_3, blob_id + offsetof(MSTableBlobRec, tb_repo_id_3), 11);
	}
	else
		otab->myTableFile->write(&blob.tb_repo_id_3, blob_id + offsetof(MSTableBlobRec, tb_repo_id_3), 9);
}

bool MSTable::readBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t *auth_code,
	uint32_t *repo_id, uint64_t *offset, uint64_t *data_size, uint16_t *head_size, bool throw_error)
{
	MSTableBlobRec	blob;
	uint32_t			ac;

	if (!otab->myTableFile && !otab->isNotATable)
		otab->myTableFile = openTableFile();

	otab->myTableFile->read(&blob, blob_id, sizeof(MSTableBlobRec), sizeof(MSTableBlobRec));
	if (!(*repo_id = CS_GET_DISK_3(blob.tb_repo_id_3))) {
		if (throw_error)
			CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, "BLOB has already been freed");
		return false;
	}
	*offset = CS_GET_DISK_6(blob.tb_offset_6);
	*data_size = CS_GET_DISK_6(blob.tb_size_6);
	*head_size = CS_GET_DISK_2(blob.tb_header_size_2);
	ac = CS_GET_DISK_4(blob.tb_auth_code_4);
	if (!*auth_code)
		*auth_code = ac;
	else if (*auth_code != ac) {
		if (throw_error)
			CSException::throwException(CS_CONTEXT, MS_ERR_AUTH_FAILED, "Invalid BLOB identifier");
		return false;
	}
	return true;
}

void MSTable::freeBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t repo_id, uint64_t file_offset, uint32_t auth_code)
{
	MSTableBlobRec		blob;
	MSTableFreeBlobPtr	fblob = (MSTableFreeBlobPtr) &blob;
	MSTableHeadRec		tab_head;

	enter_();
	otab->openForReading();
	otab->myTableFile->read(&blob, blob_id, sizeof(MSTableBlobRec), sizeof(MSTableBlobRec));
	if (CS_GET_DISK_1(blob.tb_status_1) == 1 &&
		CS_GET_DISK_3(blob.tb_repo_id_3) == repo_id &&
		CS_GET_DISK_6(blob.tb_offset_6) == file_offset &&
		CS_GET_DISK_4(blob.tb_auth_code_4) == auth_code) {
		lock_(this);
		memset(&blob, 0, sizeof(MSTableBlobRec));
		CS_SET_DISK_6(fblob->tf_next_6, iFreeList);
		iFreeList = blob_id;
		CS_SET_DISK_8(tab_head.th_free_list_8, iFreeList);
		otab->myTableFile->write(&blob, blob_id, sizeof(MSTableBlobRec));
		otab->myTableFile->write(&tab_head.th_free_list_8, offsetof(MSTableHeadRec, th_free_list_8), 8);
		unlock_(this);
	}
	exit_();
}

CSObject *MSTable::getKey()
{
	return (CSObject *) myTableName;
}

int MSTable::compareKey(CSObject *key)
{
	return myTableName->compare((CSString *) key);
}

uint32_t MSTable::hashKey()
{
	return myTableName->hashKey();
}

CSString *MSTable::getTableName()
{
	return myTableName;
}

void MSTable::getDeleteInfo(uint32_t *log, uint32_t *offs, time_t *tim)
{
	if (!iTableHeadSize) {
		CSFile *fh;

		fh = openTableFile();
		fh->release();
	}

	*log = iTabTempLogID;
	*offs = iTabTempLogOffset;
	*tim = iTabDeleteTime; 
}

MSTable *MSTable::newTable(uint32_t tab_id, CSString *tab_name, MSDatabase *db, off64_t file_size, bool to_delete)
{
	MSTable *tab;

	if (!(tab = new MSTable())) {
		tab_name->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	if (to_delete) {
		/* Create a new table name: */
		char name_buffer[MS_TABLE_NAME_SIZE + 40];
		
		cs_strcpy(MS_TABLE_NAME_SIZE + 40, name_buffer, tab_name->getCString());
		cs_strcat(MS_TABLE_NAME_SIZE + 40, name_buffer, MS_DELETE_MARK);
		cs_strcat(MS_TABLE_NAME_SIZE + 40, name_buffer, tab_id);
		tab_name->release();
		tab_name = CSString::newString(name_buffer);
	}

	tab->myTableID = tab_id;
	tab->myTableName = tab_name;
	tab->myDatabase = db;
	tab->iTableFileSize = file_size;
	tab->iToDelete = to_delete;
	return tab;
}

MSTable *MSTable::newTable(uint32_t tab_id, const char *name, MSDatabase *db, off64_t file_size, bool to_delete)
{
	return newTable(tab_id, CSString::newString(name), db, file_size, to_delete);
}


