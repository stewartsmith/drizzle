/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_PLUGIN_STORAGE_ENGINE_H
#define DRIZZLED_PLUGIN_STORAGE_ENGINE_H


#include <drizzled/definitions.h>
#include <drizzled/plugin.h>
#include <drizzled/handler_structs.h>
#include <drizzled/message.h>
#include <drizzled/message/cache.h>
#include "drizzled/plugin/plugin.h"
#include "drizzled/sql_string.h"
#include "drizzled/identifier.h"
#include "drizzled/cached_directory.h"
#include "drizzled/plugin/monitored_in_transaction.h"

#include <bitset>
#include <string>
#include <vector>
#include <set>

namespace drizzled
{

class TableList;
class Session;
class Cursor;
typedef struct st_hash HASH;

class TableShare;
typedef drizzle_lex_string LEX_STRING;
typedef bool (stat_print_fn)(Session *session, const char *type, uint32_t type_len,
                             const char *file, uint32_t file_len,
                             const char *status, uint32_t status_len);

/* Possible flags of a StorageEngine (there can be 32 of them) */
enum engine_flag_bits {
  HTON_BIT_ALTER_NOT_SUPPORTED,       // Engine does not support alter
  HTON_BIT_HIDDEN,                    // Engine does not appear in lists
  HTON_BIT_NOT_USER_SELECTABLE,
  HTON_BIT_TEMPORARY_NOT_SUPPORTED,   // Having temporary tables not supported
  HTON_BIT_TEMPORARY_ONLY,
  HTON_BIT_DOES_TRANSACTIONS,
  HTON_BIT_STATS_RECORDS_IS_EXACT,
  HTON_BIT_NULL_IN_KEY,
  HTON_BIT_CAN_INDEX_BLOBS,
  HTON_BIT_PRIMARY_KEY_IN_READ_INDEX,
  HTON_BIT_PARTIAL_COLUMN_READ,
  HTON_BIT_TABLE_SCAN_ON_INDEX,
  HTON_BIT_FAST_KEY_READ,
  HTON_BIT_NO_BLOBS,
  HTON_BIT_HAS_RECORDS,
  HTON_BIT_NO_AUTO_INCREMENT,
  HTON_BIT_DUPLICATE_POS,
  HTON_BIT_AUTO_PART_KEY,
  HTON_BIT_REQUIRE_PRIMARY_KEY,
  HTON_BIT_REQUIRES_KEY_COLUMNS_FOR_DELETE,
  HTON_BIT_PRIMARY_KEY_REQUIRED_FOR_DELETE,
  HTON_BIT_NO_PREFIX_CHAR_KEYS,
  HTON_BIT_HAS_CHECKSUM,
  HTON_BIT_SKIP_STORE_LOCK,
  HTON_BIT_SCHEMA_DICTIONARY,
  HTON_BIT_FOREIGN_KEYS,
  HTON_BIT_SIZE
};

static const std::bitset<HTON_BIT_SIZE> HTON_NO_FLAGS(0);
static const std::bitset<HTON_BIT_SIZE> HTON_ALTER_NOT_SUPPORTED(1 << HTON_BIT_ALTER_NOT_SUPPORTED);
static const std::bitset<HTON_BIT_SIZE> HTON_HIDDEN(1 << HTON_BIT_HIDDEN);
static const std::bitset<HTON_BIT_SIZE> HTON_NOT_USER_SELECTABLE(1 << HTON_BIT_NOT_USER_SELECTABLE);
static const std::bitset<HTON_BIT_SIZE> HTON_TEMPORARY_NOT_SUPPORTED(1 << HTON_BIT_TEMPORARY_NOT_SUPPORTED);
static const std::bitset<HTON_BIT_SIZE> HTON_TEMPORARY_ONLY(1 << HTON_BIT_TEMPORARY_ONLY);
static const std::bitset<HTON_BIT_SIZE> HTON_HAS_DOES_TRANSACTIONS(1 << HTON_BIT_DOES_TRANSACTIONS);
static const std::bitset<HTON_BIT_SIZE> HTON_STATS_RECORDS_IS_EXACT(1 << HTON_BIT_STATS_RECORDS_IS_EXACT);
static const std::bitset<HTON_BIT_SIZE> HTON_NULL_IN_KEY(1 << HTON_BIT_NULL_IN_KEY);
static const std::bitset<HTON_BIT_SIZE> HTON_CAN_INDEX_BLOBS(1 << HTON_BIT_CAN_INDEX_BLOBS);
static const std::bitset<HTON_BIT_SIZE> HTON_PRIMARY_KEY_IN_READ_INDEX(1 << HTON_BIT_PRIMARY_KEY_IN_READ_INDEX);
static const std::bitset<HTON_BIT_SIZE> HTON_PARTIAL_COLUMN_READ(1 << HTON_BIT_PARTIAL_COLUMN_READ);
static const std::bitset<HTON_BIT_SIZE> HTON_TABLE_SCAN_ON_INDEX(1 << HTON_BIT_TABLE_SCAN_ON_INDEX);
static const std::bitset<HTON_BIT_SIZE> HTON_FAST_KEY_READ(1 << HTON_BIT_FAST_KEY_READ);
static const std::bitset<HTON_BIT_SIZE> HTON_NO_BLOBS(1 << HTON_BIT_NO_BLOBS);
static const std::bitset<HTON_BIT_SIZE> HTON_HAS_RECORDS(1 << HTON_BIT_HAS_RECORDS);
static const std::bitset<HTON_BIT_SIZE> HTON_NO_AUTO_INCREMENT(1 << HTON_BIT_NO_AUTO_INCREMENT);
static const std::bitset<HTON_BIT_SIZE> HTON_DUPLICATE_POS(1 << HTON_BIT_DUPLICATE_POS);
static const std::bitset<HTON_BIT_SIZE> HTON_AUTO_PART_KEY(1 << HTON_BIT_AUTO_PART_KEY);
static const std::bitset<HTON_BIT_SIZE> HTON_REQUIRE_PRIMARY_KEY(1 << HTON_BIT_REQUIRE_PRIMARY_KEY);
static const std::bitset<HTON_BIT_SIZE> HTON_REQUIRES_KEY_COLUMNS_FOR_DELETE(1 << HTON_BIT_REQUIRES_KEY_COLUMNS_FOR_DELETE);
static const std::bitset<HTON_BIT_SIZE> HTON_PRIMARY_KEY_REQUIRED_FOR_DELETE(1 << HTON_BIT_PRIMARY_KEY_REQUIRED_FOR_DELETE);
static const std::bitset<HTON_BIT_SIZE> HTON_NO_PREFIX_CHAR_KEYS(1 << HTON_BIT_NO_PREFIX_CHAR_KEYS);
static const std::bitset<HTON_BIT_SIZE> HTON_HAS_CHECKSUM(1 << HTON_BIT_HAS_CHECKSUM);
static const std::bitset<HTON_BIT_SIZE> HTON_SKIP_STORE_LOCK(1 << HTON_BIT_SKIP_STORE_LOCK);
static const std::bitset<HTON_BIT_SIZE> HTON_HAS_SCHEMA_DICTIONARY(1 << HTON_BIT_SCHEMA_DICTIONARY);
static const std::bitset<HTON_BIT_SIZE> HTON_HAS_FOREIGN_KEYS(1 << HTON_BIT_FOREIGN_KEYS);


class Table;
class NamedSavepoint;

namespace plugin
{

typedef std::vector<StorageEngine *> EngineVector;

typedef std::set<std::string> TableNameList;

extern const std::string UNKNOWN_STRING;
extern const std::string DEFAULT_DEFINITION_FILE_EXT;


/*
  StorageEngine is a singleton structure - one instance per storage engine -
  to provide access to storage engine functionality that works on the
  "global" level (unlike Cursor class that works on a per-table basis)

  usually StorageEngine instance is defined statically in ha_xxx.cc as

  static StorageEngine { ... } xxx_engine;
*/
class StorageEngine : public Plugin,
                      public MonitoredInTransaction
{
  friend class SEAPITester;
public:
  typedef uint64_t Table_flags;

private:
  const std::bitset<HTON_BIT_SIZE> flags; /* global Cursor flags */

  static EngineVector &getSchemaEngines();

  virtual void setTransactionReadWrite(Session& session);

  /*
   * Indicates to a storage engine the start of a
   * new SQL statement.
   */
  virtual void doStartStatement(Session *session)
  {
    (void) session;
  }

  /*
   * Indicates to a storage engine the end of
   * the current SQL statement in the supplied
   * Session.
   */
  virtual void doEndStatement(Session *session)
  {
    (void) session;
  }

protected:
  std::string table_definition_ext;

public:
  const std::string& getTableDefinitionFileExtension()
  {
    return table_definition_ext;
  }

private:
  std::vector<std::string> aliases;

public:
  const std::vector<std::string>& getAliases() const
  {
    return aliases;
  }

  void addAlias(std::string alias)
  {
    aliases.push_back(alias);
  }

protected:

  /**
    @brief
    Used as a protobuf storage currently by TEMP only engines.
  */
  typedef std::map <std::string, message::Table> ProtoCache;
  ProtoCache proto_cache;
  pthread_mutex_t proto_cache_mutex;

public:

  StorageEngine(const std::string name_arg,
                const std::bitset<HTON_BIT_SIZE> &flags_arg= HTON_NO_FLAGS);

  virtual ~StorageEngine();

  virtual int doGetTableDefinition(Session &session,
                                   const drizzled::TableIdentifier &identifier,
                                   message::Table &table_message)
  {
    (void)session;
    (void)identifier;
    (void)table_message;

    return ENOENT;
  }

  /* Old style cursor errors */
protected:
  void print_keydup_error(uint32_t key_nr, const char *msg, Table &table);
  void print_error(int error, myf errflag, Table *table= NULL);
  virtual bool get_error_message(int error, String *buf);
public:
  virtual void print_error(int error, myf errflag, Table& table);

  bool is_user_selectable() const
  {
    return not flags.test(HTON_BIT_NOT_USER_SELECTABLE);
  }

  bool check_flag(const engine_flag_bits flag) const
  {
    return flags.test(flag);
  }

  // @todo match check_flag interface
  virtual uint32_t index_flags(enum  ha_key_alg) const { return 0; }
  virtual void startStatement(Session *session)
  {
    doStartStatement(session);
  }
  virtual void endStatement(Session *session)
  {
    doEndStatement(session);
  }

  /*
   * Called during Session::cleanup() for all engines
   */
  virtual int close_connection(Session  *)
  {
    return 0;
  }
  virtual Cursor *create(Table &)= 0;
  /* args: path */
  virtual bool flush_logs() { return false; }
  virtual bool show_status(Session *, stat_print_fn *, enum ha_stat_type)
  {
    return false;
  }

  /**
    If frm_error() is called then we will use this to find out what file
    extentions exist for the storage engine. This is also used by the default
    rename_table and delete_table method in Cursor.cc.

    For engines that have two file name extentions (separate meta/index file
    and data file), the order of elements is relevant. First element of engine
    file name extentions array should be meta/index file extention. Second
    element - data file extention. This order is assumed by
    prepare_for_repair() when REPAIR Table ... USE_FRM is issued.
  */
  virtual const char **bas_ext() const =0;

protected:
  virtual int doCreateTable(Session &session,
                            Table &table_arg,
                            const drizzled::TableIdentifier &identifier,
                            message::Table &message)= 0;

  virtual int doRenameTable(Session &session,
                            const drizzled::TableIdentifier &from, const drizzled::TableIdentifier &to)= 0;

public:

  int renameTable(Session &session, const drizzled::TableIdentifier &from, const drizzled::TableIdentifier &to);

  virtual void doGetTableIdentifiers(CachedDirectory &directory,
                                     const drizzled::SchemaIdentifier &schema_identifier,
                                     TableIdentifier::vector &set_of_identifiers)= 0;

  virtual int doDropTable(Session &session,
                          const drizzled::TableIdentifier &identifier)= 0;

  /* Class Methods for operating on plugin */
  static bool addPlugin(plugin::StorageEngine *engine);
  static void removePlugin(plugin::StorageEngine *engine);

  static int getTableDefinition(Session& session,
                                const drizzled::TableIdentifier &identifier,
                                message::table::shared_ptr &table_proto,
                                bool include_temporary_tables= true);
  static bool doesTableExist(Session &session,
                             const drizzled::TableIdentifier &identifier,
                             bool include_temporary_tables= true);

  virtual bool doDoesTableExist(Session& session, const drizzled::TableIdentifier &identifier);

  static plugin::StorageEngine *findByName(const std::string &find_str);
  static plugin::StorageEngine *findByName(Session& session, const std::string &find_str);

  static void closeConnection(Session* session);
  static void dropDatabase(char* path);
  static bool flushLogs(plugin::StorageEngine *db_type);
  static int dropTable(Session& session,
                       const drizzled::TableIdentifier &identifier);
  static int dropTable(Session& session,
                       StorageEngine &engine,
                       const drizzled::TableIdentifier &identifier);
  static void getIdentifiers(Session &session,
                             const SchemaIdentifier &schema_identifier,
                             TableIdentifier::vector &set_of_identifiers);

  // Check to see if any SE objects to creation.
  static bool canCreateTable(const drizzled::TableIdentifier &identifier);
  virtual bool doCanCreateTable(const drizzled::TableIdentifier &identifier)
  { (void)identifier;  return true; }

  // @note All schema methods defined here
  static void getIdentifiers(Session &session, SchemaIdentifier::vector &schemas);
  static bool getSchemaDefinition(const drizzled::TableIdentifier &identifier, message::schema::shared_ptr &proto);
  static bool getSchemaDefinition(const drizzled::SchemaIdentifier &identifier, message::schema::shared_ptr &proto);
  static bool doesSchemaExist(const drizzled::SchemaIdentifier &identifier);
  static const CHARSET_INFO *getSchemaCollation(const drizzled::SchemaIdentifier &identifier);
  static bool createSchema(const drizzled::message::Schema &schema_message);
  static bool dropSchema(const drizzled::SchemaIdentifier &identifier);
  static bool alterSchema(const drizzled::message::Schema &schema_message);

  // @note make private/protected
  virtual void doGetSchemaIdentifiers(SchemaIdentifier::vector&)
  { }

  virtual bool doGetSchemaDefinition(const drizzled::SchemaIdentifier&, drizzled::message::schema::shared_ptr&)
  { 
    return false; 
  }

  virtual bool doCreateSchema(const drizzled::message::Schema&)
  { return false; }

  virtual bool doAlterSchema(const drizzled::message::Schema&)
  { return false; }

  virtual bool doDropSchema(const drizzled::SchemaIdentifier&)
  { return false; }

  static inline const std::string &resolveName(const StorageEngine *engine)
  {
    return engine == NULL ? UNKNOWN_STRING : engine->getName();
  }

  static int createTable(Session& session,
                         const drizzled::TableIdentifier &identifier,
                         message::Table& table_proto);

  static void removeLostTemporaryTables(Session &session, const char *directory);

  Cursor *getCursor(Table &share);

  uint32_t max_record_length() const
  { return std::min((unsigned int)HA_MAX_REC_LENGTH, max_supported_record_length()); }
  uint32_t max_keys() const
  { return std::min((unsigned int)MAX_KEY, max_supported_keys()); }
  uint32_t max_key_parts() const
  { return std::min((unsigned int)MAX_REF_PARTS, max_supported_key_parts()); }
  uint32_t max_key_length() const
  { return std::min((unsigned int)MAX_KEY_LENGTH, max_supported_key_length()); }
  uint32_t max_key_part_length(void) const
  { return std::min((unsigned int)MAX_KEY_LENGTH, max_supported_key_part_length()); }

  virtual uint32_t max_supported_record_length(void) const
  { return HA_MAX_REC_LENGTH; }
  virtual uint32_t max_supported_keys(void) const { return 0; }
  virtual uint32_t max_supported_key_parts(void) const { return MAX_REF_PARTS; }
  virtual uint32_t max_supported_key_length(void) const { return MAX_KEY_LENGTH; }
  virtual uint32_t max_supported_key_part_length(void) const { return 255; }

  /* TODO-> Make private */
  static int deleteDefinitionFromPath(const drizzled::TableIdentifier &identifier);
  static int renameDefinitionFromPath(const drizzled::TableIdentifier &dest, const drizzled::TableIdentifier &src);
  static int writeDefinitionFromPath(const drizzled::TableIdentifier &identifier, message::Table &proto);
  static bool readTableFile(const std::string &path, message::Table &table_message);

public:
  /* 
   * The below are simple virtual overrides for the plugin::MonitoredInTransaction
   * interface.
   */
  virtual bool participatesInSqlTransaction() const
  {
    return false; /* plugin::StorageEngine is non-transactional in terms of SQL */
  }
  virtual bool participatesInXaTransaction() const
  {
    return false; /* plugin::StorageEngine is non-transactional in terms of XA */
  }
  virtual bool alwaysRegisterForXaTransaction() const
  {
    return false;
  }

  virtual bool validateCreateTableOption(const std::string &key, const std::string &state)
  {
    (void)key;
    (void)state;

    return false;
  }

  virtual bool validateCreateSchemaOption(const std::string &key, const std::string &state)
  {
    (void)key;
    (void)state;

    return false;
  }
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_STORAGE_ENGINE_H */
