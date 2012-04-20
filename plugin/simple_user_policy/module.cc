/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor <mordred@inaugust.com>
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

#include <drizzled/plugin/authorization.h>
#include <drizzled/module/option_map.h>
#include <boost/program_options.hpp>

namespace po= boost::program_options;

#include "policy.h"

using namespace drizzled;

namespace simple_user_policy
{

std::string remap_dot_to;

static int init(module::Context &context)
{
  context.add(new Policy);
  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("remap-dot-to",
    po::value<std::string>(&remap_dot_to)->default_value("."),
    N_("Remap '.' to another character/string when controlling access to a schema. Useful for usernames that have a '.' in them. If a '.' is remapped to an underscore, you don't have to quote the schema name."));
}

} /* namespace simple_user_policy */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "simple_user_policy",
  "1.1",
  "Monty Taylor",
  N_("Authorization matching username to schema object name"),
  PLUGIN_LICENSE_GPL,
  simple_user_policy::init,
  NULL,
  simple_user_policy::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
