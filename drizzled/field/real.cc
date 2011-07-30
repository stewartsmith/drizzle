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

#include <config.h>
#include <drizzled/field/real.h>
#include <drizzled/error.h>
#include <drizzled/table.h>

#include <math.h>

#include <limits>

using namespace std;

namespace drizzled {

extern const double log_10[309];

/*
  Floating-point numbers
 */

unsigned char *
Field_real::pack(unsigned char *to, const unsigned char *from,
                 uint32_t max_length, bool low_byte_first)
{
  assert(max_length >= pack_length());
#ifdef WORDS_BIGENDIAN
  if (low_byte_first != getTable()->getShare()->db_low_byte_first)
  {
    const unsigned char *dptr= from + pack_length();
    while (dptr-- > from)
      *to++ = *dptr;
    return(to);
  }
  else
#endif
    return(Field::pack(to, from, max_length, low_byte_first));
}

const unsigned char *
Field_real::unpack(unsigned char *to, const unsigned char *from,
                   uint32_t param_data, bool low_byte_first)
{
#ifdef WORDS_BIGENDIAN
  if (low_byte_first != getTable()->getShare()->db_low_byte_first)
  {
    const unsigned char *dptr= from + pack_length();
    while (dptr-- > from)
      *to++ = *dptr;
    return(from + pack_length());
  }
  else
#endif
    return(Field::unpack(to, from, param_data, low_byte_first));
}

/*
  If a field has fixed length, truncate the double argument pointed to by 'nr'
  appropriately.
  Also ensure that the argument is within [-max_value; max_value] range.
*/

int Field_real::truncate(double *nr, double max_value)
{
  int error= 1;
  double res= *nr;

  if (res == numeric_limits<double>::quiet_NaN())
  {
    res= 0;
    set_null();
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    goto end;
  }

  if (!not_fixed)
  {
    uint32_t order= field_length - dec;
    uint32_t step= array_elements(log_10) - 1;
    max_value= 1.0;
    for (; order > step; order-= step)
      max_value*= log_10[step];
    max_value*= log_10[order];
    max_value-= 1.0 / log_10[dec];

    /* Check for infinity so we don't get NaN in calculations */
    if (res != numeric_limits<double>::infinity())
    {
      double tmp= rint((res - floor(res)) * log_10[dec]) / log_10[dec];
      res= floor(res) + tmp;
    }
  }

  if (res < -max_value)
  {
   res= -max_value;
   set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
  }
  else if (res > max_value)
  {
    res= max_value;
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
  }
  else
    error= 0;

end:
  *nr= res;
  return error;
}


int Field_real::store_decimal(const type::Decimal *dm)
{
  double dbl;
  class_decimal2double(E_DEC_FATAL_ERROR, dm, &dbl);
  return store(dbl);
}

type::Decimal *Field_real::val_decimal(type::Decimal *decimal_value) const
{
  ASSERT_COLUMN_MARKED_FOR_READ;

  double2_class_decimal(E_DEC_FATAL_ERROR, val_real(), decimal_value);
  return decimal_value;
}

} /* namespace drizzled */
