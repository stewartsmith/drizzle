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

/**
 * @file 
 *   variables I_S table methods.
 */

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
#include "variables.h"

#include <vector>

#include "drizzled/pthread_globals.h"

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the variables I_S tables.
 */
extern vector<const plugin::ColumnInfo *> *status_columns;

/*
 * Methods for the variables related I_S tables.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * variables I_S tables.
 */
static plugin::InfoSchemaTable *glob_var_table= NULL;
static plugin::InfoSchemaTable *sess_var_table= NULL;
static plugin::InfoSchemaTable *var_table= NULL;

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *GlobalVariablesIS::getTable()
{
  if (methods == NULL)
  {
    methods= new VariablesISMethods();
  }

  if (glob_var_table == NULL)
  {
    glob_var_table= new plugin::InfoSchemaTable("GLOBAL_VARIABLES",
                                                *status_columns,
                                                -1, -1, false, false,
                                                0, 
                                                methods);
  }

  return glob_var_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void GlobalVariablesIS::cleanup()
{
  delete glob_var_table;
  delete methods;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *SessionVariablesIS::getTable()
{
  if (sess_var_table == NULL)
  {
    sess_var_table= new plugin::InfoSchemaTable("SESSION_VARIABLES",
                                                *status_columns,
                                                -1, -1, false, false, 0,
                                                methods);
  }

  return sess_var_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void SessionVariablesIS::cleanup()
{
  delete sess_var_table;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *VariablesIS::getTable()
{
  if (var_table == NULL)
  {
    var_table= new plugin::InfoSchemaTable("VARIABLES",
                                           *status_columns,
                                           -1, -1, true, false, 0,
                                           methods);
  }

  return var_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void VariablesIS::cleanup()
{
  delete var_table;
}

int VariablesISMethods::fillTable(Session *session, 
                                  Table *table,
                                  plugin::InfoSchemaTable *schema_table)
{
  int res= 0;
  LEX *lex= session->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NULL;
  const string schema_table_name= schema_table->getTableName();
  sql_var_t option_type= OPT_SESSION;
  bool upper_case_names= (schema_table_name.compare("VARIABLES") != 0);
  bool sorted_vars= (schema_table_name.compare("VARIABLES") == 0);

  if (lex->option_type == OPT_GLOBAL ||
      schema_table_name.compare("GLOBAL_VARIABLES") == 0)
  {
    option_type= OPT_GLOBAL;
  }

  pthread_rwlock_rdlock(&LOCK_system_variables_hash);
  res= show_status_array(session, wild, enumerate_sys_vars(session, sorted_vars),
                         option_type, NULL, "", table, upper_case_names, schema_table);
  pthread_rwlock_unlock(&LOCK_system_variables_hash);
  return res;
}

