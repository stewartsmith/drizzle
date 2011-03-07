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

#include <drizzled/function/str/char.h>

namespace drizzled
{

String *Item_func_char::val_str(String *str)
{
  assert(fixed == 1);
  str->length(0);
  str->set_charset(collation.collation);
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    int32_t num=(int32_t) args[i]->val_int();
    if (!args[i]->null_value)
    {
      char char_num= (char) num;
      if (num&0xFF000000L) {
        str->append((char)(num>>24));
        goto b2;
      } else if (num&0xFF0000L) {
    b2:        str->append((char)(num>>16));
        goto b1;
      } else if (num&0xFF00L) {
    b1:        str->append((char)(num>>8));
      }
      str->append(&char_num, 1);
    }
  }
  str->realloc(str->length());                  // Add end 0 (for Purify)
  return check_well_formed_result(str);
}

} /* namespace drizzled */
