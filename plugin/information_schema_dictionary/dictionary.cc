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

#include <config.h>
#include <plugin/information_schema_dictionary/dictionary.h>

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

static int init(drizzled::module::Context &context)
{
  check_constraints = new CheckConstraints;
  column_domain_usage = new ColumnDomainUsage;
  column_privileges = new ColumnPrivileges;
  columns = new Columns;
  constraint_column_usage = new ConstraintColumnUsage;
  constraint_table_usage = new ConstraintTableUsage;
  domain_constraints = new DomainConstraints;
  domains = new Domains;
  key_column_usage = new KeyColumnUsage;
  parameters = new Parameters;
  referential_constraints = new ReferentialConstraints;
  routines = new Routines;
  routine_columns = new RoutineColumns;
  schemata = new Schemata;
  table_constraints = new TableConstraints;
  table_privileges = new TablePriviledges;
  tables = new Tables;
  view_column_usage = new ViewColumnUsage;
  view_table_usage = new ViewTableUsage;
  views = new Views;

  context.add(check_constraints);
  context.add(column_domain_usage);
  context.add(column_privileges);
  context.add(columns);
  context.add(constraint_column_usage);
  context.add(constraint_table_usage);
  context.add(domain_constraints);
  context.add(domains);
  context.add(key_column_usage);
  context.add(parameters);
  context.add(referential_constraints);
  context.add(routines);
  context.add(routine_columns);
  context.add(schemata);
  context.add(table_constraints);
  context.add(table_privileges);
  context.add(tables);
  context.add(view_column_usage);
  context.add(view_table_usage);
  context.add(views);

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
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
