/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (C) 2010 Stewart Smith
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

#include "config.h"

#include <drizzled/session.h>
#include <drizzled/function/str/strfunc.h>

using namespace drizzled;

class DatabaseFunction :public Item_str_func
{
public:
  DatabaseFunction() :Item_str_func() {}
  String *val_str(String *);
  void fix_length_and_dec()
  {
    max_length= MAX_FIELD_NAME * system_charset_info->mbmaxlen;
    maybe_null=1;
  }
  const char *func_name() const { return "database"; }
  const char *fully_qualified_func_name() const { return "database()"; }
};

String *DatabaseFunction::val_str(String *str)
{
  assert(fixed == 1);
  Session *session= current_session;
  if (session->db.empty())
  {
    null_value= 1;
    return 0;
  }
  else
    str->copy(session->db.c_str(), session->db.length(), system_charset_info);
  return str;
}

plugin::Create_function<DatabaseFunction> *database_function= NULL;

static int initialize(drizzled::module::Context &context)
{
  database_function= new plugin::Create_function<DatabaseFunction>("database");
  context.add(database_function);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "database_function",
  "1.0",
  "Stewart Smith",
  "returns the current database",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
