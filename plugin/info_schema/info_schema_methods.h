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

#ifndef PLUGIN_INFO_SCHEMA_INFO_SCHEMA_METHODS_H
#define PLUGIN_INFO_SCHEMA_INFO_SCHEMA_METHODS_H

#include "drizzled/plugin/info_schema_table.h"

void store_key_column_usage(Table *table, 
                            LEX_STRING *db_name,
                            LEX_STRING *table_name, 
                            const char *key_name,
                            uint32_t key_len, 
                            const char *con_type, 
                            uint32_t con_len,
                            int64_t idx);

/**
 * @class StatsISMethods
 * @brief
 *   Class which implements any methods that the SCHEMATA
 *   I_S table needs besides the default methods
 */
class StatsISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

/**
 * @class StatusISMethods
 * @brief
 *   Class which implements any methods that the STATUS
 *   I_S table needs besides the default methods
 */
class StatusISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables);
};

/**
 * @class TablesISMethods
 * @brief
 *   Class which implements any methods that the TABLE_NAMES
 *   I_S table needs besides the default methods
 */
class TablesISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

/**
 * @class TabNamesISMethods
 * @brief
 *   Class which implements any methods that the TABLE_NAMES
 *   I_S table needs besides the default methods
 */
class TabNamesISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int oldFormat(Session *session,
                        drizzled::plugin::InfoSchemaTable *schema_table) const;
};

/**
 * @class VariablesISMethods
 * @brief
 *   Class which implements any methods that the VARIABLES
 *   I_S table needs besides the default methods
 */
class VariablesISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables);
};

#endif /* PLUGIN_INFO_SCHEMA_INFO_SCHEMA_METHODS_H */
