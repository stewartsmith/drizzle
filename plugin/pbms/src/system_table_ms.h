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
 * 2007-07-18
 *
 * H&G2JCtL
 *
 * System tables.
 *
 */

/*

DROP TABLE IF EXISTS pbms_repository;
CREATE TABLE pbms_repository (
	Repository_id     INT COMMENT 'The reppository file number',
	Repo_blob_offset  BIGINT COMMENT 'The offset of the BLOB in the repository file',
	Blob_size         BIGINT COMMENT 'The size of the BLOB in bytes',
	Head_size         SMALLINT UNSIGNED COMMENT 'The size of the BLOB header - preceeds the BLOB data',
	Access_code       INT COMMENT 'The 4-byte authorisation code required to access the BLOB - part of the BLOB URL',
	Creation_time     TIMESTAMP COMMENT 'The time the BLOB was created',
	Last_ref_time     TIMESTAMP COMMENT 'The last time the BLOB was referenced',
	Last_access_time  TIMESTAMP COMMENT 'The last time the BLOB was accessed (read)',
	Content_type      CHAR(128) COMMENT 'The content type of the BLOB - returned by HTTP GET calls',
	Blob_data         LONGBLOB COMMENT 'The data of this BLOB'
) ENGINE=PBMS;

	PRIMARY KEY (Repository_id, Repo_blob_offset)

DROP TABLE IF EXISTS pbms_reference;
CREATE TABLE pbms_reference (
	Table_name        CHAR(64) COMMENT 'The name of the referencing table',
	Blob_id           BIGINT COMMENT 'The BLOB reference number - part of the BLOB URL',
	Column_name       CHAR(64) COMMENT 'The column name of the referencing field',
	Row_condition     VARCHAR(255) COMMENT 'This condition identifies the row in the table',
	Blob_url          VARCHAR(200) COMMENT 'The BLOB URL for HTTP GET access',
	Repository_id     INT COMMENT 'The repository file number of the BLOB',
	Repo_blob_offset  BIGINT COMMENT 'The offset in the repository file',
	Blob_size         BIGINT COMMENT 'The size of the BLOB in bytes',
	Deletion_time     TIMESTAMP COMMENT 'The time the BLOB was deleted',
	Remove_in         INT COMMENT 'The number of seconds before the reference/BLOB is removed perminently',
	Temp_log_id       INT COMMENT 'Temporary log number of the referencing deletion entry',
	Temp_log_offset   BIGINT COMMENT 'Temporary log offset of the referencing deletion entry'
) ENGINE=PBMS;

	PRIMARY KEY (Table_name, Blob_id, Column_name, Condition)
*/

#pragma once
#ifndef __SYSTEMTABLE_MS_H__
#define __SYSTEMTABLE_MS_H__

#include "cslib/CSMutex.h"

#include "defs_ms.h"
#include "repository_ms.h"
#include "cloud_ms.h"

#ifdef DRIZZLED
using namespace drizzled;
#else
int pbms_discover_system_tables(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen);
#endif

class PBMSSystemTables
{
	public:
	
	#ifdef DRIZZLED
	static int getSystemTableInfo(const char *name, drizzled::message::Table &table);
	static void getSystemTableNames(bool isPBMS, std::set<std::string> &set_of_names);
	#endif

	static bool isSystemTable(bool isPBMS, const char *name);

	static void transferSystemTables(MSDatabase *dst_db, MSDatabase *src_db);
	static void loadSystemTables(MSDatabase *db);
	static void removeSystemTables(CSString *path);

	static CSStringBuffer *dumpSystemTables(MSDatabase *db);
	static void restoreSystemTables(MSDatabase *db, const char *data, size_t size);

	static void systemTablesStartUp();
	static void systemTableShutDown();

	private:
	static bool try_loadSystemTables(CSThread *self, int i, MSDatabase *db);
};

class MSSystemTableShare;
class MSDatabase;
class CSDaemon;
class MSTempLogFile;

#ifdef DRIZZLED
#define GET_FIELD(table, column_index) table->getField(column_index)
#define GET_TABLE_FIELDS(table) table->getFields()
#else
#define GET_FIELD(table, column_index) table->field[column_index]
#define GET_TABLE_FIELDS(table) table->field
#endif


class MSOpenSystemTable : public CSRefObject {
public:
	MSSystemTableShare		*myShare;
	TABLE					*mySQLTable;

	MSOpenSystemTable(MSSystemTableShare *share, TABLE *table);
	virtual ~MSOpenSystemTable();

	/*
	 * The getFieldValue() methods access fields in a server record.
	 * They assumes the caller knows what it is doing and does no error checking.
	 */
	inline void getFieldValue(const char *row, uint16_t column_index, String *value)
	{
		Field *assumed_str_field = GET_FIELD(mySQLTable, column_index);
		unsigned char *old_ptr = assumed_str_field->ptr;
		

#ifdef DRIZZLED
		assumed_str_field->ptr = (unsigned char *)row + assumed_str_field->offset(mySQLTable->getInsertRecord());
		assumed_str_field->setReadSet();
#else
		assumed_str_field->ptr = (unsigned char *)row + assumed_str_field->offset(mySQLTable->record[0]);
		assumed_str_field->table->use_all_columns();
#endif
		value = assumed_str_field->val_str_internal(value);
		
		assumed_str_field->ptr = old_ptr;
	}

	inline void getFieldValue(const char *row, uint16_t column_index, uint64_t *value)
	{
		Field *assumed_int_field = GET_FIELD(mySQLTable, column_index);
		unsigned char *old_ptr = assumed_int_field->ptr;


#ifdef DRIZZLED
		assumed_int_field->ptr = (unsigned char *)row + assumed_int_field->offset(mySQLTable->getInsertRecord());
		assumed_int_field->setReadSet();
#else
		assumed_int_field->ptr = (unsigned char *)row + assumed_int_field->offset(mySQLTable->record[0]);
		assumed_int_field->table->use_all_columns();
#endif
		*value = assumed_int_field->val_int();
		
		assumed_int_field->ptr = old_ptr;
	}

	inline void getFieldValue(const char *row, uint16_t column_index, uint32_t *value)
	{
		uint64_t v64;
		getFieldValue(row, column_index, &v64);
		*value = (uint32_t) v64;
	}

	virtual void use() { }
	virtual void unuse() { }
	virtual void backupSeqScanInit() { }
	virtual void seqScanInit() { }
	virtual bool seqScanNext(char *buf) { UNUSED(buf); return false; }
	virtual int	getRefLen() { return 0; }
	virtual void seqScanPos(unsigned char *pos) { UNUSED(pos);}
	virtual void seqScanRead(unsigned char *pos , char *buf) {UNUSED(pos);UNUSED(buf); }
	virtual void insertRow(char *buf) {UNUSED(buf); }
	virtual void deleteRow(char *buf) {UNUSED(buf);  }
	virtual void updateRow(char *old_data, char *new_data) {UNUSED(old_data);UNUSED(new_data); }
/*	
	virtual void index_init(uint8_t idx) { }
	virtual void index_end() { }
	virtual bool index_read(char *buf, const char *key,
								 uint key_len, enum ha_rkey_function find_flag){ return false;}
	virtual bool index_read_idx(char *buf, uint idx, const char *key,
										 uint key_len, enum ha_rkey_function find_flag){ return false;}
	virtual bool index_next(char *buf){ return false;}
	virtual bool index_prev(char *buf){ return false;}
	virtual bool index_first(char *buf){ return false;}
	virtual bool index_last(char *buf){ return false;}
	virtual bool index_read_last(char *buf, const char *key, uint key_len){ return false;}
*/
	
	static void setNotNullInRecord(Field *field, char *record);
	static void setNullInRecord(Field *field, char *record);

private:
};

typedef struct MSRefData {
	uint32_t				rd_ref_count;
	uint32_t				rd_tab_id;
	uint64_t				rd_blob_id;
	uint64_t				rd_blob_ref_id;
	uint32_t				rd_temp_log_id;
	uint32_t				rd_temp_log_offset;
	uint16_t				rd_col_index;
} MSRefDataRec, *MSRefDataPtr;

class MSRepositoryTable : public MSOpenSystemTable {
public:
	MSRepositoryTable(MSSystemTableShare *share, TABLE *table);
	virtual ~MSRepositoryTable();

	virtual void use();
	virtual void unuse();
	virtual void seqScanInit();
	virtual bool seqScanNext(char *buf);
	virtual int	getRefLen();
	virtual void seqScanPos(unsigned char *pos);
	virtual void seqScanRead(uint32_t repo, uint64_t offset, char *buf);
	virtual void seqScanRead(unsigned char *pos, char *buf);

	friend class MSReferenceTable;
	friend class MSBlobDataTable;
	friend class MSMetaDataTable;
	friend class MSBlobAliasTable;
	friend class MSDumpTable;

private:
	uint32_t			iRepoIndex;
	uint64_t			iRepoCurrentOffset;
	uint64_t			iRepoOffset;
	uint64_t			iRepoFileSize;
	CSDaemon		*iCompactor;
	MSRepoFile		*iRepoFile;
	CSStringBuffer	*iBlobBuffer;

	virtual bool returnRecord(char *buf);
	virtual bool returnSubRecord(char *buf);
	virtual bool returnRow(MSBlobHeadPtr blob, char *buf);
	virtual bool resetScan(bool positioned, uint32_t iRepoIndex = 0);
};

class MSBlobDataTable : public MSRepositoryTable {
public:
	MSBlobDataTable(MSSystemTableShare *share, TABLE *table);
	~MSBlobDataTable();
	
private:
	virtual bool returnRow(MSBlobHeadPtr blob, char *buf);
};

// The main purpose of this class if for internal use when
// building the alias index.
class MSBlobAliasTable : public MSRepositoryTable {
public:
	MSBlobAliasTable(MSSystemTableShare *share, TABLE *table):MSRepositoryTable(share, table){}	
private:
	
	virtual bool returnRow(MSBlobHeadPtr blob, char *buf);
};


class MSReferenceTable : public MSRepositoryTable {
public:
	MSReferenceTable(MSSystemTableShare *share, TABLE *table);
	virtual ~MSReferenceTable();

	void unuse();
	void seqScanInit();
	int	getRefLen();
	void seqScanPos(unsigned char *pos);
	virtual void seqScanRead(uint32_t repo, uint64_t offset, char *buf) { return MSRepositoryTable::seqScanRead(repo, offset, buf);}
	void seqScanRead(unsigned char *pos, char *buf);
	bool seqScanNext(char *buf);

private:
	MSRefDataPtr	iRefDataList;
	/* 
	 * I need my own copy of the current repository index and offset
	 * to ensure it referess to the same blob as the reference data position.
	*/
	uint32_t			iRefCurrentIndex;		
	uint64_t			iRefCurrentOffset;		
	uint32_t			iRefCurrentDataUsed;
	uint32_t			iRefCurrentDataPos;
	uint32_t			iRefDataSize;
	uint32_t			iRefDataUsed;
	uint32_t			iRefDataPos;
	uint32_t			iRefAuthCode;
	uint64_t			iRefBlobSize;
	uint32_t			iRefBlobRepo;
	uint64_t			iRefBlobOffset;
	MSOpenTable		*iRefOpenTable;
	MSTempLogFile	*iRefTempLog;

	virtual bool returnRecord(char *buf);
	virtual bool returnSubRecord(char *buf);
	virtual bool returnRow(MSBlobHeadPtr blob, char *buf) { return MSRepositoryTable::returnRow(blob, buf);}
	virtual void returnRow(MSRefDataPtr ref_data, char *buf);
	virtual bool resetScan(bool positioned, uint32_t iRepoIndex = 0);
};

class MSMetaDataTable : public MSRepositoryTable {
public:
	MSMetaDataTable(MSSystemTableShare *share, TABLE *table);
	virtual ~MSMetaDataTable();

	void use();
	void unuse();
	void seqScanInit();
	int	getRefLen();
	void seqScanPos(unsigned char *pos);
	virtual void seqScanRead(uint32_t repo, uint64_t offset, char *buf) { return MSRepositoryTable::seqScanRead(repo, offset, buf);}
	void seqScanRead(unsigned char *pos, char *buf);
	bool seqScanNext(char *buf);
	void insertRow(char *buf);
	void deleteRow(char *buf);
	void updateRow(char *old_data, char *new_data);

#ifdef HAVE_ALIAS_SUPPORT
	bool matchAlias(uint32_t repo_id, uint64_t offset, const char *alias);
#endif

	friend class MSSysMeta;

private:
	CSStringBuffer	*iMetData;
	uint32_t		iMetCurrentBlobRepo;		
	uint64_t		iMetCurrentBlobOffset;		
	uint32_t			iMetCurrentDataPos;
	uint32_t			iMetCurrentDataSize;
	uint32_t			iMetDataPos;
	uint32_t			iMetDataSize;
	uint32_t		iMetBlobRepo;
	uint64_t		iMetBlobOffset;
	uint8_t			iMetState[20];
	bool			iMetStateSaved;
	
	bool nextRecord(char **name, char **value);
	void seqScanReset();
	
	virtual bool returnRecord(char *buf);
	virtual bool returnSubRecord(char *buf);
	virtual bool returnRow(MSBlobHeadPtr blob, char *buf) { return MSRepositoryTable::returnRow(blob, buf);}
	virtual void returnRow(char *name, char *value, char *buf);
	virtual bool resetScan(bool positioned, uint32_t index = 0) {bool have_data= false; return resetScan(positioned, &have_data, index);}
	virtual bool resetScan(bool positioned, bool *have_data, uint32_t iRepoIndex = 0);
	
	static MSMetaDataTable *newMSMetaDataTable(MSDatabase *db);
};

class MSSystemTableShare : public CSRefObject {
public:
	CSString				*myTablePath;
	THR_LOCK				myThrLock;
	MSDatabase				*mySysDatabase;

	MSSystemTableShare();
	~MSSystemTableShare();

	/* Make this object sortable: */
	virtual CSObject *getKey();
	virtual int compareKey(CSObject *);

private:
	uint32_t					iOpenCount;

public:
	static CSSyncSortedList	*gSystemTableList;

	// Close all open system tables for the database being dropped.
	static void removeDatabaseSystemTables(MSDatabase *doomed_db); 
	static void startUp();
	static void shutDown();

	static MSOpenSystemTable *openSystemTable(const char *table_path, TABLE *table);
	static void releaseSystemTable(MSOpenSystemTable *tab);

	static MSSystemTableShare *newTableShare(CSString *table_path);
};

#endif
