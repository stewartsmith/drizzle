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

#ifndef DRIZZLE_SERVER_FIELD_LONGSTR
#define DRIZZLE_SERVER_FIELD_LONGSTR

#include <drizzled/field/str.h>

/* base class for Field_varstring and Field_blob */

class Field_longstr :public Field_str
{
protected:
  int  report_if_important_data(const char *ptr, const char *end);
public:
  Field_longstr(unsigned char *ptr_arg, uint32_t len_arg, unsigned char *null_ptr_arg,
                unsigned char null_bit_arg, utype unireg_check_arg,
                const char *field_name_arg, const CHARSET_INFO * const charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg, unireg_check_arg,
               field_name_arg, charset_arg)
    {}

  int store_decimal(const my_decimal *d);
  uint32_t max_data_length() const;
};

#endif
