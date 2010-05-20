#ifdef xxxDRIZZLED
/* 
   -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
   *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

   Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* need to define DRIZZLE_SERVER to get inside the Session */
#define DRIZZLE_SERVER 1

#include <drizzled/server_includes.h>
#include <drizzled/plugin_blobcontainer.h>
#include <drizzled/table.h>
#include <drizzled/gettext.h>
#include <drizzled/errmsg.h>
#include <drizzled/error.h>

#include "CSConfig.h"
#include "CSGlobal.h"
#include "CSStrUtil.h"
#include "Engine_ms.h"

/*
	select * from information_schema.plugins;
 insert into bt values (13, "PBMS_BLOB xxxxxxxx", "zzzzzz", "Another pbms blob");
 select * from bt;

*/
#define NO_ERROR	false
#define ERROR	true

static ulong plugin_min_blob_size= 0;

static bool debug_trace= true;

void * (*blobcontainer_session_alloc)(Session *session, unsigned int size);

#define DB_NAME(f) (f->table->s->db.str)
#define TAB_NAME(f) (*(f->table_name))
#define ENG_NAME(f) (f->table->s->db_plugin->name.str)
#define DUMMY_URL "PBMS_BLOB_URL"
#define NORMAL_PRIORITY 1

//-------------------
static int report_error(Session *session , PBMSResultRec *rec, const char *msg)
{
	if (rec) {
		if (msg)
			errmsg_printf(session, NORMAL_PRIORITY, "PBMS blobcontainer: (%"PRId32") %s (%s)", rec->mr_code, rec->mr_message, msg);
		else
			errmsg_printf(session, NORMAL_PRIORITY, "PBMS blobcontainer: (%"PRId32") %s", rec->mr_code, rec->mr_message);
	} else if (msg)
		errmsg_printf(session, NORMAL_PRIORITY, "PBMS blobcontainer: %s", msg);
		
	return ER_UNKNOWN_ERROR;
}

//-------------------
static bool local_insert_blob(Session *session, Field_blob *field, unsigned char **blob, uint32_t *len, int *error, bool can_create)
{
	char *out_url = NULL, blob_url_buffer[PBMS_BLOB_URL_SIZE], *in_url = (char *)*blob;
	size_t url_len = *len;
	bool retry = true;
	PBMSResultRec result = {0};
	
	*error= 0;

	if ((*len < plugin_min_blob_size) || !*blob)
		return NO_ERROR;

try_again:   

	out_url = pbms_use_blob(DB_NAME(field), TAB_NAME(field), field->field_index, in_url, url_len,  blob_url_buffer, &result);
	if (out_url) {
		if (*len < strlen(out_url)) {
			*blob= (unsigned char *) blobcontainer_session_alloc(session, strlen(out_url));
			if (!*blob) {
				(void) pbms_release_blob(DB_NAME(field), TAB_NAME(field), field->field_index, out_url, strlen(out_url), &result);
				*error= HA_ERR_OUT_OF_MEM;
				goto done;
			}    
		}
		*len= strlen(out_url);
		memcpy((char*)(*blob), out_url, *len);

	} else  if ((result.mr_code == MS_ERR_INCORRECT_URL) &&  can_create) { // The data is not a url.
		if (!retry) { // This should never happen.
			*error = report_error(session, NULL, "Bad PBMS BLOB URL generated.");
			goto done;
		}
	
		result.mr_code = 0;
		if ( pbms_new_blob(DB_NAME(field), TAB_NAME(field), field->field_index, *blob, *len,  blob_url_buffer, &result)) {
			*error = report_error(session, &result, "pbms_new_blob() failed.");
			goto done;
		}		
		in_url = blob_url_buffer;
		url_len = strlen(in_url);
		retry = false;
		goto try_again;
	} else
		*error = report_error(session, &result, "pbms_use_blob() failed.");
		
	
 
 done:
 
	return (*error != 0);
}

//-------------------
static bool local_delete_blob(Session *session, Field_blob *field, const unsigned char *blob, uint32_t len, int *error)
{
	PBMSResultRec result = {0};
	
	*error= 0;

	if (!blob)
		return NO_ERROR;
		
	if (pbms_release_blob(DB_NAME(field), TAB_NAME(field), field->field_index, (char*)blob, len,  &result)) {
		*error = report_error(session, &result, "pbms_new_blob() failed.");
	}
	
	return (*error != 0);
}

//-------------------
static bool blobcontainer_insert_blob(Session *session, Field_blob *field, unsigned char **blob, uint32_t *len, int *error)
{
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) {
		char str[21];
		memcpy(str, *blob, (*len < 21)?*len:20);
		str[(*len < 21)?*len:20]= 0;
		printf("blobcontainer_insert_blob(%s.%s, \"%s\", %"PRId32") called.\n", *(field->table_name), field->field_name, str, *len);
	}

	return local_insert_blob(session, field, blob, len, error, true);
}

//-------------------
static bool blobcontainer_undo_insert_blob(Session *session, Field_blob *field, unsigned char *blob, uint32_t len)
{
	int err;
	
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) 
		printf("blobcontainer_undo_insert_blob(%s.%s) called.\n", *(field->table_name), field->field_name);

	return local_delete_blob( session, field, blob, len, &err);
}

static bool blobcontainer_update_blob(Session *session, Field_blob *field, const unsigned char *old_blob, uint32_t old_len, unsigned char **blob, uint32_t *len, int *error)
{
	*error= 0;
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) {
		char new_str[21], old_str[21];
		memcpy(new_str, *blob, (*len < 21)?*len:20);
		new_str[(*len < 21)?*len:20]= 0;

		memcpy(old_str, old_blob, (old_len < 21)?old_len:20);
		old_str[(old_len < 21)?old_len:20]= 0;

		printf("blobcontainer_update_blob(%s.%s, \"%s\", %"PRId32", \"%s\", %"PRId32") called.\n", *(field->table_name), field->field_name, old_str, old_len, new_str, *len);
	}

	if (local_insert_blob(session, field, blob, len, error, true) == ERROR)
		return ERROR;

	if (local_delete_blob(session, field, old_blob, old_len, error) == ERROR) {
		int err = 0;
		if (local_delete_blob(session, field, *blob, *len, &err) == ERROR) { // Should never happen.
			report_error(session, NULL, "Cleanup after failed update failed. Possible non referenced BLOB.");
		}			
		return ERROR;
	}

	return NO_ERROR ;
}

static bool blobcontainer_undo_update_blob(Session *session, Field_blob *field, const unsigned char *old_blob , uint32_t old_len , unsigned char *blob , uint32_t len )
{
	bool rtc1, rtc2;
	int err;
	unsigned char *temp_blob = (unsigned char *) old_blob;
	
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) 
		printf("blobcontainer_undo_update_blob(%s.%s) called.\n", *(field->table_name), field->field_name);

	
	rtc1 = local_insert_blob( session, field, &temp_blob, &old_len, &err, false);	
	rtc2 = local_delete_blob( session, field, blob, len, &err);

	if (rtc1 == ERROR || rtc1 == ERROR)
		return ERROR;
		
	return NO_ERROR ;
}

static bool blobcontainer_delete_blob(Session *session , Field_blob *field, const unsigned char *blob, uint32_t len, int *error)
{
	*error= 0;
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) {
		char str[21];
		memcpy(str, blob, (len < 21)?len:20);
		str[(len < 21)?len:20]= 0;
		printf("blobcontainer_delete_blob(%s.%s, \"%s\", %"PRId32") called.\n", *(field->table_name), field->field_name, str, len);
	}

	return local_delete_blob(session, field, blob, len, error) ;
}

static bool blobcontainer_undo_delete_blob(Session *session, Field_blob *field, const unsigned char *blob, uint32_t len)
{
	unsigned char *temp_blob = (unsigned char *) blob;
	int err;
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) {
		char str[21];
		memcpy(str, blob, (len < 21)?len:20);
		str[(len < 21)?len:20]= 0;
		printf("blobcontainer_undo_delete_blob(%s.%s, \"%s\", %"PRId32") called.\n", *(field->table_name), field->field_name, str, len);
	}

	return local_insert_blob( session, field, &temp_blob, &len, &err, false);
}

static bool blobcontainer_rename_table(Session *session, const char *old_name, const  char *new_name, int *error)
{
	PBMSResultRec result = {0};
	char old_db_name[PATH_MAX], new_db_name[PATH_MAX], *old_table, *new_table;
	*error= 0;
	
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) 
		printf("blobcontainer_rename_table(%s, %s) called.\n", old_name, new_name);

	cs_strcpy(PATH_MAX, old_db_name, old_name);
	old_table = cs_last_name_of_path(old_db_name);
	cs_remove_last_name_of_path(old_db_name);
	
	cs_strcpy(PATH_MAX, new_db_name, new_name);
	new_table = cs_last_name_of_path(new_db_name);
	cs_remove_last_name_of_path(new_db_name);
	
	
	if (strcmp(old_db_name, new_db_name)) {
		*error = report_error(session, NULL, "Cannot rename tables across databases.");
		return ERROR;
	}
	
	
	
	if (pbms_rename_table(cs_last_name_of_path(new_db_name), old_table, new_table,  &result)) {
		*error = report_error(session, &result, "pbms_rename_table() failed.");
	}

	return (*error != 0);
}

static bool blobcontainer_undo_rename_table(Session *session, const char *old_name, const  char *new_name)
{
	int err;
	
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) 
		printf("blobcontainer_undo_rename_table(%s, %s) called.\n", old_name, new_name);
		
	return (blobcontainer_rename_table(session, new_name, old_name, &err));
}

static bool blobcontainer_drop_table(Session *session, const char *name, int *error)
{
	PBMSResultRec result = {0};
	char db_name[PATH_MAX], *table;

	*error= 0;
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) 
		printf("blobcontainer_drop_table(%s) called.\n", name);

	cs_strcpy(PATH_MAX, db_name, name);
	table = cs_last_name_of_path(db_name);
	if (table == db_name)
		return NO_ERROR;
		
	*(table -1) = 0;
	
	if (pbms_drop_table(cs_last_name_of_path(db_name), table,  &result)) {
		*error = report_error(session, &result, "pbms_drop_table() failed.");
	}

	return (*error != 0);
}

static bool blobcontainer_undo_drop_table(Session *session __attribute__((unused)), const char *name)
{
	if (pbms_enabled(ENG_NAME(field)) 
		return NO_ERROR;
		
	if (debug_trace) 
		printf("blobcontainer_undo_drop_table(%s) called.\n", name);

	return report_error(session,NULL, "Cannot undo DROP TABLE."); ;
}


static int pbms_blobfilter_plugin_init(void *p)
{
  blobcontainer_t *bc= (blobcontainer_t*) p;
  
  blobcontainer_session_alloc = bc->blobcontainer_session_alloc;
  
  bc->blobcontainer_insert_blob= blobcontainer_insert_blob;
  bc->blobcontainer_undo_insert_blob= blobcontainer_undo_insert_blob;

  bc->blobcontainer_update_blob= blobcontainer_update_blob;
  bc->blobcontainer_undo_update_blob= blobcontainer_undo_update_blob;
  
  bc->blobcontainer_delete_blob= blobcontainer_delete_blob;
  bc->blobcontainer_undo_delete_blob= blobcontainer_undo_delete_blob;
  
  bc->blobcontainer_rename_table= blobcontainer_rename_table;
  bc->blobcontainer_undo_rename_table= blobcontainer_undo_rename_table;

  bc->blobcontainer_drop_table= blobcontainer_drop_table;
  bc->blobcontainer_undo_drop_table= blobcontainer_undo_drop_table;
  
  if (debug_trace) 
    printf("pbms_blobfilter_plugin_init() called.\n");

  return 0;
}

static int pbms_blobfilter_plugin_deinit(void *p)
{
  blobcontainer_t *bc= (blobcontainer_t*) p;
  bc->blobcontainer_insert_blob= NULL;
  bc->blobcontainer_undo_insert_blob= NULL;
  
  bc->blobcontainer_update_blob= NULL;
  bc->blobcontainer_undo_update_blob= NULL;
  
  bc->blobcontainer_delete_blob= NULL;
  bc->blobcontainer_undo_delete_blob= NULL;
  
  bc->blobcontainer_rename_table= NULL;
  bc->blobcontainer_undo_rename_table= NULL;

  bc->blobcontainer_drop_table= NULL;
  bc->blobcontainer_undo_drop_table= NULL;
    
  return 0;
}
////////////////////////////////////////

class PBMS_Filter: public Filter
{

	PBMS_Filter(){}
	~PBMS_Filter(){}
	
	virtual bool insert_row(Table *table, void *buf, int *error){return false;}
	virtual bool complete_insert_row(Table *table, void *buf, bool undo){return false;}

	virtual bool update_row(Table *table, const void *old_data, void *new_data, int *error){return false;}
	virtual bool complete_update_row(Table *table, const void *old_data, void *new_data, bool undo){return false;}

	virtual bool delete_row(Table *table, const void *buf, int *error){return false;}
	virtual bool complete_delete_row(Table *table, const void *buf, int *error){return false;}

	virtual bool delete_row(Table *table, const void *buf, int *error){return false;}
	virtual bool complete_delete_row(Table *table, const void *buf, bool undo){return false;}

	virtual bool rename_table(const char *old_table_path, const char *new_table_path, int *error){return false;}
	virtual bool complete_rename_table(const char *old_table_path, const char *new_table_path, bool undo){return false;}

	virtual bool drop_table(const char *table_path, int *error){return false;}
	virtual bool complete_drop_table(const char *table_path, bool undo){return false;}

}

static PBMS_Filter *pbms_Filter= NULL;
static int pbms_blobfilter_plugin_init(drizzled::plugin::Registry &registry)
{
   PBMSResultRec result;
   if (!pbms_initialize("Drizzle", true, &result)) {
       sql_print_error("pbms_initialize() Error: %s", result.mr_message);
       return 1;
   }

  pbms_Filter = new PBMS_Filter();
  registry.add(pbms_Filter);

  return 0;
}

static int pbms_blobfilter_plugin_deinit(drizzled::plugin::Registry &registry)
{
	registry.remove(pbms_Filter);
	delete pbms_Filter;
	pbms_Filter= NULL;

	pbms_finalize();
	return 0;
}



#ifdef BLOBFILTER_SYSTEM_VARS
static DRIZZLE_SYSVAR_BOOL(
  tracing_enable,
  debug_trace,
  PLUGIN_VAR_NOCMDARG,
  "Enable plugin tracing",
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static DRIZZLE_SYSVAR_ULONG(
  min_blob_size,
  plugin_min_blob_size,
  PLUGIN_VAR_OPCMDARG,
  "Minimum BLOB size to be stored.",
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

static struct st_mysql_sys_var* pbms_blobfilter_system_variables[]= {
  DRIZZLE_SYSVAR(tracing_enable),
  DRIZZLE_SYSVAR(min_blob_size),
  NULL
};
#else
#define pbms_blobfilter_system_variables NULL
#endif

//struct st_mysql_plugin pbms_blobcontainer_plugin = {
//  DRIZZLE_FILTER_PLUGIN,
drizzle_declare_plugin(blobfilter)
{
  "PBMS BLOB Filter",
  "1.0",
  "Barry Leslie  PrimeBase Technologies GmbH",
  "BLOB filter plugin for the PBMS storage engine.",
  PLUGIN_LICENSE_GPL,
  pbms_blobfilter_plugin_init, /* Plugin Init */
  pbms_blobfilter_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  pbms_blobfilter_system_variables,   
  NULL    /* config options */
};

#endif // DRIZZLED
