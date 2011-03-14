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
 * Engine interface.
 *
 */
#ifdef DRIZZLED
#include <config.h>

#include <drizzled/common.h>
#include <drizzled/current_session.h>
#include <drizzled/session.h>
#endif


#include "cslib/CSConfig.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSThread.h"

#ifndef DRIZZLED
#define PBMS_API pbms_internal  
#include "pbms.h"
#endif

#include "engine_ms.h"
#include "connection_handler_ms.h"
#include "open_table_ms.h"
#include "network_ms.h"
#include "transaction_ms.h"
#include "mysql_ms.h"


#ifdef new
#undef new
#endif

// From ha-pbms.cc:
extern CSThread *pbms_getMySelf(THD *thd);
extern void pbms_setMySelf(THD *thd, CSThread *self);

#ifndef DRIZZLED

/*
 * ---------------------------------------------------------------
 * ENGINE CALL-IN INTERFACE
 */

static PBMS_API *StreamingEngines;
// If PBMS support is built directly into the mysql/drizzle handler code 
// then calls from all other handlers are ignored.
static bool have_handler_support = false; 

/*
 * ---------------------------------------------------------------
 * ENGINE CALLBACK INTERFACE
 */

static void ms_register_engine(PBMSEnginePtr engine)
{
	if (engine->ms_internal)
		have_handler_support = true;
}

static void ms_deregister_engine(PBMSEnginePtr engine)
{
	UNUSED(engine);
}

static int ms_create_blob(bool internal, const char *db_name, const char *tab_name, char *blob, size_t blob_len, PBMSBlobURLPtr blob_url, PBMSResultPtr result)
{
	if (have_handler_support && !internal) {
		MSEngine::errorResult(CS_CONTEXT, MS_ERR_INVALID_OPERATION, "Invalid ms_create_blob() call", result);
		return MS_ERR_ENGINE;
	}

	return MSEngine::createBlob(db_name, tab_name, blob, blob_len, blob_url, result);
}

/*
 * ms_use_blob() may or may not alter the blob url depending on the type of URL and if the BLOB is in a
 * different database or not. It may also add a BLOB reference to the BLOB table log if the BLOB was from
 * a different table or no table was specified when the BLOB was uploaded.
 *
 * There is no need to undo this function because it will be undone automaticly if the BLOB is not retained.
 */
static int ms_retain_blob(bool internal, const char *db_name, const char *tab_name, PBMSBlobURLPtr ret_blob_url, char *blob_url, unsigned short col_index, PBMSResultPtr result)
{
	if (have_handler_support && !internal) {
		cs_strcpy(PBMS_BLOB_URL_SIZE, ret_blob_url->bu_data, blob_url); // This should have already been converted.
		return MS_OK;
	}
	
	return MSEngine::referenceBlob(db_name, tab_name, ret_blob_url, blob_url, col_index, result);	
}

static int ms_release_blob(bool internal, const char *db_name, const char *tab_name, char *blob_url, PBMSResultPtr result)
{

	if (have_handler_support && !internal) 
		return MS_OK;
	
	return MSEngine::dereferenceBlob(db_name, tab_name, blob_url, result);	
}

static int ms_drop_table(bool internal, const char *db_name, const char *tab_name, PBMSResultPtr result)
{
	if (have_handler_support && !internal) 
		return MS_OK;

	return MSEngine::dropTable(db_name, tab_name, result);	
}

static int ms_rename_table(bool internal, const char * db_name, const char *from_table, const char *to_db, const char *to_table, PBMSResultPtr result)
{
	if (have_handler_support && !internal) 
		return MS_OK;

	return MSEngine::renameTable(db_name, from_table, to_db, to_table, result);	
}

static void ms_completed(bool internal, bool ok)
{
	if (have_handler_support && !internal) 
		return;
		
	MSEngine::callCompleted(ok);	
}

PBMSCallbacksRec engine_callbacks = {
	MS_CALLBACK_VERSION,
	ms_register_engine,
	ms_deregister_engine,
	ms_create_blob,
	ms_retain_blob,
	ms_release_blob,
	ms_drop_table,
	ms_rename_table,
	ms_completed
};

// =============================
int MSEngine::startUp(PBMSResultPtr result)
{
	int err = 0;
	
	StreamingEngines = new PBMS_API();
	err = StreamingEngines->PBMSStartup(&engine_callbacks, result);
	if (err)
		delete StreamingEngines;
	else { // Register the PBMS enabled engines the startup before PBMS
		PBMSSharedMemoryPtr		sh_mem = StreamingEngines->sharedMemory;
		PBMSEnginePtr			engine;
		
		for (int i=0; i<sh_mem->sm_list_len; i++) {
			if ((engine = sh_mem->sm_engine_list[i])) 
				ms_register_engine(engine);
		}
	}
	return err;
}

void MSEngine::shutDown()
{
	StreamingEngines->PBMSShutdown();

	delete StreamingEngines;
}

const PBMSEnginePtr MSEngine::getEngineInfoAt(int indx)
{
	PBMSSharedMemoryPtr		sh_mem = StreamingEngines->sharedMemory;
	PBMSEnginePtr			engine = NULL;
	
	if (sh_mem) {
		for (int i=0; i<sh_mem->sm_list_len; i++) {
			if ((engine = sh_mem->sm_engine_list[i])) {
				if (!indx)
					return engine;
				indx--;
			}
		}
	}
	
	return (const PBMSEnginePtr)NULL;
}
#endif	

//---------------
bool MSEngine::try_createBlob(CSThread *self, const char *db_name, const char *tab_name, char *blob, size_t blob_len, PBMSBlobURLPtr blob_url)
{
	volatile bool rtc = true;
	
	try_(a) {
		MSOpenTable		*otab;
		CSInputStream	*i_stream = NULL;
		
		otab = openTable(db_name, tab_name, true);
		frompool_(otab);
		
		if (!otab->getDB()->isRecovering()) {
			i_stream = CSMemoryInputStream::newStream((unsigned char *)blob, blob_len);
			otab->createBlob(blob_url, blob_len, NULL, 0, i_stream);
		} else
			CSException::throwException(CS_CONTEXT, MS_ERR_RECOVERY_IN_PROGRESS, "Cannot create BLOBs during repository recovery.");

		backtopool_(otab);
		rtc = false;			
	}
	catch_(a);
	cont_(a);
	return rtc;
}

//---------------
int32_t	MSEngine::createBlob(const char *db_name, const char *tab_name, char *blob, size_t blob_len, PBMSBlobURLPtr blob_url, PBMSResultPtr result)
{

	CSThread		*self;
	int32_t			err = MS_OK;
	
	if ((err = enterConnectionNoThd(&self, result)))
		return err;

	inner_();
	if (try_createBlob(self, db_name, tab_name, blob, blob_len, blob_url))
		err = exceptionToResult(&self->myException, result);

	return_(err);
}

//---------------
bool MSEngine::try_referenceBlob(CSThread *self, const char *db_name, const char *tab_name, PBMSBlobURLPtr ret_blob_url, char *blob_url, uint16_t col_index)
{
	volatile bool rtc = true;
	try_(a) {
		MSBlobURLRec	blob;
		MSOpenTable		*otab;
		
		if (! PBMSBlobURLTools::couldBeURL(blob_url, &blob)){
			char buffer[CS_EXC_MESSAGE_SIZE];
			
			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Incorrect URL: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, blob_url);
			CSException::throwException(CS_CONTEXT, MS_ERR_INCORRECT_URL, buffer);
		}
		
		otab = openTable(db_name, tab_name, true);
		frompool_(otab);

		otab->useBlob(blob.bu_type, blob.bu_db_id, blob.bu_tab_id, blob.bu_blob_id, blob.bu_auth_code, col_index, blob.bu_blob_size, blob.bu_blob_ref_id, ret_blob_url);

		backtopool_(otab);
		rtc = false;			
	}
	catch_(a);
	cont_(a);
	return rtc;
}

//---------------
int32_t	MSEngine::referenceBlob(const char *db_name, const char *tab_name, PBMSBlobURLPtr ret_blob_url, char *blob_url, uint16_t col_index, PBMSResultPtr result)
{

	CSThread		*self;
	int32_t			err = MS_OK;
	
	if ((err = enterConnectionNoThd(&self, result)))
		return err;

	inner_();
	if (try_referenceBlob(self, db_name, tab_name, ret_blob_url, blob_url, col_index))
		err = exceptionToResult(&self->myException, result);

	return_(err);

}

//---------------
bool MSEngine::try_dereferenceBlob(CSThread *self, const char *db_name, const char *tab_name, char *blob_url)
{
	volatile bool rtc = true;
	try_(a) {
		MSBlobURLRec	blob;
		MSOpenTable		*otab;

		if (! PBMSBlobURLTools::couldBeURL(blob_url, &blob)){
			char buffer[CS_EXC_MESSAGE_SIZE];

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Incorrect URL: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, blob_url);
			CSException::throwException(CS_CONTEXT, MS_ERR_INCORRECT_URL, buffer);
		}
		
		otab = openTable(db_name, tab_name, true);
		frompool_(otab);
		if (!otab->getDB()->isRecovering()) {
			if (otab->getTableID() == blob.bu_tab_id)
				otab->releaseReference(blob.bu_blob_id, blob.bu_blob_ref_id);
			else {
				char buffer[CS_EXC_MESSAGE_SIZE];

				cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Incorrect table ID: ");
				cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, blob_url);
				CSException::throwException(CS_CONTEXT, MS_ERR_INCORRECT_URL, buffer);
			}
		}
		else {
			char buffer[CS_EXC_MESSAGE_SIZE];

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Incorrect URL: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, blob_url);
			CSException::throwException(CS_CONTEXT, MS_ERR_INCORRECT_URL, buffer);
		}
		
		backtopool_(otab);	
		rtc = false;			
	}
	catch_(a);
	cont_(a);
	return rtc;
}

int32_t	MSEngine::dereferenceBlob(const char *db_name, const char *tab_name, char *blob_url, PBMSResultPtr result)
{
	CSThread		*self;
	int32_t			err = MS_OK;

	if ((err = enterConnectionNoThd(&self, result)))
		return err;

	inner_();
	if (try_dereferenceBlob(self, db_name, tab_name, blob_url))
		err = exceptionToResult(&self->myException, result);

	return_(err);
}

bool MSEngine::try_dropDatabase(CSThread *self, const char *db_name)
{
	volatile bool rtc = true;
	try_(a) {
		MSDatabase::dropDatabase(db_name);
		rtc = false;
	}
	catch_(a);
	cont_(a);
	
	return rtc;
}

int32_t MSEngine::dropDatabase(const char *db_name, PBMSResultPtr result)
{
	CSThread *self;
	int		err = MS_OK;
	
	if ((err = enterConnectionNoThd(&self, result)))
		return err;

	inner_();
	
	if (try_dropDatabase(self, db_name))
		err = exceptionToResult(&self->myException, result);

	return_(err);
}

//---------------
typedef struct UnDoInfo {
	bool udo_WasRename;
	CSString *udo_toDatabaseName;
	CSString *udo_fromDatabaseName;
	CSString *udo_OldName;
	CSString *udo_NewName;
} UnDoInfoRec, *UnDoInfoPtr;

//---------------
bool MSEngine::try_dropTable(CSThread *self, const char *db_name, const char *tab_name)
{
	volatile bool rtc = true;
	try_(a) {

		CSPath			*new_path;
		CSPath			*old_path;
		MSOpenTable		*otab;
		MSOpenTablePool	*tab_pool;
		MSTable			*tab;
		UnDoInfoPtr		undo_info = NULL;

		undo_info = (UnDoInfoPtr) cs_malloc(sizeof(UnDoInfoRec));
		
		undo_info->udo_WasRename = false;
		self->myInfo = undo_info;

		otab = openTable(db_name, tab_name, false);
		if (!otab) {
			goto end_try;
		}
		
		// If we are recovering do not delete the table.
		// It is normal for MySQL recovery scripts to delete any table they aare about to
		// recover and then recreate it. If this is done after the repository has been recovered
		// then this would delete all the recovered BLOBs in the table.
		if (otab->getDB()->isRecovering()) {
			otab->returnToPool();
			goto end_try;
		}

		frompool_(otab);

		// Before dropping the table the table ref file is renamed so that
		// it is out of the way incase a new table is created before the
		// old one is cleaned up.
		
		old_path = otab->getDBTable()->getTableFile();
		push_(old_path);

		new_path = otab->getDBTable()->getTableFile(tab_name, true);

		// Rearrage the object stack to pop the otab object
		pop_(old_path);
		pop_(otab);

		push_(new_path);
		push_(old_path);
		frompool_(otab);
		
		tab = otab->getDBTable();
		pop_(otab);
		push_(tab);

		tab_pool = MSTableList::lockTablePoolForDeletion(otab);
		frompool_(tab_pool);

		if (old_path->exists())
			old_path->move(RETAIN(new_path));
		tab->myDatabase->dropTable(RETAIN(tab));
		
		/* Add the table to the temp delete list if we are not recovering... */
		tab->prepareToDelete();

		backtopool_(tab_pool);	// The will unlock and close the table pool freeing all tables in it.
		pop_(tab);				// Returning the pool will have released this. (YUK!)
		release_(old_path);
		release_(new_path);


				
end_try:
		rtc = false;	
	}
	catch_(a);
	cont_(a);
	return rtc;
}

//---------------
int32_t	MSEngine::dropTable(const char *db_name, const char *tab_name, PBMSResultPtr result)
{
	CSThread	*self;
	int			err = MS_OK;

	if ((err = enterConnectionNoThd(&self, result)))
		return err;

	inner_();
	if (try_dropTable(self, db_name, tab_name))
		err = exceptionToResult(&self->myException, result);

	outer_();
	exitConnection();
	return err;
}

//---------------
static void completeDeleteTable(UnDoInfoPtr info, bool ok)
{
	// TO DO: figure out a way to undo the delete.
	cs_free(info);
	if (!ok) 
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_IMPLEMENTED, "Cannot undo delete table.");
}

//---------------
bool MSEngine::renameTable(const char *from_db_name, const char *from_table, const char *to_db_name, const char *to_table)
{
	MSOpenTable		*otab;
	CSPath			*from_path;
	CSPath			*to_path;
	MSOpenTablePool	*tab_pool;
	MSTable			*tab;

	enter_();
	
	if (strcmp(to_db_name, from_db_name) != 0) {
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_IMPLEMENTED, "Cannot rename tables containing BLOBs across databases (yet). Sorry!");
	}
	
	otab = openTable(from_db_name, from_table, false);
	if (!otab)
		return_(false);
		
	frompool_(otab);

	if (otab->getDB()->isRecovering()) 
		CSException::throwException(CS_CONTEXT, MS_ERR_RECOVERY_IN_PROGRESS, "Cannot rename tables during repository recovery.");

	from_path = otab->getDBTable()->getTableFile();
	push_(from_path);

	to_path = otab->getDBTable()->getTableFile(to_table, false);

	// Rearrage the object stack to pop the otab object
	pop_(from_path);
	pop_(otab);

	push_(to_path);
	push_(from_path);
	frompool_(otab);

	otab->openForReading();
	tab = otab->getDBTable();
	tab->retain();
	pop_(otab);
	push_(tab);
	
	tab_pool = MSTableList::lockTablePoolForDeletion(otab);
	frompool_(tab_pool);

	from_path->move(RETAIN(to_path));
	tab->myDatabase->renameTable(tab, to_table);

	backtopool_(tab_pool);	// The will unlock and close the table pool freeing all tables in it.
	pop_(tab);				// Returning the pool will have released this. (YUK!)
	release_(from_path);
	release_(to_path);
	
	return_(true);
}

//---------------
bool MSEngine::try_renameTable(CSThread *self, const char *from_db_name, const char *from_table, const char *to_db_name, const char *to_table)
{
	volatile bool rtc = true;
	try_(a) {
		UnDoInfoPtr undo_info = (UnDoInfoPtr) cs_malloc(sizeof(UnDoInfoRec));
		push_ptr_(undo_info);

		undo_info->udo_WasRename = true;
		if (renameTable(from_db_name, from_table, to_db_name, to_table)) {		
			undo_info->udo_fromDatabaseName = CSString::newString(from_db_name);
			push_(undo_info->udo_fromDatabaseName);

			undo_info->udo_toDatabaseName = CSString::newString(to_db_name);
			push_(undo_info->udo_toDatabaseName);

			undo_info->udo_OldName = CSString::newString(from_table);
			push_(undo_info->udo_OldName);

			undo_info->udo_NewName = CSString::newString(to_table);
			
			pop_(undo_info->udo_OldName);
			pop_(undo_info->udo_toDatabaseName);
			pop_(undo_info->udo_fromDatabaseName);
		} else {
			undo_info->udo_fromDatabaseName = undo_info->udo_toDatabaseName = undo_info->udo_OldName = undo_info->udo_NewName = NULL;
		}
		self->myInfo = undo_info;
		pop_(undo_info);
		rtc = false;			
	}
	catch_(a);
	cont_(a);
	return rtc;
}

//---------------
int32_t	MSEngine::renameTable(const char *from_db_name, const char *from_table, const char *to_db_name, const char *to_table, PBMSResultPtr result)
{
	CSThread	*self;
	int err = MS_OK;

	if ((err = enterConnectionNoThd(&self, result)))
		return err;

	inner_();
	if (try_renameTable(self, from_db_name, from_table, to_db_name, to_table))
		err = exceptionToResult(&self->myException, result);

	outer_();
	exitConnection();
	return err;
}

//---------------
void MSEngine::completeRenameTable(UnDoInfoPtr info, bool ok)
{
	// Swap the paths around here to revers the rename.
	CSString		*from_db_name= info->udo_toDatabaseName;
	CSString		*to_db_name= info->udo_fromDatabaseName;
	CSString		*from_table= info->udo_NewName;
	CSString		*to_table= info->udo_OldName;
	
	enter_();
	
	cs_free(info);
	if (from_db_name) {
		push_(from_db_name);
		push_(from_table);
		push_(to_db_name);
		push_(to_table);
		if (!ok) 
			renameTable(from_db_name->getCString(), from_table->getCString(), to_db_name->getCString(), to_table->getCString());
			
		release_(to_table);
		release_(to_db_name);
		release_(from_table);
		release_(from_db_name);
	}
	exit_();
}

//---------------
static bool try_CompleteTransaction(CSThread *self, bool ok)
{
	volatile bool rtc = true;
	try_(a) {
		if (ok)
			MSTransactionManager::commit();
		else if (self->myIsAutoCommit)
			MSTransactionManager::rollback();
		else
			MSTransactionManager::rollbackToPosition(self->myStartStmt); // Rollback the last logical statement.
		rtc = false;
	}
	catch_(a)
	cont_(a);
	
	return rtc;
}

//---------------
void MSEngine::callCompleted(bool ok)
{
	CSThread	*self;
	PBMSResultRec	result;
	
	if (enterConnectionNoThd(&self, &result))
		return ;

	if (self->myInfo) {
		UnDoInfoPtr info = (UnDoInfoPtr) self->myInfo;
		if (info->udo_WasRename) 
			completeRenameTable(info, ok);
		else 
			completeDeleteTable(info, ok);

		
		self->myInfo = NULL;
	} else if (self->myTID && (self->myIsAutoCommit || !ok)) {
		inner_();
		if (try_CompleteTransaction(self, ok)) {
			self->logException();
		}
		outer_();
	}
	
	self->myStartStmt = self->myStmtCount;
}

//---------------
MSOpenTable *MSEngine::openTable(const char *db_name, const char *tab_name, bool create)
{
	MSOpenTable		*otab = NULL;
	uint32_t db_id, tab_id;
	enter_();
	
	if ( MSDatabase::convertTableAndDatabaseToIDs(db_name, tab_name, &db_id, &tab_id, create))  
		otab = MSTableList::getOpenTableByID(db_id, tab_id);
		
	return_(otab);
}

//---------------
bool MSEngine::couldBeURL(const char *blob_url, size_t length)
{
	MSBlobURLRec blob;
	return PBMSBlobURLTools::couldBeURL(blob_url, length, &blob);
}

//---------------
int MSEngine::exceptionToResult(CSException *e, PBMSResultPtr result)
{
	const char *context, *trace;

	result->mr_code = e->getErrorCode();
	cs_strcpy(MS_RESULT_MESSAGE_SIZE, result->mr_message, e->getMessage());
	context = e->getContext();
	trace = e->getStackTrace();
	if (context && *context) {
		cs_strcpy(MS_RESULT_STACK_SIZE, result->mr_stack, context);
		if (trace && *trace)
			cs_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, "\n");
	}
	else
		*result->mr_stack = 0;
	if (trace && *trace)
		cs_strcat(MS_RESULT_STACK_SIZE, result->mr_stack, trace);
	return MS_ERR_ENGINE;
}

//---------------
int MSEngine::errorResult(const char *func, const char *file, int line, int err, const char *message, PBMSResultPtr result)
{
	CSException e;
		
	e.initException(func, file, line, err, message);
	return exceptionToResult(&e, result);
}

//---------------
int MSEngine::osErrorResult(const char *func, const char *file, int line, int err, PBMSResultPtr result)
{
	CSException e;
		
	e.initOSError(func, file, line, err);
	return MSEngine::exceptionToResult(&e, result);
}

//---------------
int MSEngine::enterConnection(THD *thd, CSThread **r_self, PBMSResultPtr result, bool doCreate)
{
	CSThread	*self = NULL;

#ifndef DRIZZLED
	// In drizzle there is no 1:1 relationship between pthreads and sessions
	// so we must always get it from the session handle NOT the current pthread.
	self = CSThread::getSelf();
#endif
	if (!self) {	
		if (thd) {
			if (!(self = pbms_getMySelf(thd))) {
				if (!doCreate)
					return MS_ERR_NOT_FOUND;
					
				if (!(self = CSThread::newCSThread()))
					return osErrorResult(CS_CONTEXT, ENOMEM, result);
				if (!CSThread::attach(self))
					return MSEngine::exceptionToResult(&self->myException, result);
				pbms_setMySelf(thd, self);
			} else {
				if (!CSThread::setSelf(self))
					return MSEngine::exceptionToResult(&self->myException, result);
			}
		} else {
			if (!doCreate)
				return MS_ERR_NOT_FOUND;
				
			if (!(self = CSThread::newCSThread()))
				return osErrorResult(CS_CONTEXT, ENOMEM, result);
			if (!CSThread::attach(self))
				return MSEngine::exceptionToResult(&self->myException, result);
		}
	}

	*r_self = self;
	return MS_OK;
}

//---------------
int MSEngine::enterConnectionNoThd(CSThread **r_self, PBMSResultPtr result)
{
	return enterConnection(current_thd, r_self, result, true);
}

//---------------
void MSEngine::exitConnection()
{
	THD			*thd = (THD *) current_thd;
	CSThread	*self;

	self = CSThread::getSelf();
	if (self && self->pbms_api_owner)
		return;


	if (thd)
		CSThread::setSelf(NULL);
	else {
		self = CSThread::getSelf();
		CSThread::detach(self);
	}
}

//---------------
void MSEngine::closeConnection(THD* thd)
{
	CSThread	*self;

	self = CSThread::getSelf();
	if (self && self->pbms_api_owner)
		return;

	if (thd) {
		if ((self = pbms_getMySelf(thd))) {
			pbms_setMySelf(thd, NULL);
			CSThread::setSelf(self);
			CSThread::detach(self);
		}
	}
	else {
		self = CSThread::getSelf();
		CSThread::detach(self);
	}
}




