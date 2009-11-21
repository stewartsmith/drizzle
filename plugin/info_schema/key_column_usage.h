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

#ifndef PLUGIN_INFO_SCHEMA_KEY_COLUMN_USAGE_H
#define PLUGIN_INFO_SCHEMA_KEY_COLUMN_USAGE_H

#include "drizzled/plugin/info_schema_table.h"

/**
 * @class KeyColUsageISMethods
 * @brief
 *   Class which implements any methods that the key column usage
 *   I_S table needs besides the default methods
 */
class KeyColUsageISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

class KeyColumnUsageIS
{
  /**
   * Create the various columns for the KEY_COLUMN_USAGE I_S table and add them
   * to the std::vector of columns for the KEY_COLUMN_USAGE table.
   *
   * @return cols vector to add columns to
   */
  static std::vector<const drizzled::plugin::ColumnInfo *>
    *createColumns();
public:
  static drizzled::plugin::InfoSchemaTable *getTable();
  static void cleanup();
};

#endif /* PLUGIN_INFO_SCHEMA_KEY_COLUMN_USAGE_H */
