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

#ifndef PLUGIN_INFO_SCHEMA_HELPER_METHODS_H
#define PLUGIN_INFO_SCHEMA_HELPER_METHODS_H

#include "drizzled/plugin/info_schema_table.h"

bool show_status_array(Session *session, 
                       const char *wild,
                       SHOW_VAR *variables,
                       enum enum_var_type value_type,
                       struct system_status_var *status_var,
                       const char *prefix, Table *table,
                       bool ucase_names,
                       drizzled::plugin::InfoSchemaTable *schema_table);

void store_key_column_usage(Table *table, 
                            LEX_STRING *db_name,
                            LEX_STRING *table_name, 
                            const char *key_name,
                            uint32_t key_len, 
                            const char *con_type, 
                            uint32_t con_len,
                            int64_t idx);

/**
 * Iterate through the given vector of columns and delete the memory that
 * has been allocated for them.
 *
 * @param[out] cols vector to clear and de-allocate memory from
 */
void clearColumns(std::vector<const drizzled::plugin::ColumnInfo *> &cols);

#endif /* PLUGIN_INFO_SCHEMA_HELPER_METHODS_H */
