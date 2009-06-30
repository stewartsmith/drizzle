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

#ifndef DRIZZLED_INFO_SCHEMA_COLUMNS_H
#define DRIZZLED_INFO_SCHEMA_COLUMNS_H

#include "drizzled/info_schema.h"

/**
 * Create the various columns for the CHARACTER_SET I_S table and add them
 * to the std::vector of columns for the CHARACTER_SET table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createCharSetColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various columns for the Collations I_S table and add them
 * to the std::vector of columns for the Collations table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createCollationColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various columns for the character set applicability
 * I_S table and add them to the std::vector of columns for the
 * table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createCollCharSetColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various columns for the COLUMNS
 * I_S table and add them to the std::vector of columns for the
 * table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createColColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various columns for the key column usage
 * I_S table and add them to the std::vector of columns for the
 * table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createKeyColUsageColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various columns for the OPEN_TABLES
 * I_S table and add them to the std::vector of columns for the
 * table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createOpenTabColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various columns for the PLUGINS
 * I_S table and add them to the std::vector of columns for the
 * table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createPluginsColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various volumns for the PROCESSLIST I_S table and add them
 * to the std::vector of columns for the PROCESSLIST table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createProcessListColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various volumns for the REFERENTIAL_CONSTRAINTS I_S table 
 * and add them to the std::vector of columns for this table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createRefConstraintColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various volumns for the TABLE_CONSTRAINTS I_S table 
 * and add them to the std::vector of columns for this table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createTabConstraintsColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various volumns for the TABLE_NAMES I_S table 
 * and add them to the std::vector of columns for this table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createTablesColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Create the various volumns for the TABLE_NAMES I_S table 
 * and add them to the std::vector of columns for this table.
 *
 * @param[out] cols vector to add columns to
 * @return false on success; true on failure
 */
bool createTabNamesColumns(std::vector<const ColumnInfo *>& cols);

/**
 * Iterate through the given vector of columns and delete the memory that
 * has been allocated for them.
 *
 * @param[out] cols vector to clear and de-allocate memory from
 */
void clearColumns(std::vector<const ColumnInfo *>& cols);

#endif /* DRIZZLE_INFO_SCHEMA_COLUMNS_H */
