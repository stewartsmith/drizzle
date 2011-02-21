/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>
#include "gman_servers_set.h"
#include "function_map.h"

using namespace std;
using namespace drizzled;

String *Item_func_gman_servers_set::val_str(String *str)
{
  String *servers;
  String *function;

  if (arg_count < 1 || arg_count > 2 || !(servers= args[0]->val_str(str)))
  {
    null_value= 1;
    return NULL;
  }

  function= (arg_count == 2) ? args[1]->val_str(str) : NULL;

  if (!GetFunctionMap().add(string(function == NULL ? "" : function->ptr()),
                            string(servers->ptr())))
  {
    null_value= 1;
    return NULL;
  }

  null_value= 0;

  buffer.realloc(servers->length());
  strcpy(buffer.ptr(), servers->ptr());
  buffer.length(servers->length());
  return &buffer;
}
