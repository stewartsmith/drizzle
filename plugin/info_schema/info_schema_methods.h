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

#ifndef DRIZZLED_INFO_SCHEMA_METHODS_H
#define DRIZZLED_INFO_SCHEMA_METHODS_H

#include "drizzled/info_schema.h"

/**
 * @class CharSetISMethods
 * @brief
 *   Class which implements any methods that the 
 *   CHARACTER_SET I_S table needs besides the default
 *   methods.
 */
class CharSetISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

/**
 * @class CollationISMethods
 * @brief
 *   Class which implements any methods that the Collations
 *   I_S table needs besides the default methods
 */
class CollationISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);
};

/**
 * @class CollCharISMethods
 * @brief
 *   Class which implements any methods that the collation char set
 *   I_S table needs besides the default methods
 */
class CollCharISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);
};

/**
 * @class ColumnsISMethods
 * @brief
 *   Class which implements any methods that the COLUMNS
 *   I_S table needs besides the default methods
 */
class ColumnsISMethods : public InfoSchemaMethods
{
public:
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

/**
 * @class KeyColUsageISMethods
 * @brief
 *   Class which implements any methods that the key column usage
 *   I_S table needs besides the default methods
 */
class KeyColUsageISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

/**
 * @class OpenTablesISMethods
 * @brief
 *   Class which implements any methods that the OPEN_TABLES
 *   I_S table needs besides the default methods
 */
class OpenTablesISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);
};

/**
 * @class PluginsISMethods
 * @brief
 *   Class which implements any methods that the PLUGINS
 *   I_S table needs besides the default methods
 */
class PluginsISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);
};

/**
 * @class ProcessListISMethods
 * @brief
 *   Class which implements any methods that the PROCESSLIST
 *   I_S table needs besides the default methods
 */
class ProcessListISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);
};

/**
 * @class RefConstraintISMethods
 * @brief
 *   Class which implements any methods that the REFERENTIAL_CONSTRAINTS
 *   I_S table needs besides the default methods
 */
class RefConstraintsISMethods : public InfoSchemaMethods
{
public:
  /**
   * Fill and store records into I_S.referential_constraints table
   *
   * @param[in] session   thread handle
   * @param[in] tables    table list struct(processed table)
   * @param[in] table     I_S table
   * @param[in] res       1 means the error during opening of the processed table
   *                      0 means processed table is opened without error
   * @param[in] base_name db name
   * @param[in] file_name table name
   *
   * @return 0 on success; 1 on failure
   */
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

/**
 * @class SchemataISMethods
 * @brief
 *   Class which implements any methods that the SCHEMATA
 *   I_S table needs besides the default methods
 */
class SchemataISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

/**
 * @class StatsISMethods
 * @brief
 *   Class which implements any methods that the SCHEMATA
 *   I_S table needs besides the default methods
 */
class StatsISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

/**
 * @class TabConstraintsISMethods
 * @brief
 *   Class which implements any methods that the TABLE_CONSTRAINTS
 *   I_S table needs besides the default methods
 */
class TabConstraintsISMethods : public InfoSchemaMethods
{
public:
  virtual int processTable(Session *session, TableList *tables,
                           Table *table, bool res, LEX_STRING *db_name,
                           LEX_STRING *table_name) const;
};

/**
 * @class TablesISMethods
 * @brief
 *   Class which implements any methods that the TABLE_NAMES
 *   I_S table needs besides the default methods
 */
class TablesISMethods : public InfoSchemaMethods
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
class TabNamesISMethods : public InfoSchemaMethods
{
public:
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

#endif /* DRIZZLE_INFO_SCHEMA_METHODS_H */
