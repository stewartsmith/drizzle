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

#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/item/string.h>

namespace drizzled {

Item *Item_string::safe_charset_converter(const charset_info_st * const tocs)
{
  String tmp, cstr, *ostr= val_str(&tmp);
  cstr.copy(ostr->ptr(), ostr->length(), tocs);
  Item_string* conv= new Item_string(cstr.ptr(), cstr.length(), cstr.charset(), collation.derivation);
  char* ptr= getSession().mem.strmake(cstr);

  conv->str_value.set(ptr, cstr.length(), cstr.charset());
  /* Ensure that no one is going to change the result string */
  conv->str_value.mark_as_const();
  return conv;
}


Item *Item_static_string_func::safe_charset_converter(const charset_info_st * const tocs)
{
  String tmp, cstr, *ostr= val_str(&tmp);
  cstr.copy(ostr->ptr(), ostr->length(), tocs);
  Item_string* conv= new Item_static_string_func(func_name, cstr.ptr(), cstr.length(), cstr.charset(), collation.derivation);
  conv->str_value.copy();
  /* Ensure that no one is going to change the result string */
  conv->str_value.mark_as_const();
  return conv;
}


bool Item_string::eq(const Item *item, bool binary_cmp) const
{
  if (type() == item->type() && item->basic_const_item())
  {
    if (binary_cmp)
      return !stringcmp(&str_value, &item->str_value);
    return (collation.collation == item->collation.collation &&
            !sortcmp(&str_value, &item->str_value, collation.collation));
  }
  return 0;
}

void Item_string::print(String *str)
{
  if (is_cs_specified())
  {
    str->append('_');
    str->append(collation.collation->csname);
  }

  str->append('\'');

  str_value.print(str);

  str->append('\'');
}

double Item_string::val_real()
{
  assert(fixed == 1);
  int error;
  char *end, *org_end;
  double tmp;
  const charset_info_st * const cs= str_value.charset();

  org_end= (char*) str_value.ptr() + str_value.length();
  tmp= my_strntod(cs, (char*) str_value.ptr(), str_value.length(), &end,
                  &error);
  if (error || (end != org_end && !check_if_only_end_space(cs, end, org_end)))
  {
    /*
      We can use str_value.ptr() here as Item_string is gurantee to put an
      end \0 here.
    */
    push_warning_printf(&getSession(), DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "DOUBLE",
                        str_value.ptr());
  }
  return tmp;
}

/**
  @todo
  Give error if we wanted a signed integer and we got an unsigned one
*/
int64_t Item_string::val_int()
{
  assert(fixed == 1);
  int err;
  int64_t tmp;
  char *end= (char*) str_value.ptr()+ str_value.length();
  char *org_end= end;
  const charset_info_st * const cs= str_value.charset();

  tmp= (*(cs->cset->strtoll10))(cs, str_value.ptr(), &end, &err);
  /*
    TODO: Give error if we wanted a signed integer and we got an unsigned
    one
  */
  if (err > 0 ||
      (end != org_end && !check_if_only_end_space(cs, end, org_end)))
  {
    push_warning_printf(&getSession(), DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "INTEGER",
                        str_value.ptr());
  }
  return tmp;
}

type::Decimal *Item_string::val_decimal(type::Decimal *decimal_value)
{
  return val_decimal_from_string(decimal_value);
}

int Item_string::save_in_field(Field *field, bool)
{
  String *result;
  result=val_str(&str_value);
  return save_str_value_in_field(field, result);
}


} /* namespace drizzled */
