/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef PLUGIN_FUNCTION_ENGINE_FUNCTION_H
#define PLUGIN_FUNCTION_ENGINE_FUNCTION_H

#include <assert.h>
#include <drizzled/session.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/plugin/table_function.h>

extern const drizzled::CHARSET_INFO *default_charset_info;

static const char *function_exts[] = {
  NULL
};

class Function : public drizzled::plugin::StorageEngine
{

public:
  Function(const std::string &name_arg);

  ~Function()
  { }

  drizzled::plugin::TableFunction *getTool(const char *name_arg);

  int doCreateTable(drizzled::Session&,
                    drizzled::Table&,
                    drizzled::TableIdentifier &,
                    drizzled::message::Table&)
  {
    return EPERM;
  }

  int doDropTable(drizzled::Session&, drizzled::TableIdentifier&)
  { 
    return EPERM; 
  }

  virtual drizzled::Cursor *create(drizzled::TableShare &table,
                                   drizzled::memory::Root *mem_root);

  const char **bas_ext() const 
  {
    return function_exts;
  }

  drizzled::plugin::TableFunction *getFunction(const std::string &path)
  {
    return drizzled::plugin::TableFunction::getFunction(path);
  }

  bool doCanCreateTable(const drizzled::TableIdentifier &identifier);


  void doGetTableNames(drizzled::CachedDirectory&, 
                       std::string &db, 
                       std::set<std::string> &set_of_names);

  int doGetTableDefinition(drizzled::Session &session,
                           drizzled::TableIdentifier &identifier,
                           drizzled::message::Table &table_message);

  void doGetSchemaNames(std::set<std::string>& set_of_names);

  bool doDoesTableExist(drizzled::Session& session, drizzled::TableIdentifier &identifier);

  bool doGetSchemaDefinition(const std::string &schema_name, drizzled::message::Schema &schema_message);

  int doRenameTable(drizzled::Session&, drizzled::TableIdentifier &, drizzled::TableIdentifier &)
  {
    return EPERM;
  }
};

#endif /* PLUGIN_FUNCTION_ENGINE_FUNCTION_H */
