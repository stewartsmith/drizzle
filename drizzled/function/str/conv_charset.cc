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

#include <drizzled/function/str/conv_charset.h>

namespace drizzled {

String *Item_func_conv_charset::val_str(String *str)
{
  assert(fixed == 1);
  if (use_cached_value)
    return null_value ? 0 : &str_value;
  String *arg= args[0]->val_str(str);
  if (!arg)
  {
    null_value=1;
    return 0;
  }
  null_value= false;
  str_value.copy(arg->ptr(),arg->length(), conv_charset);
  return null_value ? 0 : check_well_formed_result(&str_value);
}

void Item_func_conv_charset::fix_length_and_dec()
{
  collation.set(conv_charset, DERIVATION_IMPLICIT);
  max_length = args[0]->max_length*conv_charset->mbmaxlen;
}

void Item_func_conv_charset::print(String *str)
{
  str->append(STRING_WITH_LEN("convert("));
  args[0]->print(str);
  str->append(STRING_WITH_LEN(" using "));
  str->append(conv_charset->csname);
  str->append(')');
}

} /* namespace drizzled */
