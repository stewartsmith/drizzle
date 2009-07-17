/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include <drizzled/function/math/int.h>
#include <drizzled/function/create.h>
#include <drizzled/session.h>

using namespace std;

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


Create_function<ConnectionIdFunction> connection_idudf(string("connection_id"));

static int initialize(PluginRegistry &registry)
{
  registry.add(&connection_idudf);
  return 0;
}

static int finalize(PluginRegistry &registry)
{
   registry.remove(&connection_idudf);
   return 0;
}

drizzle_declare_plugin(connection_id)
{
  "connection_id",
  "1.0",
  "Devananda van der Veen",
  "Return the current connection_id",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  finalize,   /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
