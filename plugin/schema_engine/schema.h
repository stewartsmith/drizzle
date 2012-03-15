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

#pragma once

#include <assert.h>
#include <drizzled/plugin/storage_engine.h>
#include <boost/unordered_map.hpp>
#include <boost/thread/shared_mutex.hpp>

extern const drizzled::charset_info_st *default_charset_info;

class Schema : public drizzled::plugin::StorageEngine
{
  bool writeSchemaFile(const drizzled::identifier::Schema &schema_identifier, const drizzled::message::Schema &db);
  bool readSchemaFile(const drizzled::identifier::Schema &schema_identifier, drizzled::message::Schema &schema);
  bool readSchemaFile(std::string filename, drizzled::message::Schema &schema);

  void prime();

  typedef boost::unordered_map<std::string, drizzled::message::schema::shared_ptr> SchemaCache;
  SchemaCache schema_cache;
  bool schema_cache_filled;

  boost::shared_mutex mutex;

public:
  Schema();

  drizzled::Cursor* create(drizzled::Table&)
  {
    return NULL;
  }

  void doGetSchemaIdentifiers(drizzled::identifier::schema::vector&);
  drizzled::message::schema::shared_ptr doGetSchemaDefinition(const drizzled::identifier::Schema&);

  bool doCreateSchema(const drizzled::message::Schema&);

  bool doAlterSchema(const drizzled::message::Schema&);

  bool doDropSchema(const drizzled::identifier::Schema&);

  // Below are table methods that we don't implement (and don't need)

  int doGetTableDefinition(drizzled::Session&,
                           const drizzled::identifier::Table&,
                           drizzled::message::Table&)
  {
    return ENOENT;
  }

  bool doDoesTableExist(drizzled::Session&, const drizzled::identifier::Table&)
  {
    return false;
  }

  int doRenameTable(drizzled::Session&, const drizzled::identifier::Table&, const drizzled::identifier::Table&)
  {
    return drizzled::HA_ERR_NO_SUCH_TABLE;
  }

  int doCreateTable(drizzled::Session&,
                    drizzled::Table&,
                    const drizzled::identifier::Table&,
                    const drizzled::message::Table&)
  {
    return drizzled::ER_TABLE_PERMISSION_DENIED;
  }

  int doDropTable(drizzled::Session&, const drizzled::identifier::Table&)
  {
    return drizzled::HA_ERR_NO_SUCH_TABLE;
  }

  const char** bas_ext() const;

  void get_auto_increment(uint64_t, uint64_t,
                          uint64_t,
                          uint64_t *,
                          uint64_t *)
  {}

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::identifier::Schema &schema_identifier,
                             drizzled::identifier::table::vector &set_of_identifiers);
};

