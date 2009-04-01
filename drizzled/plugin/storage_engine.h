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
#include <drizzled/sql_plugin.h>

#include <bitset>
#include <string>

class TableList;
class Session;
class XID;
class handler;

class TABLE_SHARE;
typedef struct st_mysql_lex_string LEX_STRING;
typedef bool (stat_print_fn)(Session *session, const char *type, uint32_t type_len,
                             const char *file, uint32_t file_len,
                             const char *status, uint32_t status_len);
enum ha_stat_type { HA_ENGINE_STATUS, HA_ENGINE_LOGS, HA_ENGINE_MUTEX };

/* Possible flags of a StorageEngine (there can be 32 of them) */
enum engine_flag_bits {
  HTON_BIT_CLOSE_CURSORS_AT_COMMIT,
  HTON_BIT_ALTER_NOT_SUPPORTED,       // Engine does not support alter
  HTON_BIT_CAN_RECREATE,              // Delete all is used for truncate
  HTON_BIT_HIDDEN,                    // Engine does not appear in lists
  HTON_BIT_FLUSH_AFTER_RENAME,
  HTON_BIT_NOT_USER_SELECTABLE,
  HTON_BIT_TEMPORARY_NOT_SUPPORTED,   // Having temporary tables not supported
  HTON_BIT_SUPPORT_LOG_TABLES,        // Engine supports log tables
  HTON_BIT_NO_PARTITION,              // You can not partition these tables
  HTON_BIT_SIZE
};

static const std::bitset<HTON_BIT_SIZE> HTON_NO_FLAGS(0);
static const std::bitset<HTON_BIT_SIZE> HTON_CLOSE_CURSORS_AT_COMMIT(1 <<  HTON_BIT_CLOSE_CURSORS_AT_COMMIT);
static const std::bitset<HTON_BIT_SIZE> HTON_ALTER_NOT_SUPPORTED(1 << HTON_BIT_ALTER_NOT_SUPPORTED);
static const std::bitset<HTON_BIT_SIZE> HTON_CAN_RECREATE(1 << HTON_BIT_CAN_RECREATE);
static const std::bitset<HTON_BIT_SIZE> HTON_HIDDEN(1 << HTON_BIT_HIDDEN);
static const std::bitset<HTON_BIT_SIZE> HTON_FLUSH_AFTER_RENAME(1 << HTON_BIT_FLUSH_AFTER_RENAME);
static const std::bitset<HTON_BIT_SIZE> HTON_NOT_USER_SELECTABLE(1 << HTON_BIT_NOT_USER_SELECTABLE);
static const std::bitset<HTON_BIT_SIZE> HTON_TEMPORARY_NOT_SUPPORTED(1 << HTON_BIT_TEMPORARY_NOT_SUPPORTED);
static const std::bitset<HTON_BIT_SIZE> HTON_SUPPORT_LOG_TABLES(1 << HTON_BIT_SUPPORT_LOG_TABLES);
static const std::bitset<HTON_BIT_SIZE> HTON_NO_PARTITION(1 << HTON_BIT_NO_PARTITION);

/*
  StorageEngine is a singleton structure - one instance per storage engine -
  to provide access to storage engine functionality that works on the
  "global" level (unlike handler class that works on a per-table basis)

  usually StorageEngine instance is defined statically in ha_xxx.cc as

  static StorageEngine { ... } xxx_engine;

  savepoint_*, prepare, recover, and *_by_xid pointers can be 0.
*/
class StorageEngine
{
  /*
    Name used for storage engine.
  */
  const std::string name;
  const bool two_phase_commit;
  bool enabled;
  const std::bitset<HTON_BIT_SIZE> flags; /* global handler flags */
  /*
    to store per-savepoint data storage engine is provided with an area
    of a requested size (0 is ok here).
    savepoint_offset must be initialized statically to the size of
    the needed memory to store per-savepoint information.
    After xxx_init it is changed to be an offset to savepoint storage
    area and need not be used by storage engine.
    see binlog_engine and binlog_savepoint_set/rollback for an example.
  */
  size_t savepoint_offset;
  size_t orig_savepoint_offset;

protected:

  /**
   * Implementing classes should override these to provide savepoint
   * functionality.
   */
  virtual int savepoint_set_hook(Session *, void *) { return 0; }

  virtual int savepoint_rollback_hook(Session *, void *) { return 0; }

  virtual int savepoint_release_hook(Session *, void *) { return 0; }

public:

  /*
    each storage engine has it's own memory area (actually a pointer)
    in the session, for storing per-connection information.
    It is accessed as

      session->ha_data[xxx_engine.slot]

   slot number is initialized by MySQL after xxx_init() is called.
  */
  uint32_t slot;

  StorageEngine(const std::string &name_arg,
                const std::bitset<HTON_BIT_SIZE> &flags_arg= HTON_NO_FLAGS,
                size_t savepoint_offset_arg= 0,
                bool support_2pc= false);

  virtual ~StorageEngine();

  bool has_2pc()
  {
    return two_phase_commit;
  }


  bool is_enabled() const
  {
    return enabled;
  }

  bool is_user_selectable() const
  {
    return not flags.test(HTON_BIT_NOT_USER_SELECTABLE);
  }

  bool check_flag(const engine_flag_bits flag) const
  {
    return flags.test(flag);
  }

  void enable() { enabled= true; }
  void disable() { enabled= false; }

  std::string get_name() { return name; }

  /*
    StorageEngine methods:

    close_connection is only called if
    session->ha_data[xxx_engine.slot] is non-zero, so even if you don't need
    this storage area - set it to something, so that MySQL would know
    this storage engine was accessed in this connection
  */
  virtual int close_connection(Session  *)
  {
    return 0;
  }
  /*
    'all' is true if it's a real commit, that makes persistent changes
    'all' is false if it's not in fact a commit but an end of the
    statement that is part of the transaction.
    NOTE 'all' is also false in auto-commit mode where 'end of statement'
    and 'real commit' mean the same event.
  */
  virtual int  commit(Session *, bool)
  {
    return 0;
  }

  virtual int  rollback(Session *, bool)
  {
    return 0;
  }

  /*
    The void * points to an uninitialized storage area of requested size
    (see savepoint_offset description)
  */
  int savepoint_set(Session *session, void *sp)
  {
    return savepoint_set_hook(session, (unsigned char *)sp+savepoint_offset);
  }

  /*
    The void * points to a storage area, that was earlier passed
    to the savepoint_set call
  */
  int savepoint_rollback(Session *session, void *sp)
  {
     return savepoint_rollback_hook(session,
                                    (unsigned char *)sp+savepoint_offset);
  }

  int savepoint_release(Session *session, void *sp)
  {
    return savepoint_release_hook(session,
                                  (unsigned char *)sp+savepoint_offset);
  }

  virtual int  prepare(Session *, bool) { return 0; }
  virtual int  recover(XID *, uint32_t) { return 0; }
  virtual int  commit_by_xid(XID *) { return 0; }
  virtual int  rollback_by_xid(XID *) { return 0; }
  virtual handler *create(TABLE_SHARE *, MEM_ROOT *)= 0;
  /* args: path */
  virtual void drop_database(char*) { }
  virtual int start_consistent_snapshot(Session *) { return 0; }
  virtual bool flush_logs() { return false; }
  virtual bool show_status(Session *, stat_print_fn *, enum ha_stat_type)
  {
    return false;
  }

  /* args: current_session, tables, cond */
  virtual int fill_files_table(Session *, TableList *,
                               Item *) { return 0; }
  virtual int release_temporary_latches(Session *) { return false; }

  /* args: current_session, db, name */
  virtual int table_exists_in_engine(Session*, const char *, const char *);
};

/* lookups */
StorageEngine *ha_default_storage_engine(Session *session);
plugin_ref ha_resolve_by_name(Session *session, const LEX_STRING *name);
plugin_ref ha_lock_engine(Session *session, StorageEngine *engine);
handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         StorageEngine *db_type);
const char *ha_resolve_storage_engine_name(const StorageEngine *db_type);

#endif /* DRIZZLED_PLUGIN_STORAGE_ENGINE_H */
