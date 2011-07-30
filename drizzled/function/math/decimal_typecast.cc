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
#include <drizzled/function/math/decimal_typecast.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/internal/m_string.h>

namespace drizzled
{

String *Item_decimal_typecast::val_str(String *str)
{
  type::Decimal tmp_buf, *tmp= val_decimal(&tmp_buf);
  if (null_value)
    return NULL;
  class_decimal2string(tmp, 0, str);
  return str;
}


double Item_decimal_typecast::val_real()
{
  type::Decimal tmp_buf, *tmp= val_decimal(&tmp_buf);
  double res;
  if (null_value)
    return 0.0;
  class_decimal2double(E_DEC_FATAL_ERROR, tmp, &res);
  return res;
}


int64_t Item_decimal_typecast::val_int()
{
  type::Decimal tmp_buf, *tmp= val_decimal(&tmp_buf);
  int64_t res;
  if (null_value)
    return 0;

  tmp->val_int32(E_DEC_FATAL_ERROR, unsigned_flag, &res);

  return res;
}


type::Decimal *Item_decimal_typecast::val_decimal(type::Decimal *dec)
{
  type::Decimal tmp_buf, *tmp= args[0]->val_decimal(&tmp_buf);
  bool sign;
  uint32_t precision;

  if ((null_value= args[0]->null_value))
    return NULL;
  class_decimal_round(E_DEC_FATAL_ERROR, tmp, decimals, false, dec);
  sign= dec->sign();
  if (unsigned_flag)
  {
    if (sign)
    {
      dec->set_zero();
      goto err;
    }
  }
  precision= class_decimal_length_to_precision(max_length,
                                            decimals, unsigned_flag);
  if (precision - decimals < (uint) class_decimal_intg(dec))
  {
    max_Decimal(dec, precision, decimals);
    dec->sign(sign);
    goto err;
  }
  return dec;

err:
  push_warning_printf(&getSession(), DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                      ER_WARN_DATA_OUT_OF_RANGE,
                      ER(ER_WARN_DATA_OUT_OF_RANGE),
                      name, 1);
  return dec;
}


void Item_decimal_typecast::print(String *str)
{
  char len_buf[20*3 + 1];
  char *end;

  uint32_t precision= class_decimal_length_to_precision(max_length, decimals,
                                                 unsigned_flag);
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str);
  str->append(STRING_WITH_LEN(" as decimal("));

  end=internal::int10_to_str(precision, len_buf,10);
  str->append(len_buf, (uint32_t) (end - len_buf));

  str->append(',');

  end=internal::int10_to_str(decimals, len_buf,10);
  str->append(len_buf, (uint32_t) (end - len_buf));

  str->append(')');
  str->append(')');
}

} /* namespace drizzled */
