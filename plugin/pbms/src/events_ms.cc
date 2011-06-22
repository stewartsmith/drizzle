/*
 *  Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Barry Leslie
 *
 * 2010-06-01
 */

#include <config.h>
#include <string>
#include <inttypes.h>

#include <drizzled/session.h>
#include <drizzled/field/blob.h>
#include <drizzled/sql_lex.h>

#include "cslib/CSConfig.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSThread.h"

#include "events_ms.h"
#include "parameters_ms.h"
#include "engine_ms.h"

using namespace drizzled;
using namespace plugin;
using namespace std;


//==================================
// My table event observers: 
static bool insertRecord(const char *db, const char *table_name, char *possible_blob_url,  size_t length, 
	Session &session, Field_blob *field, unsigned char *blob_rec, size_t packlength)
{
	char *blob_url;
	char safe_url[PBMS_BLOB_URL_SIZE+1];
	PBMSBlobURLRec blob_url_buffer;
	size_t org_length = length;
	int32_t err;
	PBMSResultRec result;
	
	// Tell PBMS to record a new reference to the BLOB.
	// If 'blob' is not a BLOB URL then it will be stored in the repositor as a new BLOB
	// and a reference to it will be created.
	
	if (MSEngine::couldBeURL(possible_blob_url, length) == false) {
		err = MSEngine::createBlob(db, table_name, possible_blob_url, length, &blob_url_buffer, &result);
		if (err) {
			// If it fails log the error and continue to try and release any other BLOBs in the row.
			fprintf(stderr, "PBMSEvents: createBlob(\"%s.%s\") error (%d):'%s'\n", 
				db, table_name, result.mr_code,  result.mr_message);
				
			return true;
		}				
		blob_url = blob_url_buffer.bu_data;
	} else {
		// The BLOB URL may not be null terminate, if so
		// then copy it to a safe buffer and terminate it.
		if (possible_blob_url[length]) {
			memcpy(safe_url, possible_blob_url, length);
			safe_url[length] = 0;
			blob_url = safe_url;
		} else
			blob_url = possible_blob_url;
	}
	
	// Tell PBMS to add a reference to the BLOB.
	err = MSEngine::referenceBlob(db, table_name, &blob_url_buffer, blob_url, field->position(), &result);
	if (err) {
		// If it fails log the error and continue to try and release any other BLOBs in the row.
		fprintf(stderr, "PBMSEvents: referenceBlob(\"%s.%s\", \"%s\" ) error (%d):'%s'\n", 
			db, table_name, blob_url, result.mr_code,  result.mr_message);
			
		return true;
	}
	
	// The URL is modified on insert so if the BLOB length changed reset it. 
	// This will happen if the BLOB data was replaced with a BLOB reference. 
	length = strlen(blob_url_buffer.bu_data)  +1;
	if ((length != org_length) || memcmp(blob_url_buffer.bu_data, possible_blob_url, length)) {
		char *blob = possible_blob_url; // This is the BLOB as the server currently sees it.
		
		if (length != org_length) {
			field->store_length(blob_rec, length);
		}
		
		if (length > org_length) {
			// This can only happen if the BLOB URL is actually larger than the BLOB itself.
			blob = (char *) session.mem.alloc(length);
			memcpy(blob_rec+packlength, &blob, sizeof(char*));
		}			
		memcpy(blob, blob_url_buffer.bu_data, length);
	} 

	return false;
}

//---
static bool deleteRecord(const char *db, const char *table_name, char *blob_url,  size_t length)
{
	int32_t err;
	char safe_url[PBMS_BLOB_URL_SIZE+1];
	PBMSResultRec result;
	bool call_failed = false;
	
	// Check to see if this is a valid URL.
	if (MSEngine::couldBeURL(blob_url, length)) {
	
		// The BLOB URL may not be null terminate, if so
		// then copy it to a safe buffer and terminate it.
		if (blob_url[length]) {
			memcpy(safe_url, blob_url, length);
			safe_url[length] = 0;
			blob_url = safe_url;
		}
		
		// Signal PBMS to delete the reference to the BLOB.
		err = MSEngine::dereferenceBlob(db, table_name, blob_url, &result);
		if (err) {
			// If it fails log the error and continue to try and release any other BLOBs in the row.
			fprintf(stderr, "PBMSEvents: dereferenceBlob(\"%s.%s\") error (%d):'%s'\n", 
				db, table_name, result.mr_code,  result.mr_message);
				
			call_failed = true;
		}
	}

	return call_failed;
}

//---
static bool observeBeforeInsertRecord(BeforeInsertRecordEventData &data)
{
	Field_blob *field;
	unsigned char *blob_rec;
	char *blob_url;
	size_t packlength, i, length;

	for (i= 0; i < data.table.sizeBlobFields(); i++) {
		field = data.table.getBlobFieldAt(i);
		
		if (field->is_null_in_record(data.row))
			continue;
			
		// Get the blob record:
		packlength = field->pack_length() - data.table.getBlobPtrSize();

		blob_rec = (unsigned char *)data.row + field->offset(data.table.getInsertRecord());
		length = field->get_length(blob_rec);
		memcpy(&blob_url, blob_rec +packlength, sizeof(char*));

		if (insertRecord(data.table.getSchemaName(), data.table.getTableName(), 
			blob_url, length, data.session, field, blob_rec, packlength))
			return true;
	}

	return false;
}

//---
static bool observeAfterInsertRecord(AfterInsertRecordEventData &data)
{
	bool has_blob = false;
	
	for (uint32_t i= 0; (i < data.table.sizeBlobFields()) && (has_blob == false); i++) {
		Field_blob *field = data.table.getBlobFieldAt(i);
		
		if ( field->is_null_in_record(data.row) == false)
			has_blob = true;
	}
	
	if  (has_blob)
		MSEngine::callCompleted(data.err == 0);
	
	return false;
}

//---
static bool observeBeforeUpdateRecord(BeforeUpdateRecordEventData &data)
{
	Field_blob *field;
	uint32_t field_offset;
	const unsigned char *old_blob_rec;
	unsigned char *new_blob_rec= NULL;
	char *old_blob_url, *new_blob_url;
	size_t packlength, i, old_length= 0, new_length= 0;
	const unsigned char *old_row = data.old_row;
	unsigned char *new_row = data.new_row;
	const char *db = data.table.getSchemaName();
	const char *table_name = data.table.getTableName();
	bool old_null, new_null;

	for (i= 0; i < data.table.sizeBlobFields(); i++) {
		field = data.table.getBlobFieldAt(i);
		
		new_null = field->is_null_in_record(new_row);		
		old_null = field->is_null_in_record(old_row);
		
		if (new_null && old_null)
			continue;
		
		// Check to see if the BLOB data was updated.

		// Get the blob records:
		field_offset = field->offset(data.table.getInsertRecord());
		packlength = field->pack_length() - data.table.getBlobPtrSize();

		if (new_null) {
			new_blob_url = NULL;
		} else {
			new_blob_rec = new_row + field_offset;
			new_length = field->get_length(new_blob_rec);
			memcpy(&new_blob_url, new_blob_rec +packlength, sizeof(char*));
		}
		
		if (old_null) {
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
		// (old_null != new_null)
		if ((old_blob_url != new_blob_url) || (old_null != new_null)) {
			
			// The BLOB was updated so delete the old one and insert the new one.
			if ((old_null == false) && deleteRecord(db, table_name, old_blob_url, old_length))
				return true;
				
			if ((new_null == false) && insertRecord(db, table_name, new_blob_url, new_length, data.session, field, new_blob_rec, packlength))
				return true;

		}
		
	}

	return false;
}

//---
static bool observeAfterUpdateRecord(AfterUpdateRecordEventData &data)
{
	bool has_blob = false;
	const unsigned char *old_row = data.old_row;
	const unsigned char *new_row = data.new_row;
	
	for (uint32_t i= 0; (i < data.table.sizeBlobFields()) && (has_blob == false); i++) {
		Field_blob *field = data.table.getBlobFieldAt(i);		
		bool new_null = field->is_null_in_record(new_row);		
		bool old_null = field->is_null_in_record(old_row);
		
		if ( (new_null == false) || (old_null == false)) {
			const unsigned char *blob_rec;			
			size_t field_offset = field->offset(data.table.getInsertRecord());
			size_t packlength = field->pack_length() - data.table.getBlobPtrSize();
			char *old_blob_url, *new_blob_url;
			
			blob_rec = new_row + field_offset;
			memcpy(&new_blob_url, blob_rec +packlength, sizeof(char*));

			blob_rec = old_row + field_offset;
			memcpy(&old_blob_url, blob_rec +packlength, sizeof(char*));

			has_blob = ((old_blob_url != new_blob_url) || (old_null != new_null));
		}
	}
	
	if  (has_blob)
		MSEngine::callCompleted(data.err == 0);

  return false;
}

//---
static bool observeAfterDeleteRecord(AfterDeleteRecordEventData &data)
{
	Field_blob *field;
	const unsigned char *blob_rec;
	char *blob_url;
	size_t packlength, i, length;
	bool call_failed = false;
	bool has_blob = false;
	
	if (data.err != 0)
		return false;

	for (i= 0; (i < data.table.sizeBlobFields()) && (call_failed == false); i++) {
		field = data.table.getBlobFieldAt(i);
		
		if (field->is_null_in_record(data.row))
			continue;
			
		has_blob = true;	
		// Get the blob record:
		packlength = field->pack_length() - data.table.getBlobPtrSize();

		blob_rec = data.row + field->offset(data.table.getInsertRecord());
		length = field->get_length(blob_rec);
		memcpy(&blob_url, blob_rec +packlength, sizeof(char*));

		if (deleteRecord(data.table.getSchemaName(), data.table.getTableName(), blob_url, length))
			call_failed = true;
	}
	
	if (has_blob)
		MSEngine::callCompleted(call_failed == false);
		
	return call_failed;
}

//==================================
// My session event observers: 
static bool observeAfterDropDatabase(AfterDropDatabaseEventData &data)
{
	PBMSResultRec result;
	if (data.err != 0)
		return false;

	if (MSEngine::dropDatabase(data.db.c_str(), &result) != 0) {
		fprintf(stderr, "PBMSEvents: dropDatabase(\"%s\") error (%d):'%s'\n", 
			data.db.c_str(), result.mr_code,  result.mr_message);
	}
	
	// Always return no error for after drop database. What could the server do about it?
	return false;
}

//==================================
// My schema event observers: 
static bool observeAfterDropTable(AfterDropTableEventData &data)
{
	PBMSResultRec result;
	if (data.err != 0)
		return false;

	if (MSEngine::dropTable(data.table.getSchemaName().c_str(), data.table.getTableName().c_str(), &result) != 0) {
		fprintf(stderr, "PBMSEvents: dropTable(\"%s.%s\") error (%d):'%s'\n", 
			data.table.getSchemaName().c_str(), data.table.getTableName().c_str(), result.mr_code,  result.mr_message);
		return true;
	}
	MSEngine::callCompleted(true);
	
	return false;
}

//---
static bool observeAfterRenameTable(AfterRenameTableEventData &data)
{
	PBMSResultRec result;
	if (data.err != 0)
		return false;

	const char *from_db = data.from.getSchemaName().c_str();
	const char *from_table = data.from.getTableName().c_str();
	const char *to_db = data.to.getSchemaName().c_str();
	const char *to_table = data.to.getTableName().c_str();
	
	if (MSEngine::renameTable(from_db, from_table, to_db, to_table, &result) != 0) {
		fprintf(stderr, "PBMSEvents: renameTable(\"%s.%s\" To \"%s.%s\") error (%d):'%s'\n", 
			from_db, from_table, to_db, to_table, result.mr_code,  result.mr_message);
		return true;
	}
	MSEngine::callCompleted(true);
	
	return false;
}

//==================================
/* This is where I register which table events my pluggin is interested in.*/
void PBMSEvents::registerTableEventsDo(TableShare &table_share, EventObserverList &observers)
{
  if ((PBMSParameters::isPBMSEventsEnabled() == false) 
    || (PBMSParameters::isBLOBTable(table_share.getSchemaName(), table_share.getTableName()) == false))
    return;
    
  if (table_share.blob_fields > 0) {
	  registerEvent(observers, BEFORE_INSERT_RECORD, PBMSParameters::getBeforeInsertEventPosition()); // I want to be called first if passible
	  registerEvent(observers, AFTER_INSERT_RECORD); 
	  registerEvent(observers, BEFORE_UPDATE_RECORD, PBMSParameters::getBeforeUptateEventPosition());
	  registerEvent(observers, AFTER_UPDATE_RECORD); 
	  registerEvent(observers, AFTER_DELETE_RECORD);
 }
}

//==================================
/* This is where I register which schema events my pluggin is interested in.*/
void PBMSEvents::registerSchemaEventsDo(const std::string &db, EventObserverList &observers)
{
  if ((PBMSParameters::isPBMSEventsEnabled() == false) 
    || (PBMSParameters::isBLOBDatabase(db.c_str()) == false))
    return;
    
  registerEvent(observers, AFTER_DROP_TABLE);
  registerEvent(observers, AFTER_RENAME_TABLE);
}

//==================================
/* This is where I register which schema events my pluggin is interested in.*/
void PBMSEvents::registerSessionEventsDo(Session &, EventObserverList &observers)
{
  if (PBMSParameters::isPBMSEventsEnabled() == false) 
    return;
    
  registerEvent(observers, AFTER_DROP_DATABASE);
}

//==================================
/* The event observer.*/
bool PBMSEvents::observeEventDo(EventData &data)
{
  bool result= false;
  
  switch (data.event) {
  case AFTER_DROP_DATABASE:
    result = observeAfterDropDatabase((AfterDropDatabaseEventData &)data);
    break;
    
  case AFTER_DROP_TABLE:
    result = observeAfterDropTable((AfterDropTableEventData &)data);
    break;
    
  case AFTER_RENAME_TABLE:
    result = observeAfterRenameTable((AfterRenameTableEventData &)data);
    break;
    
  case BEFORE_INSERT_RECORD:
     result = observeBeforeInsertRecord((BeforeInsertRecordEventData &)data);
    break;
    
  case AFTER_INSERT_RECORD:
    result = observeAfterInsertRecord((AfterInsertRecordEventData &)data);
    break;
    
 case BEFORE_UPDATE_RECORD:
    result = observeBeforeUpdateRecord((BeforeUpdateRecordEventData &)data);
    break;
             
  case AFTER_UPDATE_RECORD:
    result = observeAfterUpdateRecord((AfterUpdateRecordEventData &)data);
    break;
    
  case AFTER_DELETE_RECORD:
    result = observeAfterDeleteRecord((AfterDeleteRecordEventData &)data);
    break;

  default:
    fprintf(stderr, "PBMSEvents: Unexpected event '%s'\n", EventObserver::eventName(data.event));
 
  }
  
  return result;
}

