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
 * Contains the information about a table.
 *
 */

#pragma once
#ifndef __TABLE_MS_H__
#define __TABLE_MS_H__

#include "defs_ms.h"

#define MS_TABLE_FILE_MAGIC			0x1234ABCD
#define MS_TABLE_FILE_VERSION		1
#define MS_TABLE_FILE_HEAD_SIZE		128

class MSOpenTable;
class MSDatabase;
class CSHTTPOutputStream;

typedef struct MSTableHead {
	CSDiskValue4			th_magic_4;							/* Table magic number. */
	CSDiskValue2			th_version_2;						/* The header version. */
	CSDiskValue2			th_head_size_2;						/* The size of the header. */
	CSDiskValue8			th_free_list_8;

	/* These 3 fields are written together by prepareToDelete()! */
	CSDiskValue4			th_del_time_4;						/* The time this table was deleted. */
	CSDiskValue4			th_temp_log_id_4;					/* The temp log entry for this deleted table. */
	CSDiskValue4			th_temp_log_offset_4;

	CSDiskValue4			th_reserved_4;
} MSTableHeadRec, *MSTableHeadPtr;

/* File offset = th_size_4 + (Blob ID - 1) * sizeof(MSTableBlobRec) */ 
typedef struct MSTableBlob {
	CSDiskValue1			tb_status_1;						/* 0 = free, 1 = inuse*/

	/* These 3 fields are written together: */
	CSDiskValue3			tb_repo_id_3;						/* File ID (non-zero). */
	CSDiskValue6			tb_offset_6;						/* Offset into the file of the BLOB. */
	CSDiskValue2			tb_header_size_2;					/* Size of the header section of the blob. */

	CSDiskValue6			tb_size_6;							/* Size of the BLOB data. (Where ever it may be stored.) */ 
	CSDiskValue4			tb_auth_code_4;						/* BLOB authorisation code. */
} MSTableBlobRec, *MSTableBlobPtr;

typedef struct MSTableFreeBlob {
	CSDiskValue4			tf_null_4;							/* Set to zero */
	CSDiskValue6			tf_next_6;							/* Next record in the free list. */
} MSTableFreeBlobRec, *MSTableFreeBlobPtr;

class MSTable : public CSSharedRefObject {
public:
	CSString	*myTableName;
	uint32_t		myTableID;
	MSDatabase	*myDatabase;

	MSTable();
	virtual ~MSTable();

	CSPath *getTableFile(const char *table_name, bool to_delete);

	CSPath *getTableFile();

	CSFile *openTableFile();
	
	void prepareToDelete();

	uint64_t createBlobHandle(MSOpenTable *otab, uint32_t repo_id, uint64_t file_offset, uint64_t size, uint16_t head_size, uint32_t auth_code);

	uint64_t findBlobHandle(MSOpenTable *otab, uint32_t repo_id, uint64_t file_offset, uint64_t size, uint16_t head_size, uint32_t auth_code);

	void setBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t repo_id, uint64_t file_offset, uint64_t size, uint16_t head_size, uint32_t auth_code);

	void updateBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t repo_id, uint64_t offset, uint16_t head_size);

	bool readBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t *auth_code, uint32_t *repo_id, uint64_t *offset, uint64_t *data_size, uint16_t *head_size, bool throw_error);

	void freeBlobHandle(MSOpenTable *otab, uint64_t blob_id, uint32_t repo_id, uint64_t file_offset, uint32_t auth_code);

	/* Make this object hashable: */
	virtual CSObject *getKey();
	virtual int compareKey(CSObject *);
	virtual uint32_t hashKey();

	off64_t getTableFileSize() { return iTableFileSize; }
	CSString *getTableName();
	bool isToDelete() { return iToDelete; }
	void getDeleteInfo(uint32_t *log, uint32_t *offs, time_t *tim);
	bool isNoTable() { return myTableName->length() == 0; }

private:
	off64_t		iTableFileSize;
	size_t		iTableHeadSize;
	off64_t		iFreeList;
	bool		iToDelete;
	uint32_t		iTabDeleteTime;
	uint32_t		iTabTempLogID;
	uint32_t		iTabTempLogOffset;

public:
	static MSTable *newTable(uint32_t tab_id, CSString *name, MSDatabase *db, off64_t file_size, bool to_delete);

	static MSTable *newTable(uint32_t tab_id, const char *name, MSDatabase *db, off64_t file_size, bool to_delete);	
};

#endif
