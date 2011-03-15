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
 * 2007-07-20
 *
 * H&G2JCtL
 *
 * Engine interface
 *
 */

#pragma once
#ifndef __ENGINE_MS_H__
#define __ENGINE_MS_H__

#include "defs_ms.h"
#include "pbms.h"

class MSOpenTable;

#ifdef DRIZZLED
namespace drizzled
{
class Session;
} /* namespace drizzled */
#else
class THD;
#endif

class MSEngine : public CSObject {
public:
	
#ifdef DRIZZLED
	static int startUp(PBMSResultPtr ) { return 0;}
	static void shutDown() {}
#else
	static int startUp(PBMSResultPtr result);

	static void shutDown();

	static const PBMSEnginePtr getEngineInfoAt(int indx);
#endif
	
	static int exceptionToResult(CSException *e, PBMSResultPtr result);
	static int errorResult(const char *func, const char *file, int line, int err, const char *message, PBMSResultPtr result);
	static int osErrorResult(const char *func, const char *file, int line, int err, PBMSResultPtr result);
	static int enterConnectionNoThd(CSThread **r_self, PBMSResultPtr result);
	static int enterConnection(THD *thd, CSThread **r_self, PBMSResultPtr result, bool doCreate);
	static void exitConnection();
	static void closeConnection(THD* thd);

	static int32_t	dropDatabase(const char *db_name, PBMSResultPtr result);
	static int32_t	createBlob(const char *db_name, const char *tab_name, char *blob, size_t blob_len, PBMSBlobURLPtr blob_url, PBMSResultPtr result);
	static int32_t	referenceBlob(const char *db_name, const char *tab_name, PBMSBlobURLPtr  ret_blob_url, char *blob_url, uint16_t col_index, PBMSResultPtr result);
	static int32_t	dereferenceBlob(const char *db_name, const char *tab_name, char *blob_url, PBMSResultPtr result);
	static int32_t	dropTable(const char *db_name, const char *tab_name, PBMSResultPtr result);
	static int32_t	renameTable(const char *from_db_name, const char *from_table, const char *to_db_name, const char *to_table, PBMSResultPtr result);
	static void		callCompleted(bool ok);
	
	static bool couldBeURL(const char *url, size_t length);
	
	private:
	static bool try_createBlob(CSThread *self, const char *db_name, const char *tab_name, char *blob, size_t blob_len, PBMSBlobURLPtr blob_url);
	static bool try_referenceBlob(CSThread *self, const char *db_name, const char *tab_name, PBMSBlobURLPtr ret_blob_url, char *blob_url, uint16_t col_index);
	static bool try_dereferenceBlob(CSThread *self, const char *db_name, const char *tab_name, char *blob_url);
	static bool try_dropDatabase(CSThread *self, const char *db_name);
	static bool try_dropTable(CSThread *self, const char *db_name, const char *tab_name);
	static bool try_renameTable(CSThread *self, const char *from_db_name, const char *from_table, const char *to_db_name, const char *to_table);
	
	static MSOpenTable *openTable(const char *db_name, const char *tab_name, bool create);
	static bool renameTable(const char *db_name, const char *from_table, const char *to_db_name, const char *to_table);
	static void completeRenameTable(struct UnDoInfo *info, bool ok);
};

#endif
