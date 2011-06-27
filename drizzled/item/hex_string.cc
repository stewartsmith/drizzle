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

#include <drizzled/error.h>
#include <drizzled/field.h>
#include <drizzled/item/hex_string.h>
#include <drizzled/item/string.h>
#include <drizzled/type/decimal.h>

#include <algorithm>

using namespace std;

namespace drizzled
{

static char _dig_vec_lower[] =
  "0123456789abcdefghijklmnopqrstuvwxyz";

inline uint32_t char_val(char X)
{
  return (uint32_t) (X >= '0' && X <= '9' ? X-'0' :
                 X >= 'A' && X <= 'Z' ? X-'A'+10 :
                 X-'a'+10);
}

Item_hex_string::Item_hex_string(const char *str, uint32_t str_length)
{
  max_length=(str_length+1)/2;
  char *ptr=(char*) memory::sql_alloc(max_length+1);
  if (!ptr)
    return;
  str_value.set(ptr,max_length,&my_charset_bin);
  char *end=ptr+max_length;
  if (max_length*2 != str_length)
    *ptr++=char_val(*str++);                    // Not even, assume 0 prefix
  while (ptr != end)
  {
    *ptr++= (char) (char_val(str[0])*16+char_val(str[1]));
    str+=2;
  }
  *ptr=0;                                       // Keep purify happy
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  fixed= 1;
  unsigned_flag= 1;
}

int64_t Item_hex_string::val_int()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  char *end= (char*) str_value.ptr()+str_value.length(),
       *ptr= end - min(str_value.length(), sizeof(int64_t));

  uint64_t value=0;
  for (; ptr != end ; ptr++)
    value=(value << 8)+ (uint64_t) (unsigned char) *ptr;
  return (int64_t) value;
}


type::Decimal *Item_hex_string::val_decimal(type::Decimal *decimal_value)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  uint64_t value= (uint64_t)val_int();
  int2_class_decimal(E_DEC_FATAL_ERROR, value, true, decimal_value);
  return (decimal_value);
}

int Item_hex_string::save_in_field(Field *field, bool)
{
  field->set_notnull();
  if (field->result_type() == STRING_RESULT)
    return field->store(str_value.ptr(), str_value.length(),
                        collation.collation);

  uint64_t nr;
  uint32_t length= str_value.length();
  if (length > 8)
  {
    nr= field->flags & UNSIGNED_FLAG ? UINT64_MAX : INT64_MAX;
    goto warn;
  }
  nr= (uint64_t) val_int();
  if ((length == 8) && !(field->flags & UNSIGNED_FLAG) && (nr > INT64_MAX))
  {
    nr= INT64_MAX;
    goto warn;
  }
  return field->store((int64_t) nr, true);  // Assume hex numbers are unsigned

warn:
  if (!field->store((int64_t) nr, true))
    field->set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE,
                       1);
  return 1;
}

void Item_hex_string::print(String *str)
{
  char *end= (char*) str_value.ptr() + str_value.length(),
       *ptr= end - min(str_value.length(), sizeof(int64_t));
  str->append("0x");
  for (; ptr != end ; ptr++)
  {
    str->append(_dig_vec_lower[((unsigned char) *ptr) >> 4]);
    str->append(_dig_vec_lower[((unsigned char) *ptr) & 0x0F]);
  }
}


bool Item_hex_string::eq(const Item *arg, bool binary_cmp) const
{
  if (arg->basic_const_item() && arg->type() == type())
  {
    if (binary_cmp)
      return !stringcmp(&str_value, &arg->str_value);
    return !sortcmp(&str_value, &arg->str_value, collation.collation);
  }
  return false;
}

Item *Item_hex_string::safe_charset_converter(const charset_info_st * const tocs)
{
  Item_string *conv;
  String tmp, *str= val_str(&tmp);

  conv= new Item_string(str->ptr(), str->length(), tocs);
  conv->str_value.copy();
  conv->str_value.mark_as_const();
  return conv;
}


} /* namespace drizzled */
