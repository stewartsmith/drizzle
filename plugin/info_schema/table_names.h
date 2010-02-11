/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef PLUGIN_INFO_SCHEMA_TABLE_NAMES_H
#define PLUGIN_INFO_SCHEMA_TABLE_NAMES_H

#include "drizzled/plugin/info_schema_table.h"

/**
 * @class TabNamesISMethods
 * @brief
 *   Class which implements any methods that the TABLE_NAMES
 *   I_S table needs besides the default methods
 */
class TabNamesISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int oldFormat(drizzled::Session *session,
                        drizzled::plugin::InfoSchemaTable *schema_table) const;
};

class TableNamesIS
{
  /**
   * Create the various columns for the TABLE_NAMES I_S table and add them
   * to the std::vector of columns for the TABLE_NAMES table.
   *
   * @return cols vector to add columns to
   */
  static std::vector<const drizzled::plugin::ColumnInfo *>
    *createColumns();
public:
  static drizzled::plugin::InfoSchemaTable *getTable();
  static void cleanup();
};

#endif /* PLUGIN_INFO_SCHEMA_TABLE_NAMES_H */
