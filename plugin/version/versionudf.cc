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
#include <drizzled/function/str/strfunc.h>
#include <drizzled/plugin/function.h>
#include <drizzled/version.h>

using namespace std;
using namespace drizzled;

class VersionFunction :public Item_str_func
{
public:
  String *val_str(String *str);
  VersionFunction() :Item_str_func() {}
  
  const char *func_name() const 
  { 
    return "version"; 
  }
  
  bool check_argument_count(int n)
  {
    return (n==0);
  }

  void fix_length_and_dec() {}
};

String *VersionFunction::val_str(String *str)
{
  str->set(::drizzled::version().c_str(), ::drizzled::version().size(),
           system_charset_info);
  return str;
}

plugin::Create_function<VersionFunction> *versionudf= NULL;

static int initialize(module::Context &context)
{
  versionudf= new plugin::Create_function<VersionFunction>("version");
  context.add(versionudf);
  return 0;
}

DRIZZLE_PLUGIN(initialize, NULL, NULL);
