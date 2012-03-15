/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/plugin/table_function.h>
#include <drizzled/identifier/schema.h>

extern const drizzled::charset_info_st *default_charset_info;

static const char *function_exts[] = {
  NULL
};

class Function : public drizzled::plugin::StorageEngine
{
  drizzled::message::schema::shared_ptr information_message;
  drizzled::message::schema::shared_ptr data_dictionary_message;

public:
  Function(const std::string &name_arg);

  ~Function()
  { }

  drizzled::plugin::TableFunction *getTool(const char *name_arg);

  int doCreateTable(drizzled::Session&,
                    drizzled::Table&,
                    const drizzled::identifier::Table &,
                    const drizzled::message::Table&)
  {
    return drizzled::ER_TABLE_PERMISSION_DENIED;
  }

  int doDropTable(drizzled::Session&, const drizzled::identifier::Table&)
  { 
    return drizzled::HA_ERR_NO_SUCH_TABLE; 
  }

  virtual drizzled::Cursor *create(drizzled::Table &table);

  const char **bas_ext() const 
  {
    return function_exts;
  }

  drizzled::plugin::TableFunction *getFunction(const std::string &path)
  {
    return drizzled::plugin::TableFunction::getFunction(path);
  }

  bool doCanCreateTable(const drizzled::identifier::Table &identifier);


  int doGetTableDefinition(drizzled::Session &session,
                           const drizzled::identifier::Table &identifier,
                           drizzled::message::Table &table_message);

  void doGetSchemaIdentifiers(drizzled::identifier::schema::vector&);

  bool doDoesTableExist(drizzled::Session& session, const drizzled::identifier::Table &identifier);

  drizzled::message::schema::shared_ptr doGetSchemaDefinition(const drizzled::identifier::Schema &schema);

  int doRenameTable(drizzled::Session&, const drizzled::identifier::Table &, const drizzled::identifier::Table &)
  {
    return EPERM;
  }

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::identifier::Schema &schema_identifier,
                             drizzled::identifier::table::vector &set_of_identifiers);
};

