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

#ifndef DRIZZLED_FUNCTION_STR_CONV_CHARSET_H
#define DRIZZLED_FUNCTION_STR_CONV_CHARSET_H

#include <drizzled/function/str/strfunc.h>

namespace drizzled
{

class Item_func_conv_charset :public Item_str_func
{
  bool use_cached_value;
public:
  bool safe;
  const CHARSET_INFO *conv_charset; // keep it public
  Item_func_conv_charset(Item *a, const CHARSET_INFO * const cs) :Item_str_func(a)
  { conv_charset= cs; use_cached_value= 0; safe= 0; }
  Item_func_conv_charset(Item *a, const CHARSET_INFO * const cs, bool cache_if_const)
    :Item_str_func(a)
  {
    assert(args[0]->fixed);
    conv_charset= cs;
    if (cache_if_const && args[0]->const_item())
    {
      size_t errors= 0;
      String tmp, *str= args[0]->val_str(&tmp);
      if (!str || str_value.copy(str->ptr(), str->length(),
                                 str->charset(), conv_charset, &errors))
        null_value= 1;
      use_cached_value= 1;
      str_value.mark_as_const();
      safe= (errors == 0);
    }
    else
    {
      use_cached_value= 0;
      /*
        Conversion from and to "binary" is safe.
        Conversion to Unicode is safe.
        Other kind of conversions are potentially lossy.
      */
      safe= (args[0]->collation.collation == &my_charset_bin ||
             cs == &my_charset_bin ||
             (cs->state & MY_CS_UNICODE));
    }
  }
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "convert"; }
  virtual void print(String *str, enum_query_type query_type);
};

} /* namespace drizzled */

#endif /* DRIZZLED_FUNCTION_STR_CONV_CHARSET_H */
