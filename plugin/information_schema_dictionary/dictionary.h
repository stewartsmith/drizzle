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

#pragma once

#include <set>

#include <drizzled/plugin/table_function.h>
#include <drizzled/plugin/storage_engine.h>

#include <plugin/information_schema_dictionary/information_schema.h>

#include <drizzled/generator.h>

#include <plugin/information_schema_dictionary/check_constraints.h>
#include <plugin/information_schema_dictionary/column_domain_usage.h>
#include <plugin/information_schema_dictionary/column_privileges.h>
#include <plugin/information_schema_dictionary/columns.h>
#include <plugin/information_schema_dictionary/constraint_column_usage.h>
#include <plugin/information_schema_dictionary/constraint_table_usage.h>
#include <plugin/information_schema_dictionary/domain_constraints.h>
#include <plugin/information_schema_dictionary/domains.h>
#include <plugin/information_schema_dictionary/key_column_usage.h>
#include <plugin/information_schema_dictionary/parameters.h>
#include <plugin/information_schema_dictionary/referential_constraints.h>
#include <plugin/information_schema_dictionary/routines.h>
#include <plugin/information_schema_dictionary/routine_columns.h>
#include <plugin/information_schema_dictionary/schemata.h>
#include <plugin/information_schema_dictionary/table_constraints.h>
#include <plugin/information_schema_dictionary/table_privileges.h>
#include <plugin/information_schema_dictionary/tables.h>
#include <plugin/information_schema_dictionary/view_column_usage.h>
#include <plugin/information_schema_dictionary/view_table_usage.h>
#include <plugin/information_schema_dictionary/views.h>

