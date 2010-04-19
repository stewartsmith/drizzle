/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include "config.h"
#include "plugin/information_schema_dictionary/dictionary.h"

using namespace drizzled;

static CheckConstraints *check_constraints;
static ColumnDomainUsage *column_domain_usage;
static ColumnPrivileges *column_privileges;
static Columns *columns;
static ConstraintColumnUsage *constraint_column_usage;
static ConstraintTableUsage *constraint_table_usage;
static DomainConstraints *domain_constraints;
static Domains *domains;
static KeyColumnUsage *key_column_usage;
static Parameters *parameters;
static ReferentialConstraints *referential_constraints;
static Routines *routines;
static RoutineColumns *routine_columns;
static Schemata *schemata;
static TableConstraints *table_constraints;
static TablePriviledges *table_privileges;
static Tables *tables;
static ViewColumnUsage *view_column_usage;
static ViewTableUsage *view_table_usage;
static Views *views;

static int init(drizzled::plugin::Registry &registry)
{
  check_constraints = new(std::nothrow)CheckConstraints;
  column_domain_usage = new(std::nothrow)ColumnDomainUsage;
  column_privileges = new(std::nothrow)ColumnPrivileges;
  columns = new(std::nothrow)Columns;
  constraint_column_usage = new(std::nothrow)ConstraintColumnUsage;
  constraint_table_usage = new(std::nothrow)ConstraintTableUsage;
  domain_constraints = new(std::nothrow)DomainConstraints;
  domains = new(std::nothrow)Domains;
  key_column_usage = new(std::nothrow)KeyColumnUsage;
  parameters = new(std::nothrow)Parameters;
  referential_constraints = new(std::nothrow)ReferentialConstraints;
  routines = new(std::nothrow)Routines;
  routine_columns = new(std::nothrow)RoutineColumns;
  schemata = new(std::nothrow)Schemata;
  table_constraints = new(std::nothrow)TableConstraints;
  table_privileges = new(std::nothrow)TablePriviledges;
  tables = new(std::nothrow)Tables;
  view_column_usage = new(std::nothrow)ViewColumnUsage;
  view_table_usage = new(std::nothrow)ViewTableUsage;
  views = new(std::nothrow)Views;

  registry.add(check_constraints);
  registry.add(column_domain_usage);
  registry.add(column_privileges);
  registry.add(columns);
  registry.add(constraint_column_usage);
  registry.add(constraint_table_usage);
  registry.add(domain_constraints);
  registry.add(domains);
  registry.add(key_column_usage);
  registry.add(parameters);
  registry.add(referential_constraints);
  registry.add(routines);
  registry.add(routine_columns);
  registry.add(schemata);
  registry.add(table_constraints);
  registry.add(table_privileges);
  registry.add(tables);
  registry.add(view_column_usage);
  registry.add(view_table_usage);
  registry.add(views);

  return 0;
}

static int finalize(drizzled::plugin::Registry &registry)
{
  registry.remove(check_constraints);
  registry.remove(column_domain_usage);
  registry.remove(column_privileges);
  registry.remove(columns);
  registry.remove(constraint_column_usage);
  registry.remove(constraint_table_usage);
  registry.remove(domain_constraints);
  registry.remove(domains);
  registry.remove(key_column_usage);
  registry.remove(parameters);
  registry.remove(referential_constraints);
  registry.remove(routines);
  registry.remove(routine_columns);
  registry.remove(schemata);
  registry.remove(table_constraints);
  registry.remove(table_privileges);
  registry.remove(tables);
  registry.remove(view_column_usage);
  registry.remove(view_table_usage);
  registry.remove(views);

  delete check_constraints;
  delete column_domain_usage;
  delete column_privileges;
  delete columns;
  delete constraint_column_usage;
  delete constraint_table_usage;
  delete domain_constraints;
  delete domains;
  delete key_column_usage;
  delete parameters;
  delete referential_constraints;
  delete routines;
  delete routine_columns;
  delete schemata;
  delete table_constraints;
  delete table_privileges;
  delete tables;
  delete view_column_usage;
  delete view_table_usage;
  delete views;

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "information_schema_dictionary",
  "1.0",
  "Brian Aker",
  "Data Dictionary for ANSI information schema, etc",
  PLUGIN_LICENSE_GPL,
  init,
  finalize,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
