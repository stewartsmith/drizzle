#ifndef DRIZZLED
//#define SUPPORT_PBMS_TRIGGERS
#ifdef SUPPORT_PBMS_TRIGGERS
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
 * 2008-09-11
 *
 * H&G2JCtL
 *
 * User Defined Functions for use in triggers for non PBMS enabled engines.
 *
 */

#include "cslib/CSConfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#include "mysql_priv.h"

#include <mysql.h>
#include <ctype.h>

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSThread.h"

#include "engine_ms.h"

#ifdef MOVE_THIS_TO_ITS_OWN_FILE
bool pbms_enabled(const char *name)
{
	bool found = false;
	PBMSSharedMemoryPtr	sh_mem = StreamingEngines->sharedMemory;
	PBMSEnginePtr		engine;
	
	if (sh_mem) {
		for (int i = 0; i<sh_mem->sm_list_len && !found; i++) {
			if (engine = sh_mem->sm_engine_list[i]) {
				found = (strcasecmp(name, engine->ms_engine_name) == 0);
			}
		}
	}
	
	return found;
}

// =============================
#define PBMS_ENGINE_NAME_LEN 64 // This should be big enough.
bool pbms_engine_is_enabled(char *name, size_t len)
{
	bool found = false;

	if (len < PBMS_ENGINE_NAME_LEN) {
		char engine_name[PBMS_ENGINE_NAME_LEN];
		memcpy(engine_name, name, len);
		engine_name[len] = 0;
		
		found = pbms_enabled(engine_name);	
	}
	
	return found;
}

// =============================
static bool trig_open_table(void **open_table, char *table_path, PBMSResultPtr result)
{	
	return do_open_table(open_table, table_path, result);
}

char *pbms_trig_use_blob(const char *database, size_t db_len, const char *table, size_t tab_len, unsigned short col_position, char *url, size_t url_len, char *out_url, PBMSResultPtr result)
{
	void *open_table = NULL;
	int rtc;
	char blob_url[PBMS_BLOB_URL_SIZE], *ret_blob_url;
	char table_path[PATH_MAX];
	
	if (url_len >= PBMS_BLOB_URL_SIZE) {
		pbms_error_result(CS_CONTEXT, MS_ERR_INCORRECT_URL, "Incorrect URL", result);
		return NULL; 
	}
	
	if (pbms_table_path(database, db_len, table, tab_len,  table_path, result) || trig_open_table(&open_table, table_path, result))
		return NULL;
	
	memcpy(blob_url, url, url_len);
	blob_url[url_len] = 0;
	
	// Internally col_position are '0' based for consistency with the internal PBXT calls to PBMS.
	if (!(rtc = ms_use_blob(open_table, &ret_blob_url, blob_url, col_position -1, result))) {		
		if (!(rtc = ms_retain_blobs(open_table,  result)))
			cs_strcpy(PBMS_BLOB_URL_SIZE, out_url, ret_blob_url);
	}
	
	ms_close_table(open_table);
	
	if (rtc)
		return NULL;
	
	return out_url;	
}

// =============================
char *pbms_use_blob(const char *database, const char *table, unsigned short col_position, char *url, size_t url_len, char *out_url, PBMSResultPtr result)
{
	return pbms_trig_use_blob(database, strlen(database), table, strlen(table), col_position, url, url_len, out_url, result);
}

// =============================
int pbms_trig_release_blob(const char *database, size_t db_len, const char *table, size_t tab_len, unsigned short col_position, const char *url, size_t url_len, PBMSResultPtr result)
{
	void *open_table = NULL;
	int rtc;
	char blob_url[PBMS_BLOB_URL_SIZE];
	char table_path[PATH_MAX];
	
	if (url_len >= PBMS_BLOB_URL_SIZE)
		return 0; // url is too long to be a valid blob url.

	if (pbms_table_path(database, db_len, table, tab_len,  table_path, result) || trig_open_table(&open_table, table_path, result))
		return 1;
	
	memcpy(blob_url, url, url_len);
	blob_url[url_len] = 0;
	
	rtc = ms_release_blob(open_table, blob_url, result);
	
	ms_close_table(open_table);
	
	return rtc;	
}

// =============================
int pbms_release_blob(const char *database, const char *table, unsigned short col_position, const char *url, size_t url_len, PBMSResultPtr result)
{
	return pbms_trig_release_blob(database, strlen(database), table, strlen(table), col_position, url, url_len, result);
}

// =============================
int pbms_new_blob(const char *database, const char *table, unsigned short col_position, unsigned char *blob, size_t blob_len, char *out_url, PBMSResultPtr result)
{
	void *open_table = NULL;
	char table_path[PATH_MAX];
	int rtc;
	
	if (pbms_table_path(database, strlen(database), table,  strlen(table),  table_path, result) || trig_open_table(&open_table, table_path, result))
		return 1;
		
	rtc =  ms_create_blob(open_table, blob, blob_len, out_url, col_position -1,  result);
		
	ms_close_table(open_table);
	
	return rtc;	
}

// =============================
int pbms_trig_drop_table(const char *database, size_t db_len, const char *table, size_t tab_len, PBMSResultPtr result)
{
	char table_path[PATH_MAX];
	
	if (pbms_table_path(database, db_len, table, tab_len,  table_path, result))
		return 1;
	
	return ms_drop_table(table_path, result);
}

// =============================
int pbms_drop_table(const char *database, const char *table, PBMSResultPtr result)
{
	return pbms_trig_drop_table(database, strlen(database), table, strlen(table), result);
}

// =============================
int pbms_trig_rename_table(const char *database, size_t db_len, const char *o_table, size_t o_tab_len, const char *n_table, size_t n_tab_len, PBMSResultPtr result)
{
	char o_table_path[PATH_MAX], n_table_path[PATH_MAX];
	
	if (pbms_table_path(database, db_len, o_table, o_tab_len,  o_table_path, result) || pbms_table_path(database, db_len, n_table, n_tab_len,  n_table_path, result) )
		return 1;
	
	return ms_rename_table(o_table_path, n_table_path, result);
}

// =============================
int pbms_rename_table(const char *database, const char *o_table,  const char *n_table, PBMSResultPtr result)
{
	return pbms_trig_rename_table(database, strlen(database), o_table, strlen(o_table), n_table, strlen(n_table), result);
}

#endif MOVE_THIS_TO_ITS_OWN_FILE

extern "C" {
my_bool pbms_insert_blob_trig_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *pbms_insert_blob_trig(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *res_length, char *is_null, char *error);
void pbms_insert_blob_trig_deinit(UDF_INIT *initid);

//-----------
my_bool pbms_update_blob_trig_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *pbms_update_blob_trig(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *res_length, char *is_null, char *error);
void pbms_update_blob_trig_deinit(UDF_INIT *initid);

//-----------
my_bool pbms_delete_blob_trig_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
longlong pbms_delete_blob_trig(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);

//-----------
my_bool pbms_delete_all_blobs_in_table_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
longlong pbms_delete_all_blobs_in_table(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);

//-----------
my_bool pbms_rename_table_with_blobs_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
longlong pbms_rename_table_with_blobs(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);

//-----------
my_bool pbms_enabled_engine_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
longlong pbms_enabled_engine(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
}

static void report_udf_error(UDF_INIT *initid __attribute__((unused)), char *error, const char *func, const char *message)
{
	*error = 1;
	
	// I wish there were a way to pass the error text up to the caller but I do not know if/how it can be done.
	// So for now just send it to stderr.
	fprintf(stderr, "PBMS UDF Error (%s): %s\n", func,  message);
}

static char *local_reference_blob(UDF_INIT *initid, char *database, size_t db_len, char *table, size_t tab_len, unsigned short col_position, char *url, size_t url_len, char *result, char *error, const char *func)
{
	char *out_url = NULL, blob_url[PBMS_BLOB_URL_SIZE];
	PBMSResultRec pbmsResult = {0};
	
	out_url = pbms_trig_use_blob(database, db_len, table, tab_len, col_position, url, url_len, blob_url, &pbmsResult); 

	if (out_url) {
		size_t url_len = strlen(out_url) +1;
		if (url_len < 255) {
			cs_strcpy(255, result,  out_url);
			out_url = result;
		} else {
			initid->ptr = (char*) malloc(url_len);
			if (initid->ptr) {
				cs_strcpy(url_len, initid->ptr,  out_url);
				out_url = initid->ptr;
			} else {
				report_udf_error(initid, error, func, "Couldn't allocate memory");
				out_url = NULL;
			}
		}
	} else if  (pbmsResult.mr_code == MS_ERR_INCORRECT_URL) 
		out_url = url; // Not a URL so just return it so that it is inserted as is.
	else 
		report_udf_error(initid, error, func, pbmsResult.mr_message);
	
	
	return out_url;
}

//======================================================================
// CREATE FUNCTION pbms_insert_blob_trig RETURNS STRING SONAME "libpbms.so";
// pbms_insert_blob_trig(database, table, col_position, blob_url);
my_bool pbms_insert_blob_trig_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	if (args->arg_count != 4 || args->arg_type[0] != STRING_RESULT || args->arg_type[1] != STRING_RESULT || args->arg_type[2] != INT_RESULT || args->arg_type[3] != STRING_RESULT)
	{
		strcpy(message,"Wrong arguments to pbms_insert_blob_trig()");
		return 1;
	}
	args->maybe_null[0] = 0;
	args->maybe_null[1] = 0;
	args->maybe_null[2] = 0;
	args->maybe_null[3] = 1;

	initid->max_length=PBMS_BLOB_URL_SIZE;
	initid->maybe_null=1;
	initid->ptr=NULL;
	return 0;
}

void pbms_insert_blob_trig_deinit(UDF_INIT *initid)
{
	if (initid->ptr)
		free(initid->ptr);
}

#define INT_ARG(a)  (*((longlong*) a))

char *pbms_insert_blob_trig(UDF_INIT *initid, UDF_ARGS *args,
                    char *result, unsigned long *res_length, char *is_null,
                    char *error)
{
	char *out_url;
	
	*is_null=1;
	
	// The first parameter is the table name which should never be NULL or an empty string.
	if (!args->args[0] || !args->lengths[0] || !args->args[1] || !args->lengths[1]) {
		report_udf_error(initid, error, __FUNC__, "Bad arguments");
		return NULL;
	}

	if (!args->args[3] || !args->lengths[3]) {
		return NULL;
	}
	
	out_url = local_reference_blob(initid, args->args[0], args->lengths[0], args->args[1], args->lengths[1], INT_ARG(args->args[2]), args->args[3], args->lengths[3], result, error, __FUNC__); 	
	if (!out_url) 
		return NULL;

	*is_null=0;
	*res_length = strlen(out_url);
	return out_url;	
}

//======================================================================
// CREATE FUNCTION pbms_update_blob_trig RETURNS STRING SONAME "libpbms.so";
// pbms_update_blob_trig(database, table, col_position, old_blob_url, new_blob_url);
my_bool pbms_update_blob_trig_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	if (args->arg_count != 5 || args->arg_type[0] != STRING_RESULT || args->arg_type[1] != STRING_RESULT || args->arg_type[2] != INT_RESULT || args->arg_type[3] != STRING_RESULT || args->arg_type[4] != STRING_RESULT)
	{
		strcpy(message,"Wrong arguments to pbms_update_blob_trig()");
		return 1;
	}
	args->maybe_null[0] = 0;
	args->maybe_null[1] = 0;
	args->maybe_null[2] = 0;
	args->maybe_null[3] = 1;
	args->maybe_null[4] = 1;
	
	initid->maybe_null=1;
	initid->ptr=NULL;
	return 0;
}

void pbms_update_blob_trig_deinit(UDF_INIT *initid)
{
	free(initid->ptr);
}

char *pbms_update_blob_trig(UDF_INIT *initid, UDF_ARGS *args,
                    char *result, unsigned long *res_length, char *is_null,
                    char *error)
{
	char *out_url = NULL;
	
	
	// The first parameter is the table name which should never be NULL or an empty string.
	if (!args->args[0] || !args->lengths[0] || !args->args[1] || !args->lengths[1]) {
		report_udf_error(initid, error, __FUNC__, "Bad arguments");
		return NULL;
	}
	
	// Check to see if the blob url changed
	if (args->lengths[2] == args->lengths[3] && !memcmp(args->args[2], args->args[3], args->lengths[3])) {
		if (args->lengths[2]) {
			*is_null=0;
			*res_length = args->lengths[2];
			return args->args[2];
		}
		
		*is_null=1;
		return NULL;
	}
	
	
	if (args->lengths[4] && args->args[4]) { // Reference the new blob.
		out_url = local_reference_blob(initid, args->args[0], args->lengths[0], args->args[1], args->lengths[1], INT_ARG(args->args[2]), args->args[4], args->lengths[4], result, error, __FUNC__); 
		if (!out_url) 
			return 0;
	}
	
	if (args->lengths[3] && args->args[3]) { // Dereference the old blob
		PBMSResultRec pbmsResult = {0};
		if (pbms_trig_release_blob(args->args[0], args->lengths[0], args->args[1], args->lengths[1], INT_ARG(args->args[2]), args->args[3], args->lengths[3], &pbmsResult)) {
			report_udf_error(initid, error,  __FUNC__, pbmsResult.mr_message);
			if (out_url)
				pbms_trig_release_blob(args->args[0], args->lengths[0], args->args[1], args->lengths[1], INT_ARG(args->args[2]), out_url, strlen(out_url), &pbmsResult);
			return NULL;
		} 
	}
	
	if (out_url) {
		*is_null=0;
		*res_length = strlen(out_url);
	}
	
	return out_url;	
}

//======================================================================
// CREATE FUNCTION pbms_delete_blob_trig RETURNS INT SONAME "libpbms.so";
// pbms_delete_blob_trig(database, table, col_position, blob_url);
my_bool pbms_delete_blob_trig_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	if (args->arg_count != 4 || args->arg_type[0] != STRING_RESULT || args->arg_type[1] != STRING_RESULT || args->arg_type[2] != INT_RESULT || args->arg_type[3] != STRING_RESULT)
	{
		strcpy(message,"Wrong arguments to pbms_delete_blob_trig()");
		return 1;
	}
	args->maybe_null[0] = 0;
	args->maybe_null[1] = 0;
	args->maybe_null[2] = 0;
	args->maybe_null[3] = 1;	
	initid->maybe_null=0;
	
	return 0;
}

longlong pbms_delete_blob_trig(UDF_INIT *initid, UDF_ARGS *args, char *is_null __attribute__((unused)), char *error)
{
	PBMSResultRec pbmsResult = {0};
	
	// The first parameter is the table name which should never be NULL or an empty string.
	if (!args->args[0] || !args->lengths[0] || !args->args[1] || !args->lengths[1]) {
		report_udf_error(initid, error, __FUNC__, "Bad arguments");
		return 1;
	}
	
	if (!args->args[3] || !args->lengths[3]) {
		return 0; //Dropping a NULL blob.
	}
	
	if (! pbms_trig_release_blob(args->args[0], args->lengths[0], args->args[1], args->lengths[1], INT_ARG(args->args[2]), args->args[3], args->lengths[3], &pbmsResult))
		return  0;
	
	report_udf_error(initid, error,  __FUNC__, pbmsResult.mr_message);
	return 1;
}


//======================================================================
// CREATE FUNCTION pbms_delete_all_blobs_in_table RETURNS INT SONAME "libpbms.so";
// pbms_delete_all_blobs_in_table(database, table);
my_bool pbms_delete_all_blobs_in_table_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	if (args->arg_count != 2 || args->arg_type[0] != STRING_RESULT || args->arg_type[1] != STRING_RESULT)
	{
		strcpy(message,"Wrong arguments to pbms_delete_all_blobs_in_table()");
		return 1;
	}
	args->maybe_null[0] = 0;
	args->maybe_null[1] = 0;
	initid->maybe_null=0;
	
	return 0;
}

longlong pbms_delete_all_blobs_in_table(UDF_INIT *initid, UDF_ARGS *args, char *is_null __attribute__((unused)), char *error)
{
	PBMSResultRec pbmsResult = {0};
	
	// The first parameter is the table name which should never be NULL or an empty string.
	if (args->arg_count != 2 || !args->args[0] || !args->lengths[0] || !args->args[1] || !args->lengths[1]) {
		report_udf_error(initid, error, __FUNC__, "Bad arguments");
		return 1;
	}
	
	if (!pbms_trig_drop_table(args->args[0], args->lengths[0], args->args[1], args->lengths[1], &pbmsResult)) 
		return 0;
	
	report_udf_error(initid, error,  __FUNC__, pbmsResult.mr_message);
	return 1;
}

//======================================================================
// CREATE FUNCTION pbms_rename_table_with_blobs RETURNS INT SONAME "libpbms.so";
// pbms_rename_table_with_blobs(database, old_table, new_table);
my_bool pbms_rename_table_with_blobs_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	if (args->arg_count != 3 || args->arg_type[0] != STRING_RESULT || args->arg_type[1] != STRING_RESULT || args->arg_type[2] != STRING_RESULT)
	{
		strcpy(message,"Wrong arguments to pbms_rename_table_with_blobs()");
		return 1;
	}
	args->maybe_null[0] = 0;
	args->maybe_null[1] = 0;
	args->maybe_null[2] = 0;
	initid->maybe_null=0;
	
	return 0;
}

longlong pbms_rename_table_with_blobs(UDF_INIT *initid, UDF_ARGS *args, char *is_null __attribute__((unused)), char *error)
{
	PBMSResultRec pbmsResult = {0};
	
	// The first parameter is the table name which should never be NULL or an empty string.
	if (args->arg_count != 3 || !args->args[0] || !args->lengths[0] || !args->args[1] || !args->lengths[1] || !args->args[2] || !args->lengths[2]) {
		report_udf_error(initid, error, __FUNC__, "Bad arguments");
		return 1;
	}
	
	if (!pbms_trig_rename_table(args->args[0], args->lengths[0], args->args[1], args->lengths[1], args->args[2], args->lengths[2], &pbmsResult)) 
		return 0;
	
	report_udf_error(initid, error,  __FUNC__, pbmsResult.mr_message);
	return 1;
}

//======================================================================
// CREATE FUNCTION pbms_enabled_engine RETURNS INT SONAME "libpbms.so";
// pbms_enabled_engine(database, table);
my_bool pbms_enabled_engine_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
	{
		strcpy(message,"Wrong arguments to pbms_enabled_engine()");
		return 1;
	}
	args->maybe_null[0] = 0;
	initid->maybe_null=0;
	
	return 0;
}

longlong pbms_enabled_engine(UDF_INIT *initid, UDF_ARGS *args, char *is_null __attribute__((unused)), char *error)
{
	// The first parameter is the engine name which should never be NULL or an empty string.
	if (args->arg_count != 1 || !args->args[0] || !args->lengths[0]) {
		report_udf_error(initid, error, __FUNC__, "Bad arguments");
		return -1;
	}
	
	return pbms_engine_is_enabled(args->args[0], args->lengths[0]);
}

#endif // SUPPORT_PBMS_TRIGGERS
#endif // DRIZZLED
