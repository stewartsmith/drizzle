/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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


#include <config.h>
#include <algorithm>
#include <drizzled/field/boolean.h>
#include <drizzled/type/boolean.h>
#include <drizzled/error.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/session.h>
#include <drizzled/table.h>
#include <drizzled/temporal.h>

namespace drizzled {
namespace field {

Boolean::Boolean(unsigned char *ptr_arg,
                 uint32_t len_arg,
                 unsigned char *null_ptr_arg,
                 unsigned char null_bit_arg,
                 const char *field_name_arg,
                 bool ansi_display_arg) :
  Field(ptr_arg, len_arg,
        null_ptr_arg,
        null_bit_arg,
        Field::NONE,
        field_name_arg),
  ansi_display(ansi_display_arg)
  {
    if (ansi_display)
      flags|= UNSIGNED_FLAG;
  }

int Boolean::cmp(const unsigned char *a, const unsigned char *b)
{ 
  return memcmp(a, b, sizeof(unsigned char));
}

int Boolean::store(const char *from, uint32_t length, const charset_info_st * const )
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  bool result;
  if (not type::convert(result, from, length))
  {
    my_error(ER_INVALID_BOOLEAN_VALUE, MYF(0), from);
    return 1;
  }
  set(result);
  return 0;
}

int Boolean::store(int64_t nr, bool)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  set(nr != 0);
  return 0;
}

int  Boolean::store(double nr)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  set(nr);
  return 0;
}

int Boolean::store_decimal(const drizzled::type::Decimal *dec)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  set(not dec->isZero());
  return 0;
}

double Boolean::val_real() const
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  return isTrue();
}

int64_t Boolean::val_int() const
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  return isTrue();
}

String *Boolean::val_str(String *val_buffer, String *) const
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  (void)type::convert(*val_buffer, isTrue(), ansi_display);
  return val_buffer;
}

type::Decimal *Boolean::val_decimal(type::Decimal *dec) const
{
  if (isTrue())
  {
    int2_class_decimal(E_DEC_OK, 1, false, dec);
    return dec;
  }
  dec->set_zero();
  return dec;
}

void Boolean::sort_string(unsigned char *to, uint32_t length_arg)
{
  memcpy(to, ptr, length_arg);
}

} /* namespace field */
} /* namespace drizzled */
