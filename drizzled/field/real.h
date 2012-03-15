/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <drizzled/field/num.h>

namespace drizzled
{

/* base class for float and double and decimal (old one) */
class Field_real :public Field_num {
public:
  bool not_fixed;

  using Field::unpack;
  using Field::pack;

  Field_real(unsigned char *ptr_arg, uint32_t len_arg, unsigned char *null_ptr_arg,
             unsigned char null_bit_arg, utype unireg_check_arg,
             const char *field_name_arg,
             uint8_t dec_arg, bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, dec_arg, zero_arg, unsigned_arg),
    not_fixed(dec_arg >= NOT_FIXED_DEC)
    {}
  int store_decimal(const type::Decimal *);
  type::Decimal *val_decimal(type::Decimal *) const;
  int truncate(double *nr, double max_length);
  uint32_t max_display_length() { return field_length; }
  uint32_t size_of() const { return sizeof(*this); }
  virtual const unsigned char *unpack(unsigned char* to, const unsigned char *from,
                              uint32_t param_data, bool low_byte_first);
  virtual unsigned char *pack(unsigned char* to, const unsigned char *from,
                      uint32_t max_length, bool low_byte_first);
};

} /* namespace drizzled */

