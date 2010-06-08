/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-05-20
 *
 * H&G2JCtL
 *
 * Table handler.
 *
 */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#ifdef DRIZZLED
#include "config.h"
#include <drizzled/common.h>
#include <drizzled/plugin.h>
#include <drizzled/field.h>
#include <drizzled/session.h>
#include <drizzled/data_home.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/plugin/transactional_storage_engine.h>

#define my_strdup(a,b) strdup(a)
using namespace drizzled;
using namespace drizzled::plugin;



#include "cslib/CSConfig.h"
#else
#include "cslib/CSConfig.h"
#include "mysql_priv.h"
#include <mysql/plugin.h>
#include <my_dir.h>
#endif 

#include <stdlib.h>
#include <time.h>
#include <inttypes.h>


#include "Defs_ms.h"

#include "cslib/CSDefs.h"
#include "cslib/CSObject.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSThread.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSTest.h"
#include "cslib/CSLog.h"

#include "Engine_ms.h"	
#include "ha_pbms.h"
#include "Network_ms.h"
#include "ConnectionHandler_ms.h"
#include "OpenTable_ms.h"
#include "Database_ms.h"
#include "TempLog_ms.h"
#include "Util_ms.h"
#include "SystemTable_ms.h"
#include "ms_mysql.h"
#include "Discover_ms.h"
#include "metadata_ms.h"
#include "Transaction_ms.h"
#include "SysTab_httpheader.h"
#include "SystemTable_ms.h"
#include "parameters_ms.h"

/* Note: 'new' used here is NOT CSObject::new which is a DEBUG define*/
#ifdef new
#undef new
#endif


#ifdef DRIZZLED

static int pbms_done_func(void *);

class PBMSStorageEngine : public drizzled::plugin::TransactionalStorageEngine {
public:
	PBMSStorageEngine()
	: TransactionalStorageEngine(std::string("PBMS"), HTON_NO_FLAGS | HTON_HIDDEN) {}

	~PBMSStorageEngine()
	{
		pbms_done_func(NULL);
	}
	
	int close_connection(Session *);
	
	int doStartTransaction(Session *session, start_transaction_option_t options);
	int doCommit(Session *, bool);
	int doRollback(Session *, bool);
	Cursor *create(TableShare& table, memory::Root *mem_root);
	void drop_database(char *);
	
	/*
	* Indicates to a storage engine the start of a
	* new SQL statement.
	*/
	void doStartStatement(Session *session)
	{
		(void) session;
	}

	/*
	* Indicates to a storage engine the end of
	* the current SQL statement in the supplied
	* Session.
	*/
	void doEndStatement(Session *session)
	{
		(void) session;
	}
	
	int doCreateTable(Session&, Table&, TableIdentifier& ident, drizzled::message::Table& );	
	int doDropTable(Session &, TableIdentifier& );
	
	int doRenameTable(Session&, TableIdentifier &from, TableIdentifier &to);
	
	void doGetTableIdentifiers(drizzled::CachedDirectory &dir,
                             drizzled::SchemaIdentifier &schema,
                             drizzled::TableIdentifiers &set_of_identifiers) 
	{
		std::set<std::string> set_of_names;
		
		doGetTableNames(dir, schema, set_of_names);
		for (std::set<std::string>::iterator set_iter = set_of_names.begin(); set_iter != set_of_names.end(); ++set_iter)
		{
			set_of_identifiers.push_back(TableIdentifier(schema, *set_iter));
		}
	}
	
	void doGetTableNames(CachedDirectory&, 
					SchemaIdentifier &schema, 
					std::set<std::string> &set_of_names) 
	{
		bool isPBMS = schema.compare("PBMS");
		
		if (isPBMS || PBMSParameters::isBLOBDatabase(schema.getSchemaName().c_str()))
			PBMSSystemTables::getSystemTableNames(isPBMS, set_of_names);
	}

	int doSetSavepoint(Session *thd, NamedSavepoint &savepoint);
	int doRollbackToSavepoint(Session *session, NamedSavepoint &savepoint);
	int doReleaseSavepoint(Session *session, NamedSavepoint &savepoint);
	const char **bas_ext() const;

  int doGetTableDefinition(Session&, TableIdentifier &identifier,
                                          drizzled::message::Table &table_proto)
  {
		int err;
		const char *tab_name = identifier.getTableName().c_str();

		// Set some required table proto info:
 		table_proto.set_schema(identifier.getSchemaName().c_str());
		table_proto.set_creation_timestamp(0);
		table_proto.set_update_timestamp(0);
		
		err = PBMSSystemTables::getSystemTableInfo(tab_name, table_proto);
		if (err)
			return err;
			
		return EEXIST;
  }

	bool doDoesTableExist(Session&, TableIdentifier &identifier)
	{
		const char *tab_name = identifier.getTableName().c_str();
		const char *db_name = identifier.getSchemaName().c_str();
		bool isPBMS = identifier.getSchemaName().compare("PBMS");
		
		if (isPBMS || PBMSParameters::isBLOBDatabase(db_name)) {
			return PBMSSystemTables::isSystemTable(isPBMS, tab_name);									 
		}
		
		return false;		
	}


};

PBMSStorageEngine	*pbms_hton;
#else
handlerton		*pbms_hton;
#endif

static const char *ha_pbms_exts[] = {
	NullS
};

/*
 * ---------------------------------------------------------------
 * UTILITIES
 */

#ifndef DRIZZLED
void pbms_take_part_in_transaction(void *thread)
{
	THD			*thd;
	if ((thd = (THD *) thread)) {
		trans_register_ha(thd, true, pbms_hton); 
	}
}
#endif

#ifdef DRIZZLED
const char **PBMSStorageEngine::bas_ext() const
#else
const char **ha_pbms::bas_ext() const
#endif
{
	return ha_pbms_exts;
}

#ifdef DRIZZLED
#define GET_MY_SELF(thd) ((CSThread *) *thd->getEngineData(pbms_hton))
#define SET_MY_SELF(thd, self) *thd->getEngineData(pbms_hton) = (void *)self
#else
#define GET_MY_SELF(thd) ((CSThread *) *thd_ha_data(thd, pbms_hton))
#define SET_MY_SELF(thd, self) *thd_ha_data(thd, pbms_hton) = (void *)self
#endif

#ifdef DRIZZLED
int PBMSStorageEngine::close_connection(Session *thd)
{
#else
static int pbms_close_connection(handlerton *hton, THD* thd)
{
	(void)hton;
#endif
	CSThread	*self;

	self = CSThread::getSelf();
	if (self && self->pbms_api_owner)
		return 0;

	if (thd) {
		if ((self = GET_MY_SELF(thd))) {
			SET_MY_SELF(thd, NULL);
			CSThread::setSelf(self);
			CSThread::detach(self);
		}
	}
	else {
		self = CSThread::getSelf();
		CSThread::detach(self);
	}
	return 0;
}


static int pbms_enter_conn(THD *thd, CSThread **r_self, PBMSResultPtr result, bool doCreate)
{
	CSThread	*self = NULL;

#ifndef DRIZZLED
	// In drizzle there is no 1:1 relationship between pthreads and sessions
	// so we must always get it from the session handle NOT the current pthread.
	self = CSThread::getSelf();
#endif
	if (!self) {	
		if (thd) {
			if (!(self = GET_MY_SELF(thd))) {
				if (!doCreate)
					return MS_ERR_NOT_FOUND;
					
				if (!(self = CSThread::newCSThread()))
					return pbms_os_error_result(CS_CONTEXT, ENOMEM, result);
				if (!CSThread::attach(self))
					return pbms_exception_to_result(&self->myException, result);
				SET_MY_SELF(thd, self);
			} else {
				if (!CSThread::setSelf(self))
					return pbms_exception_to_result(&self->myException, result);
			}
		} else {
			if (!doCreate)
				return MS_ERR_NOT_FOUND;
				
			if (!(self = CSThread::newCSThread()))
				return pbms_os_error_result(CS_CONTEXT, ENOMEM, result);
			if (!CSThread::attach(self))
				return pbms_exception_to_result(&self->myException, result);
		}
	}

	*r_self = self;
	return MS_OK;
}

int pbms_enter_conn_no_thd(CSThread **r_self, PBMSResultPtr result)
{
	return pbms_enter_conn(current_thd, r_self, result, true);
}

void pbms_exit_conn()
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

int pbms_exception_to_result(CSException *e, PBMSResultPtr result)
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

int pbms_os_error_result(const char *func, const char *file, int line, int err, PBMSResultPtr result)
{
	CSException e;
		
	e.initOSError(func, file, line, err);
	return pbms_exception_to_result(&e, result);
}

int pbms_error_result(const char *func, const char *file, int line, int err, const char *message, PBMSResultPtr result)
{
	CSException e;
		
	e.initException(func, file, line, err, message);
	return pbms_exception_to_result(&e, result);
}

/*
 * ---------------------------------------------------------------
 * HANDLER INTERFACE
 */


#ifdef DRIZZLED
Cursor *PBMSStorageEngine::create(TableShare& table, memory::Root *mem_root)
{
	PBMSStorageEngine * const hton = this;
	return new (mem_root) ha_pbms(hton, table);
}
#else
static handler *pbms_create_handler(handlerton *hton, TABLE_SHARE *table, MEM_ROOT *mem_root)
{
	return new (mem_root) ha_pbms(hton, table);
}
#endif

#ifdef DRIZZLED
int PBMSStorageEngine::doStartTransaction(Session *session, start_transaction_option_t options)
{
	(void)session;
	(void)options;
	
	return 0;
}

int PBMSStorageEngine::doCommit(Session *thd, bool all __attribute__((unused)))
{
#else
static int pbms_commit(handlerton *, THD *thd, bool all __attribute__((unused)))
{
#endif
	int			err = 0;
	CSThread	*self;
	PBMSResultRec result;

	if (pbms_enter_conn(thd, &self, &result, false))
		return 0;
	inner_();
	try_(a) {
		MSTransactionManager::commit();
	}
	catch_(a) {
		err = pbms_exception_to_result(&self->myException, &result);
	}
	cont_(a);
	return_(err);
}

#ifdef DRIZZLED
int PBMSStorageEngine::doRollback(THD *thd, bool all __attribute__((unused)))
{
#else
static int pbms_rollback(handlerton *, THD *thd, bool all __attribute__((unused)))
{
#endif
	int			err = 0;
	CSThread	*self;
	PBMSResultRec result;

	if (pbms_enter_conn(thd, &self, &result, false))
		return 0;
	inner_();
	try_(a) {
		MSTransactionManager::rollback();
	}
	catch_(a) {
		err = pbms_exception_to_result(&self->myException, &result);
	}
	cont_(a);
	return_(err);
}

#ifdef DRIZZLED
int PBMSStorageEngine::doSetSavepoint(Session *thd, NamedSavepoint &savepoint)
{
	int			err = 0;
	CSThread	*self;
	PBMSResultRec result;

	if (pbms_enter_conn(thd, &self, &result, false))
		return 0;
	
	inner_();
	try_(a) {
		MSTransactionManager::setSavepoint(savepoint.getName().c_str());
	}
	catch_(a) {
		err = pbms_exception_to_result(&self->myException, &result);
	}
	cont_(a);
	return_(err);
	
}

int PBMSStorageEngine::doRollbackToSavepoint(Session *session, NamedSavepoint &savepoint)
{
	int			err = 0;
	CSThread	*self;
	PBMSResultRec result;

	if (pbms_enter_conn(session, &self, &result, false))
		return 0;
	inner_();
	try_(a) {
		MSTransactionManager::rollbackTo(savepoint.getName().c_str());
	}
	catch_(a) {
		err = pbms_exception_to_result(&self->myException, &result);
	}
	cont_(a);
	return_(err);
}


int PBMSStorageEngine::doReleaseSavepoint(Session *session, NamedSavepoint &savepoint)
{
	int			err = 0;
	CSThread	*self;
	PBMSResultRec result;

	if (pbms_enter_conn(session, &self, &result, false))
		return 0;
		
	inner_();
	try_(a) {
		MSTransactionManager::releaseSavepoint(savepoint.getName().c_str());
	}
	catch_(a) {
		err = pbms_exception_to_result(&self->myException, &result);
	}
	cont_(a);
	return_(err);
}

#else
static int pbms_savepoint_set(handlerton *hton, THD *thd, void *sv)
{
	int			err = 0;
	CSThread	*self;
	PBMSResultRec result;

	if (pbms_enter_conn(thd, &self, &result, false))
		return 0;
		
	*((csWord4*)sv) = self->myStmtCount;
	return 0;	
}

static int pbms_savepoint_rollback(handlerton *hton, THD *thd, void *sv)
{
	int			err = 0;
	CSThread	*self;
	PBMSResultRec result;

	if (pbms_enter_conn(thd, &self, &result, false))
		return 0;
	inner_();
	try_(a) {
		MSTransactionManager::rollbackToPosition(*((csWord4*)sv));
	}
	catch_(a) {
		err = pbms_exception_to_result(&self->myException, &result);
	}
	cont_(a);
	return_(err);
}

static int pbms_savepoint_release(handlerton *hton, THD *thd, void *sv)
{
	return 0;
}

#endif

#ifdef DRIZZLED
void PBMSStorageEngine::drop_database(char *path)
{
#else
static void pbms_drop_database(handlerton *, char *path)
{
#endif
	CSThread *self;
	char db_name[PATH_MAX];
	PBMSResultRec result;
	
	if (pbms_enter_conn_no_thd(&self, &result))
		return;
	inner_();
	
	cs_strcpy(PATH_MAX, db_name, cs_last_directory_of_path(path));
	cs_remove_dir_char(db_name);
	try_(a) {
		MSDatabase::dropDatabase(db_name);
	}
	catch_(a);
	self->logException();
	cont_(a);
	exit_();
}

static bool pbms_started = false;


#ifdef DRIZZLED
int pbms_init_func(module::Context &registry)
#else
int pbms_discover_system_tables(handlerton *hton, THD* thd, const char *db, const char *name, uchar **frmblob, size_t *frmlen);
int pbms_init_func(void *p)
#endif
{
	PBMSResultRec		result;
	int					err;
	int					my_res = 0;
	CSThread			*thread;

	ASSERT(!pbms_started);
	pbms_started = false;
	
	{
		char info[120];
		snprintf(info, 120, "PrimeBase Media Stream (PBMS) Daemon %s loaded...", ms_version());
		CSL.logLine(NULL, CSLog::Protocol, info);
	}
	CSL.logLine(NULL, CSLog::Protocol, "Barry Leslie, PrimeBase Technologies GmbH, http://www.primebase.org");
	
	if ((err = MSEngine::startUp(&result))) {
		CSL.logLine(NULL, CSLog::Error, result.mr_message);
		return(1);
	}

#ifdef DRIZZLED
		pbms_hton= new PBMSStorageEngine();
		registry.add(pbms_hton);
#else
	pbms_hton = (handlerton *) p;
	pbms_hton->state = SHOW_OPTION_YES;
	pbms_hton->close_connection = pbms_close_connection; /* close_connection, cleanup thread related data. */
	pbms_hton->create = pbms_create_handler;
	pbms_hton->flags = HTON_CAN_RECREATE | HTON_HIDDEN;
	pbms_hton->drop_database = pbms_drop_database; /* Drop a database */
	pbms_hton->discover = pbms_discover_system_tables;

	pbms_hton->commit = pbms_commit; /* commit */
	pbms_hton->rollback = pbms_rollback; /* rollback */

	pbms_hton->savepoint_offset = 4;
	pbms_hton->savepoint_set = pbms_savepoint_set;
	pbms_hton->savepoint_rollback = pbms_savepoint_rollback; 
	pbms_hton->savepoint_release = pbms_savepoint_release; 
#endif
	
	/* Startup the Media Stream network: */
	cs_init_memory();
	CSThread::startUp();
	if (!(thread = CSThread::newCSThread())) {
		CSException::logOSError(CS_CONTEXT, ENOMEM);
		return(1);
	}
	if (!CSThread::attach(thread)) {
		thread->myException.log(NULL);
		CSThread::shutDown();
		cs_exit_memory();
		MSEngine::shutDown();
		return(1);
	}
	enter_();
	try_(a) {
		thread->threadName = CSString::newString("startup");
		//CSTest::runAll();
		MSDatabase::startUp(PBMSParameters::getDefaultMetaDataHeaders());
		MSTableList::startUp();
		MSSystemTableShare::startUp();
		MSNetwork::startUp(PBMSParameters::getPortNumber());
		MSTransactionManager::startUp();
		MSNetwork::startNetwork();
	}
	catch_(a) {
		self->logException();
		my_res = 1;
	}
	cont_(a);
	if (my_res) {
		try_(b) {
			MSNetwork::shutDown();
			MSTransactionManager::shutDown();
			MSSystemTableShare::shutDown();
			MSDatabase::stopThreads();
			MSTableList::shutDown();
			MSDatabase::shutDown();
			CSThread::shutDown();
		}
		catch_(b) {
			self->logException();
		}
		cont_(b);
	}
	outer_();
	CSThread::detach(thread);

	if (my_res) {
		cs_exit_memory();
		MSEngine::shutDown();
	}
	else {
		srandom(time(NULL));
		pbms_started = true;
		
	}

	return(my_res);
}

#ifdef DRIZZLED
static int pbms_done_func(void *)
#else
int pbms_done_func(void *)
#endif
{
	CSThread	*thread;

	if (!pbms_started)
		return 0;

	CSL.logLine(NULL, CSLog::Protocol, "PrimeBase Media Stream (PBMS) Daemon shutdown...");
	
	/* Shutdown the Media Stream network. */
	if (!(thread = CSThread::newCSThread()))
		CSException::logOSError(CS_CONTEXT, ENOMEM);
	else if (!CSThread::attach(thread))
		thread->myException.log(NULL);
	else {
		enter_();
		try_(a) {
			thread->threadName = CSString::newString("shutdown");
			MSNetwork::shutDown();
			MSSystemTableShare::shutDown();
			/* Ensure that the database threads are stopped before
			 * freeing the tables.
			 */
			MSDatabase::stopThreads();
			MSTableList::shutDown();
			/* Databases must be shutdown after table because tables
			 * have references to repositories.
			 */
			MSDatabase::shutDown();
			
			/* Shutdown the transaction manager after the databases
			 * incase they want to commit or rollback a transaction.
			 */
			MSTransactionManager::shutDown();
		}
		catch_(a) {
			self->logException();
		}
		cont_(a);
		outer_();
		CSThread::shutDown();
		CSThread::detach(thread);
	}

	MSEngine::shutDown();
	cs_exit_memory();

	CSL.logLine(NULL, CSLog::Protocol, "PrimeBase Media Stream (PBMS) Daemon shutdown completed");
	pbms_started = false;
	return(0);
}

#ifdef DRIZZLED
ha_pbms::ha_pbms(handlerton *hton, TableShare& table_arg) : handler(*hton, table_arg),
#else
ha_pbms::ha_pbms(handlerton *hton, TABLE_SHARE *table_arg) : handler(hton, table_arg),
#endif
ha_open_tab(NULL),
ha_error(0)
{
	memset(&ha_result, 0, sizeof(PBMSResultRec));
}

#ifndef DRIZZLED
MX_TABLE_TYPES_T ha_pbms::table_flags() const
{
	return (
		/* We need this flag because records are not packed
		 * into a table which means #ROWID != offset
		 */
		HA_REC_NOT_IN_SEQ |
		HA_CAN_SQL_HANDLER |
#if MYSQL_VERSION_ID > 50119
		/* We can do row logging, but not statement, because
		 * MVCC is not serializable!
		 */
		HA_BINLOG_ROW_CAPABLE |
#endif
		/*
		 * Auto-increment is allowed on a partial key.
		 */
		0);
}
#endif

int ha_pbms::open(const char *table_path, int , uint )
{
	CSThread *self;

	if ((ha_error = pbms_enter_conn(current_thd, &self, &ha_result, true)))
		return 1;

	inner_();
	try_(a) {
		ha_open_tab = MSSystemTableShare::openSystemTable(table_path, table);
		thr_lock_data_init(&ha_open_tab->myShare->myThrLock, &ha_lock, NULL);
		ref_length = ha_open_tab->getRefLen();
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
	}
	cont_(a);
	return_(ha_error != MS_OK);
}

int ha_pbms::close(void)
{
	CSThread *self;

	if ((ha_error = pbms_enter_conn(current_thd, &self, &ha_result, true)))
		return 1;

	inner_();
	if (ha_open_tab) {
		ha_open_tab->release();
		ha_open_tab = NULL;
	}
	outer_();
	pbms_exit_conn();
	return 0;
}

#ifdef PBMS_HAS_KEYS
/* Index access functions: */
int ha_pbms::index_init(uint idx, bool sorted __attribute__((unused)))
{
	int err = 0;
	enter_();
	try_(a) {
		ha_open_tab->index_init(idx);
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_end()
{
	int err = 0;
	enter_();
	try_(a) {
		ha_open_tab->index_end();
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_read(byte * buf, const byte * key,
							 uint key_len, enum ha_rkey_function find_flag)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->index_read(buf, key, key_len, find_flag))
			err = HA_ERR_KEY_NOT_FOUND;

	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_read_idx(byte * buf, uint idx, const byte * key,
									 uint key_len, enum ha_rkey_function find_flag)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->index_read_idx(buf, idx, key, key_len, find_flag))
			err = HA_ERR_KEY_NOT_FOUND;
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_next(byte * buf)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->index_next(buf))
			err = HA_ERR_END_OF_FILE;
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_prev(byte * buf)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->index_prev(buf))
			err = HA_ERR_END_OF_FILE;
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_first(byte * buf)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->index_first(buf))
			err = HA_ERR_END_OF_FILE;
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_last(byte * buf)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->index_last(buf))
			err = HA_ERR_END_OF_FILE;
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::index_read_last(byte * buf, const byte * key, uint key_len)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->index_read_last(buf, key, key_len))
			err = HA_ERR_KEY_NOT_FOUND;
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------

#endif // PBMS_HAS_KEYS

/* Sequential scan functions: */
#ifdef DRIZZLED
int ha_pbms::doStartTableScan(bool )
#else
int ha_pbms::rnd_init(bool )
#endif
{
	int err = 0;
	enter_();
	try_(a) {
		ha_open_tab->seqScanInit();
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
int ha_pbms::rnd_next(unsigned char *buf)
{
	int err = 0;
	enter_();
	try_(a) {
		if (!ha_open_tab->seqScanNext((char *) buf))
			err = HA_ERR_END_OF_FILE;
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//-------
void ha_pbms::position(const unsigned char *)
{
	ha_open_tab->seqScanPos((uint8_t *) ref);
}

//-------
int ha_pbms::rnd_pos(unsigned char * buf, unsigned char *pos)
{
	int err = 0;
	enter_();
	try_(a) {
		ha_open_tab->seqScanRead((uint8_t *) pos, (char *) buf);
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

//////////////////////////////
#ifdef DRIZZLED
int	ha_pbms::doInsertRecord(byte * buf)
#else
int ha_pbms::write_row(unsigned char * buf)
#endif
{
	int err = 0;
	enter_();
	try_(a) {
		ha_open_tab->insertRow((char *) buf);
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

#ifdef DRIZZLED
int	ha_pbms::doDeleteRecord(const byte * buf)
#else
int ha_pbms::delete_row(const  unsigned char * buf)
#endif
{
	int err = 0;
	enter_();
	try_(a) {
		ha_open_tab->deleteRow((char *) buf);
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

#ifdef DRIZZLED
int	ha_pbms::doUpdateRecord(const byte * old_data, byte * new_data)
#else
int ha_pbms::update_row(const unsigned char * old_data, unsigned char * new_data)
#endif
{
	int err = 0;
	enter_();
	try_(a) {
		ha_open_tab->updateRow((char *) old_data, (char *) new_data);
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

int ha_pbms::info(uint )
{
	return 0;
}

int ha_pbms::external_lock(THD *thd, int lock_type)
{
	CSThread	*self;
	int			err = 0;

	if ((ha_error = pbms_enter_conn(thd, &self, &ha_result, true)))
		return 1;

	inner_();
	try_(a) {
		if (lock_type == F_UNLCK)
			ha_open_tab->unuse();
		else
			ha_open_tab->use();
	}
	catch_(a) {
		ha_error = pbms_exception_to_result(&self->myException, &ha_result);
		err = 1;
	}
	cont_(a);
	return_(err);
}

THR_LOCK_DATA **ha_pbms::store_lock(THD *, THR_LOCK_DATA **to, enum thr_lock_type lock_type)
{
	if (lock_type != TL_IGNORE && ha_lock.type == TL_UNLOCK)
		ha_lock.type = lock_type;
	*to++ = &ha_lock;
	return to;
}


#ifdef DRIZZLED
int PBMSStorageEngine::doCreateTable(Session&, Table&, TableIdentifier& , drizzled::message::Table& )
{
	/* You cannot create PBMS tables. */
	return( HA_ERR_WRONG_COMMAND );
}

int PBMSStorageEngine::doDropTable(Session &, TableIdentifier& )
{
	/* You cannot delete PBMS tables. */
	return( HA_ERR_WRONG_COMMAND );
}

int PBMSStorageEngine::doRenameTable(Session&, TableIdentifier &, TableIdentifier &)
{
	/* You cannot rename PBMS tables. */
	return( HA_ERR_WRONG_COMMAND );
}

#else // DRIZZLED

int ha_pbms::create(const char *table_name, TABLE *, HA_CREATE_INFO *)
{
	if (PBMSSystemTables::isSystable(cs_last_name_of_path(table_name)))
		return(0);
		
	/* Create only works for system tables. */
	return( HA_ERR_WRONG_COMMAND );
}
#endif // DRIZZLED

bool ha_pbms::get_error_message(int , String *buf)
{
	if (!ha_result.mr_code)
		return false;

	buf->copy(ha_result.mr_message, strlen(ha_result.mr_message), system_charset_info);
	return true;
}


#ifdef XXXXXXX
static int		pbms_port = PBMS_PORT; 
static char		*pbms_repository_threshold;
static char		*pbms_temp_log_threshold;
static char		*pbms_http_metadata_headers;


#ifdef DRIZZLED
#define st_mysql_sys_var drizzled::drizzle_sys_var
#else

struct st_mysql_storage_engine pbms_engine_handler = {
	MYSQL_HANDLERTON_INTERFACE_VERSION
};

struct st_mysql_sys_var
{
  MYSQL_PLUGIN_VAR_HEADER;
};

#endif

#if MYSQL_VERSION_ID < 60000
#if MYSQL_VERSION_ID >= 50124
#define USE_CONST_SAVE
#endif
#else
#if MYSQL_VERSION_ID >= 60005
#define USE_CONST_SAVE
#endif
#endif

#ifdef USE_CONST_SAVE
static void pbms_repository_threshold_func(THD *, struct st_mysql_sys_var *var, void *tgt, const void *save)
#else
static void pbms_repository_threshold_func(THD *, struct st_mysql_sys_var *var, void *tgt, void *save)
#endif
{
	char *old= *(char **) tgt;
	*(char **)tgt= *(char **) save;
	if (var->flags & PLUGIN_VAR_MEMALLOC)
	{
		*(char **)tgt= my_strdup(*(char **) save, MYF(0));
		my_free(old, MYF(0));
	}
	MSDatabase::gRepoThreshold = (uint64_t) cs_byte_size_to_int8(pbms_repository_threshold);
#ifdef DEBUG
	char buffer[200];

	snprintf(buffer, 200, "pbms_repository_threshold=%"PRIu64"\n", MSDatabase::gRepoThreshold);
	CSL.log(NULL, CSLog::Protocol, buffer);
#endif
}

#ifdef USE_CONST_SAVE
static void pbms_temp_log_threshold_func(THD *, struct st_mysql_sys_var *var, void *tgt, const void *save)
#else
static void pbms_temp_log_threshold_func(THD *, struct st_mysql_sys_var *var, void *tgt, void *save)
#endif
{
	char *old= *(char **) tgt;
	*(char **)tgt= *(char **) save;
	if (var->flags & PLUGIN_VAR_MEMALLOC)
	{
		*(char **)tgt= my_strdup(*(char **) save, MYF(0));
		my_free(old, MYF(0));
	}
	MSDatabase::gTempLogThreshold = (uint64_t) cs_byte_size_to_int8(pbms_temp_log_threshold);
#ifdef DEBUG
	char buffer[200];

	snprintf(buffer, 200, "pbms_temp_log_threshold=%"PRIu64"\n",MSDatabase::gTempLogThreshold);
	CSL.log(NULL, CSLog::Protocol, buffer);
#endif
}

#ifdef USE_CONST_SAVE
static void pbms_http_metadata_headers_func(THD *, struct st_mysql_sys_var *var, void *tgt, const void *save)
#else
static void pbms_http_metadata_headers_func(THD *, struct st_mysql_sys_var *var, void *tgt, void *save)
#endif
{
	char *old= *(char **) tgt;
	*(char **)tgt= *(char **) save;
	if (var->flags & PLUGIN_VAR_MEMALLOC)
	{
		*(char **)tgt= my_strdup(*(char **) save, MYF(0));
		my_free(old, MYF(0));
	}
	
	MSHTTPHeaderTable::setDefaultMetaDataHeaders(pbms_http_metadata_headers);
	
#ifdef DEBUG
	char buffer[200];

	snprintf(buffer, 200, "pbms_http_metadata_headers=%s\n", pbms_http_metadata_headers);
	CSL.log(NULL, CSLog::Protocol, buffer);
#endif
}

#ifdef USE_CONST_SAVE
static void pbms_temp_blob_timeout_func(THD *thd, struct st_mysql_sys_var *, void *tgt, const void *save)
#else
static void pbms_temp_blob_timeout_func(THD *thd, struct st_mysql_sys_var *, void *tgt, void *save)
#endif
{
	CSThread		*self;
	PBMSResultRec	result;

	*(long *)tgt= *(long *) save;

	if (pbms_enter_conn(thd, &self, &result, true))
		return;
	MSDatabase::wakeTempLogThreads();
}

#if MYSQL_VERSION_ID >= 50118
static MYSQL_SYSVAR_INT(port, pbms_port,
	PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
	"The port for the server stream-based communications.",
	NULL, NULL, pbms_port, 0, 64*1024, 1);

static MYSQL_SYSVAR_STR(repository_threshold, pbms_repository_threshold,
	PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
	"The maximum size of a BLOB repository file.",
	NULL, /*NULL*/ /**/pbms_repository_threshold_func/**/, MS_REPO_THRESHOLD_DEF);

static MYSQL_SYSVAR_STR(temp_log_threshold, pbms_temp_log_threshold,
	PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
	"The maximum size of a temorary BLOB log file.",
	NULL, /*NULL*/ /**/pbms_temp_log_threshold_func/**/, MS_TEMP_LOG_THRESHOLD_DEF);

static MYSQL_SYSVAR_STR(http_metadata_headers, pbms_http_metadata_headers,
	PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
	"A ':' delimited list of metadata header names to be used to initialize the pbms_metadata_header table when a database is created.",
	NULL, /*NULL*/ /**/pbms_http_metadata_headers_func/**/, MS_HTTP_METADATA_HEADERS_DEF);

static MYSQL_SYSVAR_ULONG(temp_blob_timeout, MSTempLog::gTempBlobTimeout,
	PLUGIN_VAR_OPCMDARG,
	"The timeout, in seconds, for temporary BLOBs. Uploaded blob data is removed after this time, unless committed to the database.",
	NULL, pbms_temp_blob_timeout_func, MS_DEFAULT_TEMP_LOG_WAIT, 1, ~0L, 1);

static MYSQL_SYSVAR_ULONG(garbage_threshold, MSRepository::gGarbageThreshold,
	PLUGIN_VAR_OPCMDARG,
	"The percentage of garbage in a repository file before it is compacted.",
	NULL, NULL, MS_DEFAULT_GARBAGE_LEVEL, 0, 100, 1);


static MYSQL_SYSVAR_ULONG(max_keep_alive, MSConnectionHandler::gMaxKeepAlive,
	PLUGIN_VAR_OPCMDARG,
	"The timeout, in milli-seconds, before the HTTP server will close an inactive HTTP connection.",
	NULL, NULL, MS_DEFAULT_KEEP_ALIVE, 1, UINT32_MAX, 1);

struct st_mysql_sys_var* pbms_system_variables[] = {
	MYSQL_SYSVAR(port),
	MYSQL_SYSVAR(repository_threshold),
	MYSQL_SYSVAR(temp_log_threshold),
	MYSQL_SYSVAR(temp_blob_timeout),
	MYSQL_SYSVAR(garbage_threshold),
	MYSQL_SYSVAR(http_metadata_headers),
	MYSQL_SYSVAR(max_keep_alive),
	NULL
};
#endif


#endif //XXXXX

