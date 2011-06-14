/* Copyright (C) 2010 PrimeBase Technologies GmbH
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright notice, 
 *		this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, 
 *		this list of conditions and the following disclaimer in the documentation 
 *		and/or other materials provided with the distribution.
 *     * Neither the name of the "PrimeBase Technologies GmbH" nor the names of its 
 *		contributors may be used to endorse or promote products derived from this 
 *		software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 *  
 *
 * PrimeBase Media Stream for MySQL/Drizzle
 *
 * Barry Leslie
 *
 * 2009-07-16
 *
 * H&G2JCtL
 *
 * PBMS interface used to enable engines for use with the PBMS daemon.
 *
 * For an example on how to build this into an engine have a look at the PBXT engine
 * in file ha_pbxt.cc. Search for 'PBMS_ENABLED'.
 *
 */


#pragma once
#ifndef __PBMS_ENABLED_H__
#define __PBMS_ENABLED_H__

#include "pbms.h"

#ifdef DRIZZLED
#include <drizzled/common.h>
#define TABLE Table
#define uchar unsigned char
#else
#include <mysql_priv.h>
#endif

class Field;
typedef bool (*IsPBMSFilterFunc)(Field *field);

/*
 * pbms_initialize() should be called from the engines plugIn's 'init()' function.
 * The engine_name is the name of your engine, "PBXT" or "InnoDB" for example.
 *
 * The isServer flag indicates if this entire server is being enabled. This is only
 * true if this is being built into the server's handler code above the engine level
 * calls. 
 */
extern bool pbms_initialize(const char *engine_name, bool isServer, bool isTransactional, PBMSResultPtr result, IsPBMSFilterFunc is_pbms_blob);

/*
 * pbms_finalize() should be called from the engines plugIn's 'deinit()' function.
 */
extern void pbms_finalize(const char *engine_name);

/*
 * pbms_write_row_blobs() should be called from the engine's 'write_row' function.
 * It can alter the row data so it must be called before any other function using the row data.
 *
 * pbms_completed() must be called after calling pbms_write_row_blobs() and just before
 * returning from write_row() to indicate if the operation completed successfully.
 */
extern int pbms_write_row_blobs(const TABLE *table, unsigned char *buf, PBMSResultPtr result);

/*
 * pbms_update_row_blobs() should be called from the engine's 'update_row' function.
 * It can alter the row data so it must be called before any other function using the row data.
 *
 * pbms_completed() must be called after calling pbms_write_row_blobs() and just before
 * returning from write_row() to indicate if the operation completed successfully.
 */
extern int pbms_update_row_blobs(const TABLE *table, const unsigned char *old_row, unsigned char *new_row, PBMSResultPtr result);

/*
 * pbms_delete_row_blobs() should be called from the engine's 'delete_row' function.
 *
 * pbms_completed() must be called after calling pbms_delete_row_blobs() and just before
 * returning from delete_row() to indicate if the operation completed successfully.
 */
extern int pbms_delete_row_blobs(const TABLE *table, const unsigned char *buf, PBMSResultPtr result);

/*
 * pbms_rename_table_with_blobs() should be called from the engine's 'rename_table' function.
 *
 * NOTE: Renaming tables across databases is not supported.
 *
 * pbms_completed() must be called after calling pbms_rename_table_with_blobs() and just before
 * returning from rename_table() to indicate if the operation completed successfully.
 */
extern int pbms_rename_table_with_blobs(const char *old_table_path, const char *new_table_path, PBMSResultPtr result);

/*
 * pbms_delete_table_with_blobs() should be called from the engine's 'delete_table' function.
 *
 * NOTE: Currently pbms_delete_table_with_blobs() cannot be undone so it should only
 * be called after the host engine has performed successfully drop it's table.
 *
 * pbms_completed() must be called after calling pbms_delete_table_with_blobs() and just before
 * returning from delete_table() to indicate if the operation completed successfully.
 */
extern int pbms_delete_table_with_blobs(const char *table_path, PBMSResultPtr result);

/*
 * pbms_completed() must be called to indicate success or failure of a an operation after having
 * called  pbms_write_row_blobs(), pbms_delete_row_blobs(), pbms_rename_table_with_blobs(), or
 * pbms_delete_table_with_blobs().
 *
 * pbms_completed() has the effect of committing or rolling back the changes made if the session
 * is in 'autocommit' mode.
 */
extern void pbms_completed(const TABLE *table, bool ok);

#endif
