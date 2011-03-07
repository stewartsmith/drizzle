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
#include <drizzled/plugin/function.h>
#include <drizzled/function/math/int.h>
#include <plugin/compression/uncompressed_length.h>

#include <string>

using namespace std;
using namespace drizzled;


int64_t Item_func_uncompressed_length::val_int()
{
  assert(fixed == 1);
  String *res= args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (res->is_empty()) return 0;

  /*
    res->ptr() using is safe because we have tested that string is not empty,
    res->c_ptr() is not used because:
      - we do not need \0 terminated string to get first 4 bytes
      - c_ptr() tests simbol after string end (uninitialiozed memory) which
        confuse valgrind
  */
  return uint4korr(res->ptr()) & 0x3FFFFFFF;
}

