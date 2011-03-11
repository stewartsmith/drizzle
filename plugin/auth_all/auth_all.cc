/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#include <fstream>
#include <map>
#include <string>
#include <iostream>

#include <boost/program_options.hpp>

#include <drizzled/configmake.h>
#include <drizzled/plugin/authentication.h>
#include <drizzled/identifier.h>
#include <drizzled/util/convert.h>
#include <drizzled/module/option_map.h>

namespace po= boost::program_options;
namespace fs= boost::filesystem;

using namespace std;
using namespace drizzled;

namespace auth_all
{

static bool opt_allow_anonymous;

class AuthAll: public plugin::Authentication
{
public:

  AuthAll() :
    plugin::Authentication("auth_all")
  {
  }

private:

  /**
   * Base class method to check authentication for a user.
   */
  bool authenticate(const identifier::User &sctx, const string &)
  {
    if (not opt_allow_anonymous)
    {
      if (sctx.username().empty())
        return false;
    }

    return true;
  }
};

static int init(module::Context &context)
{
  context.add(new AuthAll());

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("allow_anonymous", 
          po::value<bool>(&opt_allow_anonymous)->default_value(false),
          N_("Allow anonymous access"));
}


} /* namespace auth_all */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "Allow-All-Authentication",
  "1.0",
  "Brian Aker",
  "Data Dictionary for utility tables",
  PLUGIN_LICENSE_GPL,
  auth_all::init,
  NULL,
  auth_all::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
