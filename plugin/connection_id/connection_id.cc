/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <drizzled/function/math/int.h>
#include <drizzled/plugin/function.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

using namespace std;
using namespace drizzled;

class ConnectionIdFunction :public Item_int_func
{
  int64_t value;
public:
  ConnectionIdFunction() :Item_int_func() {}
  
  int64_t val_int() 
  {
    assert(fixed == true);
    return value;
  }
  
  const char *func_name() const 
  { 
    return "connection_id"; 
  }

  void fix_length_and_dec() 
  {
    Item_int_func::fix_length_and_dec();
    max_length= 10;
  }

  bool fix_fields(Session *session, Item **ref)
  {
    if (Item_int_func::fix_fields(session, ref))
    {
      return true;
    }

    value= session->variables.pseudo_thread_id;
    return false;
  }

  bool check_argument_count(int n)
  {
    return (n == 0);
  }
};


plugin::Create_function<ConnectionIdFunction> *connection_idudf= NULL;

static int initialize(module::Context &context)
{
  connection_idudf=
    new plugin::Create_function<ConnectionIdFunction>("connection_id");
  context.add(connection_idudf);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "connection_id",
  "1.0",
  "Devananda van der Veen",
  "Return the current connection_id",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
