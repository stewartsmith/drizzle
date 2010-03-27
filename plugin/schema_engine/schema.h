/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef PLUGIN_SCHEMA_ENGINE_SCHEMA_H
#define PLUGIN_SCHEMA_ENGINE_SCHEMA_H

#include <assert.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/data_home.h>
#include <drizzled/hash.h>

#include <pthread.h>

extern const drizzled::CHARSET_INFO *default_charset_info;

static const char *schema_exts[] = {
  NULL
};

class Schema : public drizzled::plugin::StorageEngine
{
  bool writeSchemaFile(const char *path, const drizzled::message::Schema &db);
  bool readSchemaFile(const std::string &path, drizzled::message::Schema &schema);

  void prime();

  typedef drizzled::hash_map<std::string, drizzled::message::Schema> SchemaCache;
  SchemaCache schema_cache;
  bool schema_cache_filled;

  pthread_rwlock_t schema_lock;

public:
  Schema();

  ~Schema();


  bool doCanCreateTable(const drizzled::TableIdentifier &identifier);

  drizzled::Cursor *create(drizzled::TableShare &,
                           drizzled::memory::Root *)
  {
    return NULL;
  }

  void doGetSchemaNames(std::set<std::string>& set_of_names);
  bool doGetSchemaDefinition(const std::string &schema_name, drizzled::message::Schema &proto);

  bool doCreateSchema(const drizzled::message::Schema &schema_message);

  bool doAlterSchema(const drizzled::message::Schema &schema_message);

  bool doDropSchema(const std::string &schema_name);

  // Below are table methods that we don't implement (and don't need)

  int doGetTableDefinition(drizzled::Session&,
                           drizzled::TableIdentifier&,
                           drizzled::message::Table&)
  {
    return ENOENT;
  }


  void doGetTableNames(drizzled::CachedDirectory&,
                       std::string&,
                       std::set<std::string>&)
  {
  }

  bool doDoesTableExist(drizzled::Session&, drizzled::TableIdentifier&)
  {
    return false;
  }

  int doRenameTable(drizzled::Session&, drizzled::TableIdentifier&, drizzled::TableIdentifier&)
  {
    return EPERM;
  }

  int doCreateTable(drizzled::Session*,
                    drizzled::Table&,
                    drizzled::TableIdentifier&,
                    drizzled::message::Table&)
  {
    return EPERM;
  }

  int doDropTable(drizzled::Session&, drizzled::TableIdentifier&)
  {
    return 0;
  }

  const char **bas_ext() const 
  {
    return schema_exts;
  }
};

#endif /* PLUGIN_SCHEMA_ENGINE_SCHEMA_H */
