/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <drizzled/field.h>

namespace drizzled
{

class Field_num :public Field 
{
public:
  const uint8_t dec;
  bool decimal_precision;       // Purify cannot handle bit fields & only for decimal type
  bool unsigned_flag;   // Purify cannot handle bit fields

  Field_num(unsigned char *ptr_arg,uint32_t len_arg, unsigned char *null_ptr_arg,
            unsigned char null_bit_arg, utype unireg_check_arg,
            const char *field_name_arg,
            uint8_t dec_arg, bool zero_arg, bool unsigned_arg);

  Item_result result_type () const { return REAL_RESULT; }

  friend class CreateField;

  void make_field(SendField *);

  uint32_t decimals() const { return (uint32_t) dec; }

  uint32_t size_of() const { return sizeof(*this); }

  bool eq_def(Field *field);

  int store_decimal(const type::Decimal *);

  type::Decimal *val_decimal(type::Decimal *) const;

  uint32_t is_equal(CreateField *new_field);

  int check_int(const charset_info_st * const cs, const char *str, int length,
                const char *int_end, int error);

  bool get_int(const charset_info_st * const cs, const char *from, uint32_t len,
               int64_t *rnd, uint64_t unsigned_max,
               int64_t signed_min, int64_t signed_max);
};

} /* namespace drizzled */

