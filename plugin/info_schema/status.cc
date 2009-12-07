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
 *   status I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/tztime.h"

#include "helper_methods.h"
#include "status.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the status I_S tables.
 */
vector<const plugin::ColumnInfo *> *status_columns= NULL;

/*
 * Methods for the status related I_S tables.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * status I_S tables.
 */
static plugin::InfoSchemaTable *glob_status_table= NULL;
static plugin::InfoSchemaTable *sess_status_table= NULL;
static plugin::InfoSchemaTable *status_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *GlobalStatusIS::createColumns()
{
  if (status_columns == NULL)
  {
    status_columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*status_columns);
  }

  status_columns->push_back(new plugin::ColumnInfo("VARIABLE_NAME",
                                                   64,
                                                   DRIZZLE_TYPE_VARCHAR,
                                                   0,
                                                   0,
                                                   "Variable_name"));

  status_columns->push_back(new plugin::ColumnInfo("VARIABLE_VALUE",
                                                   16300,
                                                   DRIZZLE_TYPE_VARCHAR,
                                                   0,
                                                   1,
                                                   "Value"));

  return status_columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *GlobalStatusIS::getTable()
{
  status_columns= createColumns();

  if (methods == NULL)
  {
    methods= new StatusISMethods();
  }

  if (glob_status_table == NULL)
  {
    glob_status_table= new plugin::InfoSchemaTable("GLOBAL_STATUS",
                                                   *status_columns,
                                                   -1, -1, false, false,
                                                   0, 
                                                   methods);
  }

  return glob_status_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void GlobalStatusIS::cleanup()
{
  clearColumns(*status_columns);
  delete glob_status_table;
  delete methods;
  delete status_columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *SessionStatusIS::getTable()
{
  if (sess_status_table == NULL)
  {
    sess_status_table= new plugin::InfoSchemaTable("SESSION_STATUS",
                                                   *status_columns,
                                                   -1, -1, false, false,
                                                   0, 
                                                   methods);
  }

  return sess_status_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void SessionStatusIS::cleanup()
{
  delete sess_status_table;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *StatusIS::getTable()
{
  if (status_table == NULL)
  {
    status_table= new plugin::InfoSchemaTable("STATUS",
                                              *status_columns,
                                              -1, -1, true, false, 0,
                                              methods);
  }

  return status_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void StatusIS::cleanup()
{
  delete status_table;
}

int StatusISMethods::fillTable(Session *session, 
                               Table *table,
                               plugin::InfoSchemaTable *schema_table)
{
  LEX *lex= session->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NULL;
  int res= 0;
  STATUS_VAR *tmp1, tmp;
  const string schema_table_name= schema_table->getTableName();
  enum enum_var_type option_type;
  bool upper_case_names= (schema_table_name.compare("STATUS") != 0);

  if (schema_table_name.compare("STATUS") == 0)
  {
    option_type= lex->option_type;
    if (option_type == OPT_GLOBAL)
    {
      tmp1= &tmp;
    }
    else
    {
      tmp1= session->initial_status_var;
    }
  }
  else if (schema_table_name.compare("GLOBAL_STATUS") == 0)
  {
    option_type= OPT_GLOBAL;
    tmp1= &tmp;
  }
  else
  {
    option_type= OPT_SESSION;
    tmp1= &session->status_var;
  }

  pthread_mutex_lock(&LOCK_status);
  if (option_type == OPT_GLOBAL)
  {
    calc_sum_of_all_status(&tmp);
  }
  res= show_status_array(session, wild,
                         getFrontOfStatusVars(),
                         option_type, tmp1, "", table,
                         upper_case_names,
                         schema_table);
  pthread_mutex_unlock(&LOCK_status);
  return res;
}

