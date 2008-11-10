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
#include <drizzled/functions/str/collation.h>
#include <drizzled/functions/str/concat.h>
#include <drizzled/functions/str/conv.h>
#include <drizzled/functions/str/conv_charset.h>
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
#include <drizzled/functions/str/set_collation.h>
#include <drizzled/functions/str/str_conv.h>
#include <drizzled/functions/str/substr.h>
#include <drizzled/functions/str/trim.h>
#include <drizzled/functions/str/user.h>
#include <drizzled/functions/str/uuid.h>
#include <drizzled/functions/str/weight_string.h>

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

#endif /* DRIZZLED_ITEM_STRFUNC_H */
