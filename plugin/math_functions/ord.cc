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

#include "ord.h"

#include <drizzled/charset.h>
#include <drizzled/type/decimal.h>

namespace drizzled
{

int64_t Item_func_ord::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (!res->length()) return 0;
  if (use_mb(res->charset()))
  {
    register const char *str=res->ptr();
    register uint32_t n=0, l=my_ismbchar(res->charset(),str,str+res->length());
    if (!l)
      return (int64_t)((unsigned char) *str);
    while (l--)
      n=(n<<8)|(uint32_t)((unsigned char) *str++);
    return (int64_t) n;
  }
  return (int64_t) ((unsigned char) (*res)[0]);
}

} /* namespace drizzled */
