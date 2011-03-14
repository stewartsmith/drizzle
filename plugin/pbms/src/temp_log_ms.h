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
 * 2007-07-03
 *
 * H&G2JCtL
 *
 * Temporary BLOB log.
 *
 * Temporary BLOBs are BLOBs that are to be deleted after an certain
 * expiry time.
 *
 * The temporary log is also used to schedule asynchronous operations to be performed
 * on the BLOB such as uploading it to a cloud.
 *
 * Temporary BLOBs are referenced by the temporary log.
 */

#pragma once
#ifndef __TEMPLOG_MS_H__
#define __TEMPLOG_MS_H__

#include "cslib/CSDefs.h"
#include "cslib/CSFile.h"
#include "cslib/CSStream.h"

#include "defs_ms.h"

class MSOpenTable;
class MSDatabase;
class MSTempLog;

// Change history:
// April 6 2009:
// Changed MS_TEMP_LOG_MAGIC and MS_TEMP_LOG_VERSION
// when the disk size of ti_type_1 was changed from
// CSDiskValue4 to CSDiskValue1.

#define MS_TEMP_LOG_MAGIC			0xF9E6D7C9
#define MS_TEMP_LOG_VERSION			3
#define MS_TEMP_LOG_HEAD_SIZE		32

#define MS_TL_BLOB_REF				1
#define MS_TL_REPO_REF				2		
#define MS_TL_TABLE_REF				3

typedef struct MSTempLogHead {
	CSDiskValue4			th_magic_4;							/* Table magic number. */
	CSDiskValue2			th_version_2;						/* The header version. */
	CSDiskValue2			th_head_size_2;						/* The size of the header. */
	CSDiskValue2			th_rec_size_2;						/* The size of a temp log record. */
	CSDiskValue4			th_reserved_4;
} MSTempLogHeadRec, *MSTempLogHeadPtr;

typedef struct MSTempLogItem {
	CSDiskValue1			ti_type_1;							/* 1 = BLOB reference, 2 = Repository reference, 3 = Table reference */
	CSDiskValue4			ti_table_id_4;						/* Table ID (non-zero if valid). */
	CSDiskValue6			ti_blob_id_6;						/* Blob ID (non-zero if valid). */
	CSDiskValue4			ti_auth_code_4;						/* To make sure we do not delete the wrong thing. */
	CSDiskValue4			ti_time_4;							/* The time of deletion/creation */
} MSTempLogItemRec, *MSTempLogItemPtr;

class MSTempLogFile : public CSReadBufferedFile {
public:
	uint32_t		myTempLogID;
	MSTempLog	*myTempLog;

	MSTempLogFile();
	~MSTempLogFile();

	friend class MSTempLog;

private:
	static MSTempLogFile *newTempLogFile(uint32_t id, MSTempLog *temp_log, CSFile *path);
};

class MSTempLog : public CSRefObject {
public:
	uint32_t		myLogID;
	off64_t		myTempLogSize;
	int			myTemplogRecSize;
	size_t		myTempLogHeadSize;

	MSTempLog(uint32_t id, MSDatabase *db, off64_t file_size);
	virtual ~MSTempLog();

	void deleteLog();
	CSPath *getLogPath();
	MSTempLogFile *openTempLog();
	
#ifdef DEBUG
//	virtual void retain() {
//		CSRefObject::retain();
//		printf("MSTempLog retained %d\n", iRefCount);
//	}
//
//	virtual void release() {
//		printf("MSTempLog released %d\n", iRefCount);
//		CSRefObject::release();
//	}
#endif

	friend class MSTempLogThread;

private:
	MSDatabase		*iLogDatabase;
	bool			iDeleteLog;

public:

	static time_t adjustWaitTime(time_t then, time_t now);
};

class MSTempLogThread : public CSDaemon {
public:
	MSTempLogThread(time_t wait_time, MSDatabase *db);
	virtual ~MSTempLogThread(){} // Do nothing here because 'self' will no longer be valid, use completeWork().

	void close();

	virtual bool doWork();

	virtual void *completeWork();

private:
	bool try_ReleaseBLOBReference(CSThread *self, CSStringBuffer *buffer, uint32_t tab_id, int type, uint64_t blob_id, uint32_t auth_code);

	MSDatabase			*iTempLogDatabase;
	MSTempLogFile		*iTempLogFile;
	size_t				iLogRecSize;
	off64_t				iLogOffset;
	
};

#endif
