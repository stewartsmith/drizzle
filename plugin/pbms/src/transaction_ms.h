/* Copyright (C) 2009 PrimeBase Technologies GmbH, Germany
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
 * 2009-07-09
 *
 * H&G2JCtL
 *
 * PBMS transaction daemon.
 *
 *
 */
 
#pragma once
#ifndef __TRANSACTION_MS_H__
#define __TRANSACTION_MS_H__
#include "cslib/CSDefs.h"

class MSTrans;
class MSTransactionThread;

class MSTransactionManager {
public:
	MSTransactionManager(){}
	
	static void startUp();
	static void shutDown();
	static void flush();
	static void suspend(bool do_flush = false);
	static void resume();
	static void commit();
	static void rollback();
	static void rollbackToPosition(uint32_t position);
	
#ifdef DRIZZLED
	static void setSavepoint(const char *savePoint);
	static void releaseSavepoint(const char *savePoint);
	static void rollbackTo(const char *savePoint);
#endif

	static void referenceBLOB(uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id)
	 {
		logTransaction(true, db_id, tab_id, blob_id, blob_ref_id);
	 }
	static void dereferenceBLOB(uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id)
	 {
		logTransaction(false, db_id, tab_id, blob_id, blob_ref_id);
	 }
	
	static void dropDatabase(uint32_t db_id);
private:
	static void startUpReader();
	static void logTransaction(bool ref, uint32_t db_id, uint32_t tab_id, uint64_t blob_id, uint64_t blob_ref_id);
	static MSTransactionThread *tm_Reader;
	
	friend class  MSTempLogThread;
	static MSTrans *tm_Log;
	
};

#endif
