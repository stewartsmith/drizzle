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

#include CSTDINT_H

class TableList;
typedef struct st_table_share TABLE_SHARE;
typedef bool (stat_print_fn)(Session *session, const char *type, uint32_t type_len,
                             const char *file, uint32_t file_len,
                             const char *status, uint32_t status_len);
enum ha_stat_type { HA_ENGINE_STATUS, HA_ENGINE_LOGS, HA_ENGINE_MUTEX };
extern st_plugin_int *hton2plugin[MAX_HA];

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
   uint32_t flags;                                /* global handler flags */
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



#endif /* DRIZZLED_HANDLERTON_H */
