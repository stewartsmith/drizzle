/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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
 */

#include <config.h>

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <algorithm>
#include <functional>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <drizzled/cached_directory.h>
#include <drizzled/definitions.h>
#include <drizzled/base.h>
#include <drizzled/cursor.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/data_home.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/xid.h>
#include <drizzled/sql_table.h>
#include <drizzled/charset.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/table_proto.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/table/shell.h>
#include <drizzled/message/cache.h>
#include <drizzled/key.h>
#include <drizzled/session/transactions.h>
#include <drizzled/open_tables_state.h>

#include <boost/algorithm/string/compare.hpp>

static bool shutdown_has_begun= false; // Once we put in the container for the vector/etc for engines this will go away.

namespace drizzled {
namespace plugin {

static EngineVector g_engines;
static EngineVector g_schema_engines;

const std::string DEFAULT_STRING("default");
const std::string UNKNOWN_STRING("UNKNOWN");
const std::string DEFAULT_DEFINITION_FILE_EXT(".dfe");

static std::set<std::string> set_of_table_definition_ext;

EngineVector &StorageEngine::getSchemaEngines()
{
  return g_schema_engines;
}

StorageEngine::StorageEngine(const std::string &name_arg,
                             const std::bitset<HTON_BIT_SIZE> &flags_arg) :
  Plugin(name_arg, "StorageEngine"),
  MonitoredInTransaction(), /* This gives the storage engine a "slot" or ID */
  flags(flags_arg)
{
}

StorageEngine::~StorageEngine()
{
}

void StorageEngine::setTransactionReadWrite(Session& session)
{
  TransactionContext &statement_ctx= session.transaction.stmt;
  statement_ctx.markModifiedNonTransData();
}

int StorageEngine::renameTable(Session &session, const identifier::Table &from, const identifier::Table &to)
{
  setTransactionReadWrite(session);
  if (unlikely(plugin::EventObserver::beforeRenameTable(session, from, to)))
    return ER_EVENT_OBSERVER_PLUGIN;
  int error= doRenameTable(session, from, to);
  if (unlikely(plugin::EventObserver::afterRenameTable(session, from, to, error)))
    error= ER_EVENT_OBSERVER_PLUGIN;
  return error;
}

/**
  Delete all files with extension from bas_ext().

  @param name		Base name of table

  @note
    We assume that the Cursor may return more extensions than
    was actually used for the file.

  @retval
    0   If we successfully deleted at least one file from base_ext and
    didn't get any other errors than ENOENT
  @retval
    !0  Error
*/
int StorageEngine::doDropTable(Session&, const identifier::Table &identifier)
{
  int error= 0;
  int enoent_or_zero= ENOENT;                   // Error if no file was deleted
  char buff[FN_REFLEN];

  for (const char **ext= bas_ext(); *ext ; ext++)
  {
    internal::fn_format(buff, identifier.getPath().c_str(), "", *ext,
                        MY_UNPACK_FILENAME|MY_APPEND_EXT);
    if (internal::my_delete_with_symlink(buff, MYF(0)))
    {
      if ((error= errno) != ENOENT)
        break;
    }
    else
    {
      enoent_or_zero= 0;                        // No error for ENOENT
    }

    error= enoent_or_zero;
  }
  return error;
}

bool StorageEngine::addPlugin(StorageEngine *engine)
{
  g_engines.push_back(engine);

  if (engine->getTableDefinitionFileExtension().length())
  {
    assert(engine->getTableDefinitionFileExtension().length() == DEFAULT_DEFINITION_FILE_EXT.length());
    set_of_table_definition_ext.insert(engine->getTableDefinitionFileExtension());
  }

  if (engine->check_flag(HTON_BIT_SCHEMA_DICTIONARY))
    g_schema_engines.push_back(engine);

  return false;
}

void StorageEngine::removePlugin(StorageEngine *)
{
  if (shutdown_has_begun)
    return;
  shutdown_has_begun= true;
  g_engines.clear();
  g_schema_engines.clear();
}

StorageEngine *StorageEngine::findByName(const std::string &predicate)
{
  BOOST_FOREACH(EngineVector::reference it, g_engines)
  {
    if (not boost::iequals(it->getName(), predicate))
      continue;
    if (it->is_user_selectable())
      return it;
    break;
  }
  return NULL;
}

StorageEngine *StorageEngine::findByName(Session& session, const std::string &predicate)
{
  if (boost::iequals(predicate, DEFAULT_STRING))
    return session.getDefaultStorageEngine();
  return findByName(predicate);
}

/**
  @note
    don't bother to rollback here, it's done already
*/
void StorageEngine::closeConnection(Session& session)
{
  BOOST_FOREACH(EngineVector::reference it, g_engines)
  {
    if (*session.getEngineData(it))
      it->close_connection(&session);
  }
}

bool StorageEngine::flushLogs(StorageEngine *engine)
{
  if (not engine)
  {
    if (std::find_if(g_engines.begin(), g_engines.end(), std::mem_fun(&StorageEngine::flush_logs))
        != g_engines.begin()) // Shouldn't this be .end()?
      return true;
  }
  else if (engine->flush_logs())
    return true;
  return false;
}

class StorageEngineGetTableDefinition: public std::unary_function<StorageEngine *,bool>
{
  Session& session;
  const identifier::Table &identifier;
  message::Table &table_message;
  drizzled::error_t &err;

public:
  StorageEngineGetTableDefinition(Session& session_arg,
                                  const identifier::Table &identifier_arg,
                                  message::Table &table_message_arg,
                                  drizzled::error_t &err_arg) :
    session(session_arg), 
    identifier(identifier_arg),
    table_message(table_message_arg), 
    err(err_arg) {}

  result_type operator() (argument_type engine)
  {
    int ret= engine->doGetTableDefinition(session, identifier, table_message);

    if (ret != ENOENT)
      err= static_cast<drizzled::error_t>(ret);

    return err == static_cast<drizzled::error_t>(EEXIST) or err != static_cast<drizzled::error_t>(ENOENT);
  }
};

/**
  Utility method which hides some of the details of getTableDefinition()
*/
bool plugin::StorageEngine::doesTableExist(Session &session,
                                           const identifier::Table &identifier,
                                           bool include_temporary_tables)
{
  if (include_temporary_tables && session.open_tables.doDoesTableExist(identifier))
      return true;
  BOOST_FOREACH(EngineVector::reference it, g_engines)
  {
    if (it->doDoesTableExist(session, identifier))
      return true;
  }
  return false;
}

bool plugin::StorageEngine::doDoesTableExist(Session&, const drizzled::identifier::Table&)
{
  std::cerr << " Engine was called for doDoesTableExist() and does not implement it: " << this->getName() << "\n";
  assert(0);
  return false;
}

message::table::shared_ptr StorageEngine::getTableMessage(Session& session,
                                                          const identifier::Table& identifier,
                                                          bool include_temporary_tables)
{
  drizzled::error_t error= static_cast<drizzled::error_t>(ENOENT);
  if (include_temporary_tables)
  {
    if (Table *table= session.open_tables.find_temporary_table(identifier))
    {
      return message::table::shared_ptr(new message::Table(*table->getShare()->getTableMessage()));
    }
  }

  drizzled::message::table::shared_ptr table_ptr;
  if ((table_ptr= drizzled::message::Cache::singleton().find(identifier)))
  {
    (void)table_ptr;
  }

  message::Table message;
  EngineVector::iterator iter=
    std::find_if(g_engines.begin(), g_engines.end(),
                 StorageEngineGetTableDefinition(session, identifier, message, error));

  if (iter == g_engines.end())
  {
    return message::table::shared_ptr();
  }
  message::table::shared_ptr table_message(new message::Table(message));

  drizzled::message::Cache::singleton().insert(identifier, table_message);

  return table_message;
}

class DropTableByIdentifier: public std::unary_function<EngineVector::value_type, bool>
{
  Session& session;
  const identifier::Table& identifier;
  drizzled::error_t &error;

public:

  DropTableByIdentifier(Session& session_arg,
                        const identifier::Table& identifier_arg,
                        drizzled::error_t &error_arg) :
    session(session_arg),
    identifier(identifier_arg),
    error(error_arg)
  { }

  result_type operator() (argument_type engine)
  {
    if (not engine->doDoesTableExist(session, identifier))
      return false;

    int local_error= engine->doDropTable(session, identifier);


    if (not local_error)
      return true;

    switch (local_error)
    {
    case HA_ERR_NO_SUCH_TABLE:
    case ENOENT:
      error= static_cast<drizzled::error_t>(HA_ERR_NO_SUCH_TABLE);
      return false;

    default:
      error= static_cast<drizzled::error_t>(local_error);
      return true;
    }
  } 
};


bool StorageEngine::dropTable(Session& session,
                              const identifier::Table& identifier,
                              drizzled::error_t &error)
{
  error= EE_OK;

  EngineVector::const_iterator iter= std::find_if(g_engines.begin(), g_engines.end(),
                                                  DropTableByIdentifier(session, identifier, error));

  if (error)
  {
    return false;
  }
  else if (iter == g_engines.end())
  {
    error= ER_BAD_TABLE_ERROR;
    return false;
  }

  drizzled::message::Cache::singleton().erase(identifier);

  return true;
}

bool StorageEngine::dropTable(Session& session,
                              const identifier::Table &identifier)
{
  drizzled::error_t error;
  return dropTable(session, identifier, error);
}

bool StorageEngine::dropTable(Session& session,
                              StorageEngine &engine,
                              const identifier::Table& identifier,
                              drizzled::error_t &error)
{
  error= EE_OK;
  engine.setTransactionReadWrite(session);

  assert(identifier.isTmp());
  
  if (unlikely(plugin::EventObserver::beforeDropTable(session, identifier)))
  {
    error= ER_EVENT_OBSERVER_PLUGIN;
  }
  else
  {
    error= static_cast<drizzled::error_t>(engine.doDropTable(session, identifier));

    if (unlikely(plugin::EventObserver::afterDropTable(session, identifier, error)))
    {
      error= ER_EVENT_OBSERVER_PLUGIN;
    }
  }
  drizzled::message::Cache::singleton().erase(identifier);
  return not error;
}


/**
  Initiates table-file and calls appropriate database-creator.

  @retval
   0  ok
  @retval
   1  error
*/
bool StorageEngine::createTable(Session &session,
                                const identifier::Table &identifier,
                                message::Table& table_message)
{
  drizzled::error_t error= EE_OK;

  TableShare share(identifier);
  table::Shell table(share);
  message::Table tmp_proto;

  if (share.parse_table_proto(session, table_message) || share.open_table_from_share(&session, identifier, "", 0, 0, table))
  { 
    // @note Error occured, we should probably do a little more here.
    // ER_CORRUPT_TABLE_DEFINITION,ER_CORRUPT_TABLE_DEFINITION_ENUM 
    
    my_error(ER_CORRUPT_TABLE_DEFINITION_UNKNOWN, identifier);

    return false;
  }
  else
  {
    /* Check for legal operations against the Engine using the proto (if used) */
    if (table_message.type() == message::Table::TEMPORARY &&
        share.storage_engine->check_flag(HTON_BIT_TEMPORARY_NOT_SUPPORTED) == true)
    {
      error= HA_ERR_UNSUPPORTED;
    }
    else if (table_message.type() != message::Table::TEMPORARY &&
             share.storage_engine->check_flag(HTON_BIT_TEMPORARY_ONLY) == true)
    {
      error= HA_ERR_UNSUPPORTED;
    }
    else
    {
      share.storage_engine->setTransactionReadWrite(session);

      error= static_cast<drizzled::error_t>(share.storage_engine->doCreateTable(session,
                                                                                table,
                                                                                identifier,
                                                                                table_message));
    }

    if (error == ER_TABLE_PERMISSION_DENIED)
      my_error(ER_TABLE_PERMISSION_DENIED, identifier);
    else if (error)
      my_error(ER_CANT_CREATE_TABLE, MYF(ME_BELL+ME_WAITTANG), identifier.getSQLPath().c_str(), error);
    table.delete_table();
  }
  return(error == EE_OK);
}

Cursor *StorageEngine::getCursor(Table &arg)
{
  return create(arg);
}

void StorageEngine::getIdentifiers(Session &session, const identifier::Schema &schema_identifier, identifier::table::vector &set_of_identifiers)
{
  CachedDirectory directory(schema_identifier.getPath(), set_of_table_definition_ext);

  if (schema_identifier == INFORMATION_SCHEMA_IDENTIFIER)
  { 
	}
  else if (schema_identifier == DATA_DICTIONARY_IDENTIFIER)
  { 
	}
  else if (directory.fail())
  {
    errno= directory.getError();
    if (errno == ENOENT)
      my_error(ER_BAD_DB_ERROR, MYF(ME_BELL+ME_WAITTANG), schema_identifier.getSQLPath().c_str());
    else
      my_error(ER_CANT_READ_DIR, MYF(ME_BELL+ME_WAITTANG), directory.getPath(), errno);
    return;
  }

  BOOST_FOREACH(EngineVector::reference it, g_engines)
    it->doGetTableIdentifiers(directory, schema_identifier, set_of_identifiers);

  session.open_tables.doGetTableIdentifiers(directory, schema_identifier, set_of_identifiers);
}

class DropTable: public std::unary_function<identifier::Table&, bool>
{
  Session &session;
  StorageEngine *engine;

public:

  DropTable(Session &session_arg, StorageEngine *engine_arg) :
    session(session_arg),
    engine(engine_arg)
  { }

  result_type operator() (argument_type identifier)
  {
    return engine->doDropTable(session, identifier) == 0;
  } 
};

/*
  This only works for engines which use file based DFE.

  Note-> Unlike MySQL, we do not, on purpose, delete files that do not match any engines. 
*/
void StorageEngine::removeLostTemporaryTables(Session &session, const char *directory)
{
  CachedDirectory dir(directory, set_of_table_definition_ext);
  identifier::table::vector table_identifiers;

  if (dir.fail())
  {
    errno= dir.getError();
    my_error(ER_CANT_READ_DIR, MYF(0), directory, errno);

    return;
  }

  CachedDirectory::Entries files= dir.getEntries();

  for (CachedDirectory::Entries::iterator fileIter= files.begin();
       fileIter != files.end(); fileIter++)
  {
    size_t length;
    std::string path;
    CachedDirectory::Entry *entry= *fileIter;

    /* We remove the file extension. */
    length= entry->filename.length();
    entry->filename.resize(length - DEFAULT_DEFINITION_FILE_EXT.length());

    path+= directory;
    path+= FN_LIBCHAR;
    path+= entry->filename;
    message::Table definition;
    if (StorageEngine::readTableFile(path, definition))
    {
      identifier::Table identifier(definition.schema(), definition.name(), path);
      table_identifiers.push_back(identifier);
    }
  }

  BOOST_FOREACH(EngineVector::reference it, g_engines)
  {
    table_identifiers.erase(std::remove_if(table_identifiers.begin(), table_identifiers.end(), DropTable(session, it)),
      table_identifiers.end());
  }

  /*
    Now we just clean up anything that might left over.

    We rescan because some of what might have been there should
    now be all nice and cleaned up.
  */
  std::set<std::string> all_exts= set_of_table_definition_ext;

  for (EngineVector::iterator iter= g_engines.begin();
       iter != g_engines.end() ; iter++)
  {
    for (const char **ext= (*iter)->bas_ext(); *ext ; ext++)
      all_exts.insert(*ext);
  }

  CachedDirectory rescan(directory, all_exts);

  files= rescan.getEntries();
  for (CachedDirectory::Entries::iterator fileIter= files.begin();
       fileIter != files.end(); fileIter++)
  {
    std::string path;
    CachedDirectory::Entry *entry= *fileIter;

    path+= directory;
    path+= FN_LIBCHAR;
    path+= entry->filename;

    unlink(path.c_str());
  }
}


/**
  Print error that we got from Cursor function.

  @note
    In case of delete table it's only safe to use the following parts of
    the 'table' structure:
    - table->getShare()->path
    - table->alias
*/
void StorageEngine::print_error(int error, myf errflag, const Table &table) const
{
  drizzled::error_t textno= ER_GET_ERRNO;
  switch (error) {
  case EACCES:
    textno=ER_OPEN_AS_READONLY;
    break;
  case EAGAIN:
    textno=ER_FILE_USED;
    break;
  case ENOENT:
    textno=ER_FILE_NOT_FOUND;
    break;
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_NO_ACTIVE_RECORD:
  case HA_ERR_END_OF_FILE:
    textno=ER_KEY_NOT_FOUND;
    break;
  case HA_ERR_WRONG_MRG_TABLE_DEF:
    textno=ER_WRONG_MRG_TABLE;
    break;
  case HA_ERR_FOUND_DUPP_KEY:
  {
    uint32_t key_nr= table.get_dup_key(error);
    if ((int) key_nr >= 0)
    {
      const char *err_msg= ER(ER_DUP_ENTRY_WITH_KEY_NAME);

      print_keydup_error(key_nr, err_msg, table);

      return;
    }
    textno=ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOREIGN_DUPLICATE_KEY:
  {
    uint32_t key_nr= table.get_dup_key(error);
    if ((int) key_nr >= 0)
    {
      uint32_t max_length;

      /* Write the key in the error message */
      char key[MAX_KEY_LENGTH];
      String str(key,sizeof(key),system_charset_info);

      /* Table is opened and defined at this point */
      key_unpack(&str, &table,(uint32_t) key_nr);
      max_length= (DRIZZLE_ERRMSG_SIZE-
                   (uint32_t) strlen(ER(ER_FOREIGN_DUPLICATE_KEY)));
      if (str.length() >= max_length)
      {
        str.length(max_length-4);
        str.append(STRING_WITH_LEN("..."));
      }
      my_error(ER_FOREIGN_DUPLICATE_KEY, MYF(0), table.getShare()->getTableName(),
        str.c_ptr(), key_nr+1);
      return;
    }
    textno= ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOUND_DUPP_UNIQUE:
    textno=ER_DUP_UNIQUE;
    break;
  case HA_ERR_RECORD_CHANGED:
    textno=ER_CHECKREAD;
    break;
  case HA_ERR_CRASHED:
    textno=ER_NOT_KEYFILE;
    break;
  case HA_ERR_WRONG_IN_RECORD:
    textno= ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_CRASHED_ON_USAGE:
    textno=ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_NOT_A_TABLE:
    textno= static_cast<drizzled::error_t>(error);
    break;
  case HA_ERR_CRASHED_ON_REPAIR:
    textno=ER_CRASHED_ON_REPAIR;
    break;
  case HA_ERR_OUT_OF_MEM:
    textno=ER_OUT_OF_RESOURCES;
    break;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  case HA_ERR_OLD_FILE:
    textno=ER_OLD_KEYFILE;
    break;
  case HA_ERR_UNSUPPORTED:
    textno=ER_UNSUPPORTED_EXTENSION;
    break;
  case HA_ERR_RECORD_FILE_FULL:
  case HA_ERR_INDEX_FILE_FULL:
    textno=ER_RECORD_FILE_FULL;
    break;
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_LOCK_TABLE_FULL:
    textno=ER_LOCK_TABLE_FULL;
    break;
  case HA_ERR_LOCK_DEADLOCK:
    textno=ER_LOCK_DEADLOCK;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  case HA_ERR_CANNOT_ADD_FOREIGN:
    textno=ER_CANNOT_ADD_FOREIGN;
    break;
  case HA_ERR_ROW_IS_REFERENCED:
  {
    String str;
    get_error_message(error, &str);
    my_error(ER_ROW_IS_REFERENCED_2, MYF(0), str.c_ptr_safe());
    return;
  }
  case HA_ERR_NO_REFERENCED_ROW:
  {
    String str;
    get_error_message(error, &str);
    my_error(ER_NO_REFERENCED_ROW_2, MYF(0), str.c_ptr_safe());
    return;
  }
  case HA_ERR_TABLE_DEF_CHANGED:
    textno=ER_TABLE_DEF_CHANGED;
    break;
  case HA_ERR_NO_SUCH_TABLE:
    {
      identifier::Table identifier(table.getShare()->getSchemaName(), table.getShare()->getTableName());
      my_error(ER_TABLE_UNKNOWN, identifier);
      return;
    }
  case HA_ERR_RBR_LOGGING_FAILED:
    textno= ER_BINLOG_ROW_LOGGING_FAILED;
    break;
  case HA_ERR_DROP_INDEX_FK:
  {
    const char *ptr= "???";
    uint32_t key_nr= table.get_dup_key(error);
    if ((int) key_nr >= 0)
      ptr= table.key_info[key_nr].name;
    my_error(ER_DROP_INDEX_FK, MYF(0), ptr);
    return;
  }
  case HA_ERR_TABLE_NEEDS_UPGRADE:
    textno=ER_TABLE_NEEDS_UPGRADE;
    break;
  case HA_ERR_TABLE_READONLY:
    textno= ER_OPEN_AS_READONLY;
    break;
  case HA_ERR_AUTOINC_READ_FAILED:
    textno= ER_AUTOINC_READ_FAILED;
    break;
  case HA_ERR_AUTOINC_ERANGE:
    textno= ER_WARN_DATA_OUT_OF_RANGE;
    break;
  case HA_ERR_LOCK_OR_ACTIVE_TRANSACTION:
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    return;
  default:
    {
      /* 
        The error was "unknown" to this function.
        Ask Cursor if it has got a message for this error 
      */
      bool temporary= false;
      String str;
      temporary= get_error_message(error, &str);
      if (!str.is_empty())
      {
        const char* engine_name= getName().c_str();
        if (temporary)
          my_error(ER_GET_TEMPORARY_ERRMSG, MYF(0), error, str.ptr(),
                   engine_name);
        else
          my_error(ER_GET_ERRMSG, MYF(0), error, str.ptr(), engine_name);
      }
      else
      {
	      my_error(ER_GET_ERRNO,errflag,error);
      }
      return;
    }
  }

  my_error(textno, errflag, table.getShare()->getTableName(), error);
}


/**
  Return an error message specific to this Cursor.

  @param error  error code previously returned by Cursor
  @param buf    pointer to String where to add error message

  @return
    Returns true if this is a temporary error
*/
bool StorageEngine::get_error_message(int , String* ) const
{
  return false;
}


void StorageEngine::print_keydup_error(uint32_t key_nr, const char *msg, const Table &table) const
{
  /* Write the duplicated key in the error message */
  char key[MAX_KEY_LENGTH];
  String str(key,sizeof(key),system_charset_info);

  if (key_nr == MAX_KEY)
  {
    /* Key is unknown */
    str.copy("", 0, system_charset_info);
    my_printf_error(ER_DUP_ENTRY, msg, MYF(0), str.c_ptr(), "*UNKNOWN*");
  }
  else
  {
    /* Table is opened and defined at this point */
    key_unpack(&str, &table, (uint32_t) key_nr);
    uint32_t max_length=DRIZZLE_ERRMSG_SIZE-(uint32_t) strlen(msg);
    if (str.length() >= max_length)
    {
      str.length(max_length-4);
      str.append(STRING_WITH_LEN("..."));
    }
    my_printf_error(ER_DUP_ENTRY, msg,
		    MYF(0), str.c_ptr(), table.key_info[key_nr].name);
  }
}


int StorageEngine::deleteDefinitionFromPath(const identifier::Table &identifier)
{
  std::string path(identifier.getPath());

  path.append(DEFAULT_DEFINITION_FILE_EXT);

  return internal::my_delete(path.c_str(), MYF(0));
}

int StorageEngine::renameDefinitionFromPath(const identifier::Table &dest, const identifier::Table &src)
{
  message::Table table_message;
  std::string src_path(src.getPath());
  std::string dest_path(dest.getPath());

  src_path.append(DEFAULT_DEFINITION_FILE_EXT);
  dest_path.append(DEFAULT_DEFINITION_FILE_EXT);

  bool was_read= StorageEngine::readTableFile(src_path.c_str(), table_message);

  if (not was_read)
  {
    return ENOENT;
  }

  dest.copyToTableMessage(table_message);

  int error= StorageEngine::writeDefinitionFromPath(dest, table_message);

  if (not error)
  {
    if (unlink(src_path.c_str()))
      perror(src_path.c_str());
  }

  return error;
}

int StorageEngine::writeDefinitionFromPath(const identifier::Table &identifier, const message::Table &table_message)
{
  char definition_file_tmp[FN_REFLEN];
  std::string file_name(identifier.getPath());

  file_name.append(DEFAULT_DEFINITION_FILE_EXT);

  snprintf(definition_file_tmp, sizeof(definition_file_tmp), "%sXXXXXX", file_name.c_str());

  int fd= mkstemp(definition_file_tmp);

  if (fd == -1)
  {
    perror(definition_file_tmp);
    return errno;
  }

  google::protobuf::io::ZeroCopyOutputStream* output=
    new google::protobuf::io::FileOutputStream(fd);

  bool success;

  try
  {
    success= table_message.SerializeToZeroCopyStream(output);
  }
  catch (...)
  {
    success= false;
  }

  if (not success)
  {
    my_error(ER_CORRUPT_TABLE_DEFINITION, MYF(0), identifier.getSQLPath().c_str(), table_message.InitializationErrorString().c_str());
    delete output;

    if (close(fd) == -1)
      perror(definition_file_tmp);

    if (unlink(definition_file_tmp) == -1)
      perror(definition_file_tmp);

    return ER_CORRUPT_TABLE_DEFINITION;
  }

  delete output;

  if (close(fd) == -1)
  {
    int error= errno;
    perror(definition_file_tmp);

    if (unlink(definition_file_tmp))
      perror(definition_file_tmp);

    return error;
  }

  if (rename(definition_file_tmp, file_name.c_str()) == -1)
  {
    int error= errno;
    perror(definition_file_tmp);

    if (unlink(definition_file_tmp))
      perror(definition_file_tmp);

    return error;
  }

  return 0;
}

/**
  @note on success table can be created.
*/
bool StorageEngine::canCreateTable(const identifier::Table &identifier)
{
  BOOST_FOREACH(EngineVector::reference it, g_engines)
  {
    if (not it->doCanCreateTable(identifier))
      return false;
  }
  return true;
}

bool StorageEngine::readTableFile(const std::string &path, message::Table &table_message)
{
  std::fstream input(path.c_str(), std::ios::in | std::ios::binary);

  if (input.good())
  {
    try {
      if (table_message.ParseFromIstream(&input))
      {
        return true;
      }
    }
    catch (...)
    {
      my_error(ER_CORRUPT_TABLE_DEFINITION, MYF(0),
               table_message.name().empty() ? path.c_str() : table_message.name().c_str(),
               table_message.InitializationErrorString().empty() ? "": table_message.InitializationErrorString().c_str());
    }
  }
  else
  {
    perror(path.c_str());
  }

  return false;
}

std::ostream& operator<<(std::ostream& output, const StorageEngine &engine)
{
  return output << "StorageEngine:(" <<  engine.getName() << ")";
}

} /* namespace plugin */
} /* namespace drizzled */
