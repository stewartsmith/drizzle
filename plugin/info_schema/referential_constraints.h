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

#ifndef PLUGIN_INFO_SCHEMA_REFERENTIAL_CONSTRAINTS_H
#define PLUGIN_INFO_SCHEMA_REFERENTIAL_CONSTRAINTS_H

#include "drizzled/plugin/info_schema_table.h"

/**
 * @class RefConstraintISMethods
 * @brief
 *   Class which implements any methods that the REFERENTIAL_CONSTRAINTS
 *   I_S table needs besides the default methods
 */
class RefConstraintsISMethods : public drizzled::plugin::InfoSchemaMethods
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

class ReferentialConstraintsIS
{
  /**
   * Create the various columns for the REFERENTIAL_CONSTRAINTS I_S table and add them
   * to the std::vector of columns for the REFERENTIAL_CONSTRAINTS table.
   *
   * @return cols vector to add columns to
   */
  static std::vector<const drizzled::plugin::ColumnInfo *>
    *createColumns();
public:
  static drizzled::plugin::InfoSchemaTable *getTable();
  static void cleanup();
};

#endif /* PLUGIN_INFO_SCHEMA_REFERENTIAL_CONSTRAINTS_H */
