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
 * PrimeBase Media Stream for MySQL and Drizzle
 *
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

#ifndef DRIZZLED
#if defined(MSDOS) || defined(__WIN__)
#include "pbms_enabled.h"

// Windows is not supported yet so just stub out the functions..
bool pbms_initialize(const char *engine_name __attribute__((unused)), 
					bool isServer __attribute__((unused)), 
					bool isTransactional __attribute__((unused)), 
					PBMSResultPtr result __attribute__((unused)), 
					IsPBMSFilterFunc is_pbms_blob __attribute__((unused))
					) { return true;}
void pbms_finalize() {}
int pbms_write_row_blobs(const TABLE *table __attribute__((unused)), 
						unsigned char *buf __attribute__((unused)), 
						PBMSResultPtr result __attribute__((unused))
						){ return 0;}
int pbms_update_row_blobs(const TABLE *table __attribute__((unused)), 
						const unsigned char *old_row __attribute__((unused)), 
						unsigned char *new_row __attribute__((unused)), 
						PBMSResultPtr result __attribute__((unused))
						){ return 0;}
int pbms_delete_row_blobs(const TABLE *table __attribute__((unused)), 
						const unsigned char *buf __attribute__((unused)), 
						PBMSResultPtr result __attribute__((unused))
						){ return 0;}
int pbms_rename_table_with_blobs(const char *old_table_path __attribute__((unused)), 
								const char *new_table_path __attribute__((unused)), 
								PBMSResultPtr result __attribute__((unused))
								){ return 0;}
int pbms_delete_table_with_blobs(const char *table_path __attribute__((unused)), 
								PBMSResultPtr result __attribute__((unused))
								){ return 0;}
void pbms_completed(TABLE *table __attribute__((unused)), 
					bool ok __attribute__((unused))
					){}
#else
#define PBMS_API	pbms_enabled_api

#include "pbms_enabled.h"
#include "mysql_priv.h"
#include <mysql/plugin.h>
#define session_alloc(sess, size) thd_alloc(sess, size);
#define current_session current_thd

#define GET_BLOB_FIELD(t, i) (Field_blob *)(t->field[t->s->blob_field[i]])
#define DB_NAME(f) (f->table->s->db.str)
#define TAB_NAME(f) (*(f->table_name))

static PBMS_API pbms_api;

/* 
 * A callback function to check if the column is a PBMS BLOB. 
 * Can be NULL if no check is to be done. 
 */ 
static IsPBMSFilterFunc	is_pbms_blob = NULL; 

//====================
bool pbms_initialize(const char *engine_name, bool isServer, bool isTransactional, PBMSResultPtr result, IsPBMSFilterFunc is_pbms_blob_arg)
{
	int	err;
	PBMSEngineRec enabled_engine = {
		MS_ENGINE_VERSION,
		0,
		0,
		0,
		{0},	
		0
	};

	strncpy(enabled_engine.ms_engine_name, engine_name, 32);
	enabled_engine.ms_internal = isServer;
	enabled_engine.ms_has_transactions = isTransactional;
	enabled_engine.ms_engine_name[31] = 0;

	err = pbms_api.registerEngine(&enabled_engine, result);
	is_pbms_blob = is_pbms_blob_arg;

	return (err == 0);
}


//====================
void pbms_finalize(const char *engine_name)
{
	pbms_api.deregisterEngine(engine_name);
}

//==================================
static int insertRecord(Field_blob *field, char *blob,  size_t org_length, unsigned char *blob_rec, size_t packlength, PBMSResultPtr result)
{
	int err;
	size_t length;
	PBMSBlobURLRec blob_url;
	
	err = pbms_api.retainBlob(DB_NAME(field), TAB_NAME(field), &blob_url, blob, org_length, field->position(), result);
	if (err)
		return err;
		
	// If the BLOB length changed reset it. 
	// This will happen if the BLOB data was replaced with a BLOB reference. 
	length = strlen(blob_url.bu_data)  +1;
	if ((length != org_length) || memcmp(blob_url.bu_data, blob, length)) {
		if (length != org_length) {
			field->store_length(blob_rec, packlength, length);
		}
		
		if (length > org_length) {
			// This can only happen if the BLOB URL is actually larger than the BLOB itself.
			blob = (char *) session_alloc(current_session, length);
			memcpy(blob_rec+packlength, &blob, sizeof(char*));
		}			
		memcpy(blob, blob_url.bu_data, length);
	} 
		
	return 0;
}

//====================
int pbms_update_row_blobs(const TABLE *table, const unsigned char *old_row, unsigned char *new_row, PBMSResultPtr result)
{
	Field_blob *field;
	uint32_t field_offset;
	const unsigned char *old_blob_rec;
	unsigned char *new_blob_rec;
	char *old_blob_url, *new_blob_url;
	size_t packlength, i, old_length, new_length;
	int err;
	bool old_null_blob, new_null_blob;

	result->mr_had_blobs = false;
	
	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	if (table->s->blob_fields == 0)
		return 0;
		
	for (i= 0; i < table->s->blob_fields; i++) {
		field = GET_BLOB_FIELD(table, i);

		old_null_blob = field->is_null_in_record(old_row);
		new_null_blob = field->is_null_in_record(new_row);
		if (old_null_blob && new_null_blob)
			continue;

		{
			String type_name;
			// Note: field->type() always returns MYSQL_TYPE_BLOB regardless of the type of BLOB
			field->sql_type(type_name);
			if (strcasecmp(type_name.c_ptr(), "LongBlob"))
				continue;
		}
			
		if( is_pbms_blob && !is_pbms_blob(field) )
			continue;
			
			
		// Get the blob record:
		field_offset = field->offset(field->table->record[0]);
		packlength = field->pack_length() - field->table->s->blob_ptr_size;

		if (new_null_blob) {
			new_blob_url = NULL;
		} else {
			new_blob_rec = new_row + field_offset;
			new_length = field->get_length(new_blob_rec);
			memcpy(&new_blob_url, new_blob_rec +packlength, sizeof(char*));
		}
		
		if (old_null_blob) {
			old_blob_url = NULL;
		} else {
			old_blob_rec = old_row + field_offset;
			old_length = field->get_length(old_blob_rec);
			memcpy(&old_blob_url, old_blob_rec +packlength, sizeof(char*));
		}
		
		// Check to see if the BLOBs are the same.
		// I am assuming that if the BLOB pointer is different then teh BLOB has changed.
		// Zero length BLOBs are a special case because they may have a NULL data pointer,
		// to catch this and distiguish it from a NULL BLOB I do a check to see if one field was NULL:
		// (old_null_blob != new_null_blob)
		if ((old_blob_url != new_blob_url) || (old_null_blob != new_null_blob)) {
			
			result->mr_had_blobs = true;

			// The BLOB was updated so delete the old one and insert the new one.
			if ((old_null_blob == false) && (err = pbms_api.releaseBlob(DB_NAME(field), TAB_NAME(field), old_blob_url, old_length, result)))
				return err;
				
			if ((new_null_blob == false) && (err = insertRecord(field, new_blob_url, new_length, new_blob_rec, packlength, result)))
				return err;
		} 
	}
	
	return 0;
}

//====================
int pbms_write_row_blobs(const TABLE *table, unsigned char *row_buffer, PBMSResultPtr result)
{

	Field_blob *field;
	unsigned char *blob_rec;
	char *blob_url;
	size_t packlength, i, length;
	int err;

	result->mr_had_blobs = false;

	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	if (table->s->blob_fields == 0)
		return 0;
		
	for (i= 0; i <  table->s->blob_fields; i++) {
		field =  GET_BLOB_FIELD(table, i);
		
		if (field->is_null_in_record(row_buffer))
			continue;
			
		{
			String type_name;
			// Note: field->type() always returns MYSQL_TYPE_BLOB regardless of the type of BLOB
			field->sql_type(type_name);
			if (strcasecmp(type_name.c_ptr(), "LongBlob"))
				continue;
		}
			
		if( is_pbms_blob && !is_pbms_blob(field) )
			continue;

		result->mr_had_blobs = true;

		// Get the blob record:
		packlength = field->pack_length() - field->table->s->blob_ptr_size;
		blob_rec = row_buffer + field->offset(field->table->record[0]);
		
		length = field->get_length(blob_rec);
		memcpy(&blob_url, blob_rec +packlength, sizeof(char*));

		if ((err = insertRecord(field, blob_url, length, blob_rec, packlength, result)))
			return err;
	}

	return 0;
}

//====================
int pbms_delete_row_blobs(const TABLE *table, const unsigned char *row_buffer, PBMSResultPtr result)
{
	Field_blob *field;
	const unsigned char *blob_rec;
	char *blob;
	size_t packlength, i, length;
	bool call_failed = false;
	int err;
	
	result->mr_had_blobs = false;

	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	if (table->s->blob_fields == 0)
		return 0;
		
	for (i= 0; i < table->s->blob_fields; i++) {
		field = GET_BLOB_FIELD(table, i);
		
		if (field->is_null_in_record(row_buffer))
			continue;
			
		{
			String type_name;
			// Note: field->type() always returns MYSQL_TYPE_BLOB regardless of the type of BLOB
			field->sql_type(type_name);
			if (strcasecmp(type_name.c_ptr(), "LongBlob"))
				continue;
		}
			
		if(is_pbms_blob && !is_pbms_blob(field) )
			continue;

		result->mr_had_blobs = true;	
		
		// Get the blob record:
		packlength = field->pack_length() - field->table->s->blob_ptr_size;

		blob_rec = row_buffer + field->offset(field->table->record[0]);
		length = field->get_length(blob_rec);
		memcpy(&blob, blob_rec +packlength, sizeof(char*));

		// Signal PBMS to delete the reference to the BLOB.
		err = pbms_api.releaseBlob(DB_NAME(field), TAB_NAME(field), blob, length, result);
		if (err)
			return err;
	}
	
	return 0;
}

#define MAX_NAME_SIZE 64
static void parse_table_path(const char *path, char *db_name, char *tab_name)
{
	const char *ptr = path + strlen(path) -1, *eptr;
	int len;
	
	*db_name = *tab_name = 0;
	
	while ((ptr > path) && (*ptr != '/'))ptr --;
	if (*ptr != '/') 
		return;
		
	strncpy(tab_name, ptr+1, MAX_NAME_SIZE);
	tab_name[MAX_NAME_SIZE-1] = 0;
	eptr = ptr;
	ptr--;
	
	while ((ptr > path) && (*ptr != '/'))ptr --;
	if (*ptr != '/') 
		return;
	ptr++;
	
	len = eptr - ptr;
	if (len >= MAX_NAME_SIZE)
		len = MAX_NAME_SIZE-1;
		
	memcpy(db_name, ptr, len);
	db_name[len] = 0;
	
}

//====================
int pbms_rename_table_with_blobs(const char *old_table_path, const char *new_table_path, PBMSResultPtr result)
{
	char o_db_name[MAX_NAME_SIZE], n_db_name[MAX_NAME_SIZE], o_tab_name[MAX_NAME_SIZE], n_tab_name[MAX_NAME_SIZE];

	result->mr_had_blobs = false; 
	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	result->mr_had_blobs = true; // Assume it has blobs.
	
	parse_table_path(old_table_path, o_db_name, o_tab_name);
	parse_table_path(new_table_path, n_db_name, n_tab_name);
	
	return pbms_api.renameTable(o_db_name, o_tab_name, n_db_name, n_tab_name, result);
}

//====================
int pbms_delete_table_with_blobs(const char *table_path, PBMSResultPtr result)
{
	char db_name[MAX_NAME_SIZE], tab_name[MAX_NAME_SIZE];
		
	result->mr_had_blobs = false; 
	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	result->mr_had_blobs = true; // Assume it has blobs.
	parse_table_path(table_path, db_name, tab_name);

	return pbms_api.dropTable(db_name, tab_name, result);
}

//====================
void pbms_completed(const TABLE *table, bool ok)
{
	if (!pbms_api.isPBMSLoaded())
		return;
		
	if ((!table) || (table->s->blob_fields != 0))
		pbms_api.completed(ok) ;
		
	 return ;
}
#endif
#endif // DRIZZLED
