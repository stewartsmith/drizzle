/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_SLOT_STORAGE_ENGINE_H
#define DRIZZLED_SLOT_STORAGE_ENGINE_H

#include "drizzled/registry.h"

#include <string>

class Session;
class XID;
typedef struct st_hash HASH;

namespace drizzled
{

namespace plugin
{
  class StorageEngine;
  class TableNameIteratorImplementation;
}

namespace message
{
  class Table;
}

namespace slot
{

/**
 * Class to handle all Storage Engine plugins
 */
class StorageEngine
{
private:

  Registry<plugin::StorageEngine *> all_engines;

public:

  StorageEngine();
  ~StorageEngine();

  void add(plugin::StorageEngine *engine);
  void remove(plugin::StorageEngine *engine);

  Registry<plugin::StorageEngine *>::iterator begin()
  {
    return all_engines.begin();
  }

  Registry<plugin::StorageEngine *>::iterator end()
  {
    return all_engines.end();
  }

  int getTableProto(const char* path, message::Table *table_proto);

  plugin::StorageEngine *findByName(Session *session,
                                    std::string find_str);
  void closeConnection(Session* session);
  void dropDatabase(char* path);
  int commitOrRollbackByXID(XID *xid, bool commit);
  int releaseTemporaryLatches(Session *session);
  bool flushLogs(plugin::StorageEngine *db_type);
  int recover(HASH *commit_list);
  int startConsistentSnapshot(Session *session);
  int deleteTable(Session *session, const char *path, const char *db,
                  const char *alias, bool generate_warning);

};

class TableNameIterator
{
private:
  Registry<plugin::StorageEngine *>::iterator engine_iter;
  plugin::TableNameIteratorImplementation *current_implementation;
  plugin::TableNameIteratorImplementation *default_implementation;
  std::string database;
public:
  TableNameIterator(const std::string &db);
  ~TableNameIterator();

  int next(std::string *name);
};

} /* namespace slot */
} /* namespace drizzled */

/**
  Return the storage engine plugin::StorageEngine for the supplied name

  @param session         current thread
  @param name        name of storage engine

  @return
    pointer to storage engine plugin handle
*/
drizzled::plugin::StorageEngine *ha_resolve_by_name(Session *session,
                                                    const std::string &find_str);

void ha_close_connection(Session* session);
void ha_drop_database(char* path);
int ha_commit_or_rollback_by_xid(XID *xid, bool commit);

/* report to InnoDB that control passes to the client */
int ha_release_temporary_latches(Session *session);
bool ha_flush_logs(drizzled::plugin::StorageEngine *db_type);
int ha_recover(HASH *commit_list);
int ha_start_consistent_snapshot(Session *session);
int ha_delete_table(Session *session, const char *path,
                    const char *db, const char *alias, bool generate_warning);

#endif /* DRIZZLED_SLOT_STORAGE_ENGINE_H */
