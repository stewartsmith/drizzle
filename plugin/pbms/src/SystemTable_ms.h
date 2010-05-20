/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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

#ifndef __SYSTEMTABLE_MS_H__
#define __SYSTEMTABLE_MS_H__

#include "CSMutex.h"

#include "Defs_ms.h"
#include "Repository_ms.h"
#include "cloud.h"

#if MYSQL_VERSION_ID < 50114
#define SET_FIELD(f, i, t, b) {f = t->field[i]; f->ptr = (byte *)b + f->offset();}
#else
#define SET_FIELD(f, i, t, b) {f = t->field[i]; f->ptr = (byte *)b + f->offset(f->table->record[0]);}
#endif
#define GET_INT_FIELD(v, i, t, b) {byte *s; Field *f; s = t->field[i]->ptr; SET_FIELD(f, i, t, b); v = f->val_int(); f->ptr =s;}
#define GET_STR_FIELD(v, i, t, b) {byte *s; Field *f; s = t->field[i]->ptr; SET_FIELD(f, i, t, b); v = f->val_str(v); f->ptr =s;}

#ifdef DRIZZLED
#include <drizzled/server_includes.h>
#include <drizzled/message/table.pb.h>
#include <drizzled/table_proto.h>

int pbms_discover_system_tables(const char *name, drizzled::message::Table *table);
#else
int pbms_discover_system_tables(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen);
#endif

const char *pbms_getSysTableName(int i);
bool pbms_is_Systable(const char *name);

void pbms_transfer_ststem_tables(MSDatabase *dst_db, MSDatabase *src_db);
void pbms_load_system_tables(MSDatabase *db);
void pbms_remove_ststem_tables(CSString *path);

CSStringBuffer *pbms_dump_system_tables(MSDatabase *db);
void pbms_restore_system_tables(MSDatabase *db, const char *data, size_t size);

void pbmsSystemTablesStartUp();
void pbmsSystemTableShutDown();

void ms_my_set_notnull_in_record(Field *field, char *record);

class MSSystemTableShare;
class MSDatabase;
class CSDaemon;
class MSTempLogFile;

class MSOpenSystemTable : public CSRefObject {
public:
	MSSystemTableShare		*myShare;
	TABLE					*mySQLTable;

	MSOpenSystemTable(MSSystemTableShare *share, TABLE *table);
	virtual ~MSOpenSystemTable();

	virtual void use() { }
	virtual void unuse() { }
	virtual void backupSeqScanInit() { }
	virtual void seqScanInit() { }
	virtual bool seqScanNext(char *buf) { return false; }
	virtual int	getRefLen() { return 0; }
	virtual void seqScanPos(uint8_t *pos) { }
	virtual void seqScanRead(uint8_t *pos, char *buf) { }
	virtual void insertRow(char *buf) { }
	virtual void deleteRow(char *buf) {  }
	virtual void updateRow(char *old_data, char *new_data) { }
	
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
	
private:
};

typedef struct MSRefData {
	u_int				rd_ref_count;
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
	virtual void seqScanPos(uint8_t *pos);
	virtual void seqScanRead(uint32_t repo, uint64_t offset, char *buf);
	virtual void seqScanRead(uint8_t *pos, char *buf);

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
	void seqScanPos(uint8_t *pos);
	void seqScanRead(uint8_t *pos, char *buf);
	bool seqScanNext(char *buf);

private:
	MSRefDataPtr	iRefDataList;
	/* 
	 * I need my own copy of the current repository index and offset
	 * to ensure it referess to the same blob as the reference data position.
	*/
	uint32_t			iRefCurrentIndex;		
	uint64_t			iRefCurrentOffset;		
	u_int			iRefCurrentDataUsed;
	u_int			iRefCurrentDataPos;
	u_int			iRefDataSize;
	u_int			iRefDataUsed;
	u_int			iRefDataPos;
	uint32_t			iRefAuthCode;
	uint64_t			iRefBlobSize;
	uint32_t			iRefBlobRepo;
	uint64_t			iRefBlobOffset;
	MSOpenTable		*iRefOpenTable;
	MSTempLogFile	*iRefTempLog;

	virtual bool returnRecord(char *buf);
	virtual bool returnSubRecord(char *buf);
	virtual void returnRow(MSRefDataPtr ref_data, char *buf);
	virtual bool resetScan(bool positioned, uint32_t iRepoIndex = 0);
};

class MSMetaDataTable : public MSRepositoryTable {
public:
	MSMetaDataTable(MSSystemTableShare *share, TABLE *table);
	virtual ~MSMetaDataTable();

	void unuse();
	void seqScanInit();
	int	getRefLen();
	void seqScanPos(uint8_t *pos);
	void seqScanRead(uint8_t *pos, char *buf);
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
	uint32_t			iMetCurrentBlobRepo;		
	uint64_t			iMetCurrentBlobOffset;		
	u_int			iMetCurrentDataPos;
	u_int			iMetCurrentDataSize;
	u_int			iMetDataPos;
	u_int			iMetDataSize;
	uint32_t			iMetBlobRepo;
	uint64_t			iMetBlobOffset;
	uint8_t			iMetState[20];
	bool			iMetStateSaved;
	
	bool nextRecord(char **name, char **value);
	void seqScanReset();
	
	virtual bool returnRecord(char *buf);
	virtual bool returnSubRecord(char *buf);
	virtual void returnRow(char *name, char *value, char *buf);
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
	u_int					iOpenCount;

public:
	static CSSyncSortedList	*gSystemTableList;

	static void startUp();
	static void shutDown();

	static MSOpenSystemTable *openSystemTable(const char *table_path, TABLE *table);
	static void releaseSystemTable(MSOpenSystemTable *tab);

	static MSSystemTableShare *newTableShare(CSString *table_path);
};

#endif
