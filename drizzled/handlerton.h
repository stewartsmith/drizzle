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

#ifndef DRIZZLED_HANDLERTON_H
#define DRIZZLED_HANDLERTON_H

#include <stdint.h>
#include <bitset>

#include <drizzled/definitions.h>
#include <drizzled/sql_plugin.h>

class TableList;
class Session;
class XID;
class handler;

typedef struct st_mysql_lex_string LEX_STRING;
typedef struct st_table_share TABLE_SHARE;
typedef bool (stat_print_fn)(Session *session, const char *type, uint32_t type_len,
                             const char *file, uint32_t file_len,
                             const char *status, uint32_t status_len);
enum ha_stat_type { HA_ENGINE_STATUS, HA_ENGINE_LOGS, HA_ENGINE_MUTEX };

/* Possible flags of a handlerton (there can be 32 of them) */
enum hton_flag_bits {
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
  handlerton is a singleton structure - one instance per storage engine -
  to provide access to storage engine functionality that works on the
  "global" level (unlike handler class that works on a per-table basis)

  usually handlerton instance is defined statically in ha_xxx.cc as

  static handlerton { ... } xxx_hton;

  savepoint_*, prepare, recover, and *_by_xid pointers can be 0.
*/
struct handlerton
{
  /*
    Name used for storage engine.
  */
  const char *name;

  /*
    Historical marker for if the engine is available of not
  */
  SHOW_COMP_OPTION state;

  /*
    Historical number used for frm file to determine the correct storage engine.
    This is going away and new engines will just use "name" for this.
  */
  enum legacy_db_type db_type;
  /*
    each storage engine has it's own memory area (actually a pointer)
    in the session, for storing per-connection information.
    It is accessed as

      session->ha_data[xxx_hton.slot]

   slot number is initialized by MySQL after xxx_init() is called.
   */
   uint32_t slot;
   /*
     to store per-savepoint data storage engine is provided with an area
     of a requested size (0 is ok here).
     savepoint_offset must be initialized statically to the size of
     the needed memory to store per-savepoint information.
     After xxx_init it is changed to be an offset to savepoint storage
     area and need not be used by storage engine.
     see binlog_hton and binlog_savepoint_set/rollback for an example.
   */
   uint32_t savepoint_offset;
   /*
     handlerton methods:

     close_connection is only called if
     session->ha_data[xxx_hton.slot] is non-zero, so even if you don't need
     this storage area - set it to something, so that MySQL would know
     this storage engine was accessed in this connection
   */
   int  (*close_connection)(handlerton *hton, Session *session);
   /*
     sv points to an uninitialized storage area of requested size
     (see savepoint_offset description)
   */
   int  (*savepoint_set)(handlerton *hton, Session *session, void *sv);
   /*
     sv points to a storage area, that was earlier passed
     to the savepoint_set call
   */
   int  (*savepoint_rollback)(handlerton *hton, Session *session, void *sv);
   int  (*savepoint_release)(handlerton *hton, Session *session, void *sv);
   /*
     'all' is true if it's a real commit, that makes persistent changes
     'all' is false if it's not in fact a commit but an end of the
     statement that is part of the transaction.
     NOTE 'all' is also false in auto-commit mode where 'end of statement'
     and 'real commit' mean the same event.
   */
   int  (*commit)(handlerton *hton, Session *session, bool all);
   int  (*rollback)(handlerton *hton, Session *session, bool all);
   int  (*prepare)(handlerton *hton, Session *session, bool all);
   int  (*recover)(handlerton *hton, XID *xid_list, uint32_t len);
   int  (*commit_by_xid)(handlerton *hton, XID *xid);
   int  (*rollback_by_xid)(handlerton *hton, XID *xid);
   void *(*create_cursor_read_view)(handlerton *hton, Session *session);
   void (*set_cursor_read_view)(handlerton *hton, Session *session, void *read_view);
   void (*close_cursor_read_view)(handlerton *hton, Session *session, void *read_view);
   handler *(*create)(handlerton *hton, TABLE_SHARE *table, MEM_ROOT *mem_root);
   void (*drop_database)(handlerton *hton, char* path);
   int (*start_consistent_snapshot)(handlerton *hton, Session *session);
   bool (*flush_logs)(handlerton *hton);
   bool (*show_status)(handlerton *hton, Session *session, stat_print_fn *print, enum ha_stat_type stat);
   int (*fill_files_table)(handlerton *hton, Session *session,
                           TableList *tables,
                           class Item *cond);
   std::bitset<HTON_BIT_SIZE> flags; /* global handler flags */
   int (*release_temporary_latches)(handlerton *hton, Session *session);

   int (*discover)(handlerton *hton, Session* session, const char *db, 
                   const char *name,
                   unsigned char **frmblob, 
                   size_t *frmlen);
   int (*table_exists_in_engine)(handlerton *hton, Session* session, const char *db,
                                 const char *name);
   uint32_t license; /* Flag for Engine License */
   void *data; /* Location for engines to keep personal structures */
};


/* lookups */
handlerton *ha_default_handlerton(Session *session);
plugin_ref ha_resolve_by_name(Session *session, const LEX_STRING *name);
plugin_ref ha_lock_engine(Session *session, handlerton *hton);
handlerton *ha_resolve_by_legacy_type(Session *session,
                                      enum legacy_db_type db_type);
handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         handlerton *db_type);
handlerton *ha_checktype(Session *session, enum legacy_db_type database_type,
                         bool no_substitute, bool report_error);

enum legacy_db_type ha_legacy_type(const handlerton *db_type);
const char *ha_resolve_storage_engine_name(const handlerton *db_type);
bool ha_check_storage_engine_flag(const handlerton *db_type, const hton_flag_bits flag);
bool ha_storage_engine_is_enabled(const handlerton *db_type);
LEX_STRING *ha_storage_engine_name(const handlerton *hton);

#endif /* DRIZZLED_HANDLERTON_H */
