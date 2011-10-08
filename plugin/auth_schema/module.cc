/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright 2011 Daniel Nichter
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include <drizzled/item.h>
#include <drizzled/plugin.h>
#include <drizzled/module/option_map.h>
#include <boost/program_options.hpp>
#include "auth_schema.h"

namespace po= boost::program_options;

using namespace std;

namespace drizzle_plugin {
namespace auth_schema {

/**
 * Forward declarations.
 */
bool update_table(Session *, set_var *var);

/**
 * Singleton instance of the plugin.
 */
static AuthSchema *auth_schema= NULL;

bool update_table(Session *, set_var *var)
{
  if (not var->value->str_value.data())
  {
    errmsg_printf(error::ERROR, _("auth_schema table cannot be NULL"));
    return true; // error
  }
  const string table(var->value->str_value.data());
  return auth_schema->setTable(table);
}

static void init_options(module::option_context &context)
{
  auth_schema= new AuthSchema();

  context("table",
    po::value<string>(&auth_schema->sysvar_table)->default_value("auth.users"),
    N_("Database-qualified auth table name"));
}

static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  if (not vm["table"].as<string>().empty())
    auth_schema->setTable(vm["table"].as<string>());

  context.add(auth_schema);

  context.registerVariable(
    new sys_var_bool_ptr("enabled", &auth_schema->sysvar_enabled));

  context.registerVariable(
    new sys_var_std_string("table", auth_schema->sysvar_table, NULL, &update_table));

  return 0;
}

} /* end namespace drizzle_plugin::auth_schema */
} /* end namespace drizzle_plugin */

DRIZZLE_PLUGIN(
  drizzle_plugin::auth_schema::init,
  NULL,
  drizzle_plugin::auth_schema::init_options
);
