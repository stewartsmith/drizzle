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

namespace drizzled
{

class Field_datetime :public Field_str {
public:

  using Field::store;
  using Field::val_int;
  using Field::val_str;
  using Field::cmp;

  Field_datetime(unsigned char *ptr_arg,
                 unsigned char *null_ptr_arg,
                 unsigned char null_bit_arg,
                 const char *field_name_arg) :
    Field_str(ptr_arg,
               19,
               null_ptr_arg,
               null_bit_arg,
               field_name_arg,
               &my_charset_bin)
  {}

  Field_datetime(bool maybe_null_arg,
                 const char *field_name_arg) :
    Field_str((unsigned char*) 0,
               19,
               maybe_null_arg ? (unsigned char*) "": 0,
               0,
               field_name_arg,
               &my_charset_bin) 
  {}

  enum_field_types type() const { return DRIZZLE_TYPE_DATETIME;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONGLONG; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  uint32_t decimals() const { return DATETIME_DEC; }
  int  store(const char *to,uint32_t length, const charset_info_st * const charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  int store_time(type::Time &ltime, type::timestamp_t type);
  int reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return 0;
  }
  double val_real(void) const;
  int64_t val_int(void) const;
  String *val_str(String*,String *) const;
  int cmp(const unsigned char *,const unsigned char *);
  void sort_string(unsigned char *buff,uint32_t length);
  uint32_t pack_length() const { return 8; }
  bool can_be_compared_as_int64_t() const { return true; }
  bool zero_pack() const { return 1; }
  bool get_date(type::Time &ltime,uint32_t fuzzydate) const;
  bool get_time(type::Time &ltime) const;
};

} /* namespace drizzled */

