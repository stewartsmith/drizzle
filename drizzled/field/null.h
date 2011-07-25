/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#pragma once

#include <drizzled/field/str.h>

namespace drizzled {

/*
 * Everything saved in this will disappear. It will always return NULL
 */

class Field_null : public Field_str
{
  static unsigned char null;
public:

  using Field::store;
  using Field::val_str;
  using Field::cmp;
  using Field::val_int;

  Field_null(unsigned char *ptr_arg, uint32_t len_arg, const char *field_name_arg) :
    Field_str(ptr_arg, len_arg, &null, 1, field_name_arg, &my_charset_bin)
  {}

  enum_field_types type() const
  {
    return DRIZZLE_TYPE_NULL;
  }
  int store(const char *, uint32_t, const charset_info_st * const)
  {
    return 0;
  }
  int store(double)
  {
    return 0;
  }
  int store(int64_t, bool)
  {
    return 0;
  }
  int store_decimal(const type::Decimal *)
  {
    return 0;
  }
  int reset()
  {
    return 0;
  }
  double val_real() const
  {
    return 0.0;
  }
  int64_t val_int() const
  {
    return 0;
  }
  type::Decimal *val_decimal(type::Decimal *) const
  {
    return 0;
  }
  String *val_str(String *, String *value2) const
  {
    value2->length(0);
    return value2;
  }
  int cmp(const unsigned char *, const unsigned char *)
  {
    return 0;
  }
  void sort_string(unsigned char *, uint32_t)
  {}
  uint32_t pack_length() const
  {
    return 0;
  }
  uint32_t size_of() const
  {
    return sizeof(*this);
  }
  uint32_t max_display_length()
  {
    return 4;
  }
};

} /* namespace drizzled */
