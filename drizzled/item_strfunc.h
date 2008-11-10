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

#ifndef DRIZZLED_ITEM_STRFUNC_H
#define DRIZZLED_ITEM_STRFUNC_H

#include <drizzled/functions/str/sysconst.h>
#include <drizzled/functions/str/alloc_buffer.h>
#include <drizzled/functions/str/binary.h>
#include <drizzled/functions/str/char.h>
#include <drizzled/functions/str/charset.h>
#include <drizzled/functions/str/concat.h>
#include <drizzled/functions/str/conv.h>
#include <drizzled/functions/str/database.h>
#include <drizzled/functions/str/elt.h>
#include <drizzled/functions/str/export_set.h>
#include <drizzled/functions/str/format.h>
#include <drizzled/functions/str/hex.h>
#include <drizzled/functions/str/insert.h>
#include <drizzled/functions/str/left.h>
#include <drizzled/functions/str/make_set.h>
#include <drizzled/functions/str/pad.h>
#include <drizzled/functions/str/repeat.h>
#include <drizzled/functions/str/replace.h>
#include <drizzled/functions/str/reverse.h>
#include <drizzled/functions/str/right.h>
#include <drizzled/functions/str/str_conv.h>
#include <drizzled/functions/str/substr.h>
#include <drizzled/functions/str/trim.h>
#include <drizzled/functions/str/user.h>
#include <drizzled/functions/str/uuid.h>

class Item_load_file :public Item_str_func
{
  String tmp_value;
public:
  Item_load_file(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  const char *func_name() const { return "load_file"; }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
    maybe_null=1;
    max_length=MAX_BLOB_WIDTH;
  }
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};


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
      uint32_t errors= 0;
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

class Item_func_set_collation :public Item_str_func
{
public:
  Item_func_set_collation(Item *a, Item *b) :Item_str_func(a,b) {};
  String *val_str(String *);
  void fix_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "collate"; }
  enum Functype functype() const { return COLLATE_FUNC; }
  virtual void print(String *str, enum_query_type query_type);
  Item_field *filed_for_view_update()
  {
    /* this function is transparent for view updating */
    return args[0]->filed_for_view_update();
  }
};

class Item_func_collation :public Item_str_func
{
public:
  Item_func_collation(Item *a) :Item_str_func(a) {}
  String *val_str(String *);
  const char *func_name() const { return "collation"; }
  void fix_length_and_dec()
  {
     collation.set(system_charset_info);
     max_length= 64 * collation.collation->mbmaxlen; // should be enough
     maybe_null= 0;
  };
  table_map not_null_tables() const { return 0; }
};


class Item_func_weight_string :public Item_str_func
{
  String tmp_value;
  uint32_t flags;
  uint32_t nweights;
public:
  Item_func_weight_string(Item *a, uint32_t nweights_arg, uint32_t flags_arg)
  :Item_str_func(a) { nweights= nweights_arg; flags= flags_arg; }
  const char *func_name() const { return "weight_string"; }
  String *val_str(String *);
  void fix_length_and_dec();
  /*
    TODO: Currently this Item is not allowed for virtual columns
    only due to a bug in generating virtual column value.
  */
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

#endif /* DRIZZLED_ITEM_STRFUNC_H */
