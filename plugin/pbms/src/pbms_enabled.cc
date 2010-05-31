/* Copyright (c) 2009 PrimeBase Technologies GmbH, Germany
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
int pbms_write_row_blobs(TABLE *table __attribute__((unused)), 
						unsigned char *buf __attribute__((unused)), 
						PBMSResultPtr result __attribute__((unused))
						){ return 0;}
int pbms_delete_row_blobs(TABLE *table __attribute__((unused)), 
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
#ifdef DRIZZLED
#include <sys/stat.h>
#include <drizzled/common.h>
#include <drizzled/field/blob.h>
#include <drizzled/session.h>
#include <drizzled/plugin.h>
#else
#include "mysql_priv.h"
#include <mysql/plugin.h>
#define session_alloc(sess, size) thd_alloc(sess, size);
#define current_session current_thd
#endif 

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

//====================
int pbms_write_row_blobs(TABLE *table, unsigned char *row_buffer, PBMSResultPtr result)
{
	Field_blob *field;
	char *blob_rec, *blob;
	size_t packlength, i, org_length, length;
	char blob_url_buffer[PBMS_BLOB_URL_SIZE];
	int err;

	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	if (table->s->blob_fields == 0)
		return 0;
		
	for (i= 0; i < table->s->blob_fields; i++) {
		field = GET_BLOB_FIELD(table, i);

#ifndef DRIZZLED
		{
			String type_name;
			// Note: field->type() always returns MYSQL_TYPE_BLOB regardless of the type of BLOB
			field->sql_type(type_name);
			if (strcasecmp(type_name.c_ptr(), "LongBlob"))
				continue;
		}
#endif
			
		if( is_pbms_blob && !is_pbms_blob(field) )
			continue;
			
		// Get the blob record:
		blob_rec = (char *)row_buffer + field->offset(field->table->record[0]);
		packlength = field->pack_length() - field->table->s->blob_ptr_size;

		memcpy(&blob, blob_rec +packlength, sizeof(char*));
		org_length = field->get_length((unsigned char *)blob_rec);

		
		// Signal PBMS to record a new reference to the BLOB.
		// If 'blob' is not a BLOB URL then it will be stored in the repositor as a new BLOB
		// and a reference to it will be created.
		err = pbms_api.retainBlob(DB_NAME(field), TAB_NAME(field), blob_url_buffer, blob, org_length, field->field_index, result);
		if (err)
			return err;
			
		// If the BLOB length changed reset it. 
		// This will happen if the BLOB data was replaced with a BLOB reference. 
		length = strlen(blob_url_buffer)  +1;
		if ((length != org_length) || memcmp(blob_url_buffer, blob, length)) {
			if (length != org_length) {
				field->store_length((unsigned char *)blob_rec, packlength, length);
			}
			
			if (length > org_length) {
				// This can only happen if the BLOB URL is actually larger than the BLOB itself.
				blob = (char *) session_alloc(current_session, length);
				memcpy(blob_rec+packlength, &blob, sizeof(char*));
			}			
			memcpy(blob, blob_url_buffer, length);
		} 
	}
	
	return 0;
}

//====================
int pbms_delete_row_blobs(TABLE *table, const unsigned char *row_buffer, PBMSResultPtr result)
{
	Field_blob *field;
	const char *blob_rec;
	char *blob;
	size_t packlength, i, length;
	int err;

	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	if (table->s->blob_fields == 0)
		return 0;
		
	for (i= 0; i < table->s->blob_fields; i++) {
		field = GET_BLOB_FIELD(table, i);

#ifndef DRIZZLED
		{
			String type_name;
			// Note: field->type() always returns MYSQL_TYPE_BLOB regardless of the type of BLOB
			field->sql_type(type_name);
			if (strcasecmp(type_name.c_ptr(), "LongBlob"))
				continue;
		}
#endif
			
		if(is_pbms_blob && !is_pbms_blob(field) )
			continue;
			
		// Get the blob record:
		blob_rec = (char *)row_buffer + field->offset(field->table->record[0]);
		packlength = field->pack_length() - field->table->s->blob_ptr_size;

		length = field->get_length((unsigned char *)blob_rec);
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

	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	parse_table_path(old_table_path, o_db_name, o_tab_name);
	parse_table_path(new_table_path, n_db_name, n_tab_name);
	
	return pbms_api.renameTable(o_db_name, o_tab_name, n_tab_name, result);
}

//====================
int pbms_delete_table_with_blobs(const char *table_path, PBMSResultPtr result)
{
	char db_name[MAX_NAME_SIZE], tab_name[MAX_NAME_SIZE];
		
	if (!pbms_api.isPBMSLoaded())
		return 0;
		
	parse_table_path(table_path, db_name, tab_name);

	return pbms_api.dropTable(db_name, tab_name, result);
}

//====================
void pbms_completed(TABLE *table, bool ok)
{
	if (!pbms_api.isPBMSLoaded())
		return;
		
	if ((!table) || (table->s->blob_fields != 0))
		pbms_api.completed(ok) ;
		
	 return ;
}
#endif
