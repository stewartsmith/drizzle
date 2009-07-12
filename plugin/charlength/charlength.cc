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

using namespace std;

class CharLengthFunction :public Item_int_func
{
  String value;
public:
  int64_t val_int();
  CharLengthFunction() :Item_int_func() {}
  
  const char *func_name() const 
  { 
    return "char_length"; 
  }

  void fix_length_and_dec() 
  { 
    max_length= 10;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};


int64_t CharLengthFunction::val_int()
{
  assert(fixed == true);
  String *res=args[0]->val_str(&value);
  
  if (res == NULL)
  {
    null_value= true;
    return 0;
  }
  
  null_value= false;
  return (int64_t) res->numchars();
}

Create_function<CharLengthFunction> charlengthudf(string("char_length"));
Create_function<CharLengthFunction> characterlengthudf(string("character_length"));

static int initialize(PluginRegistry &registry)
{
  registry.add(&charlengthudf);
  registry.add(&characterlengthudf);
  return 0;
}

static int finalize(PluginRegistry &registry)
{
   registry.remove(&charlengthudf);
   registry.remove(&characterlengthudf);
   return 0;
}

drizzle_declare_plugin(charlength)
{
  "charlength",
  "1.0",
  "Devananda van der Veen",
  "Return the number of characters in a string",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  finalize,   /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
