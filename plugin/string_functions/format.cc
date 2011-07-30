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
#include "format.h"

#include <limits>

#include <drizzled/charset.h>
#include <drizzled/type/decimal.h>
#include <drizzled/table.h>

using namespace std;

namespace drizzled
{

/**
  Change a number to format '3,333,333,333.000'.

  This should be 'internationalized' sometimes.
*/

const int FORMAT_MAX_DECIMALS= 30;

void Item_func_format::fix_length_and_dec()
{
  collation.set(default_charset());
  uint32_t char_length= args[0]->max_length/args[0]->collation.collation->mbmaxlen;
  max_length= ((char_length + (char_length-args[0]->decimals)/3) *
               collation.collation->mbmaxlen);
}


/**
  @todo
  This needs to be fixed for multi-byte character set where numbers
  are stored in more than one byte
*/

String *Item_func_format::val_str(String *str)
{
  uint32_t length;
  uint32_t str_length;
  /* Number of decimal digits */
  int dec;
  /* Number of characters used to represent the decimals, including '.' */
  uint32_t dec_length;
  int diff;
  assert(fixed == 1);

  dec= (int) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1;
    return NULL;
  }

  dec= set_zone(dec, 0, FORMAT_MAX_DECIMALS);
  dec_length= dec ? dec+1 : 0;
  null_value=0;

  if (args[0]->result_type() == DECIMAL_RESULT ||
      args[0]->result_type() == INT_RESULT)
  {
    type::Decimal dec_val, rnd_dec, *res;
    res= args[0]->val_decimal(&dec_val);
    if ((null_value=args[0]->null_value))
      return 0;
    class_decimal_round(E_DEC_FATAL_ERROR, res, dec, false, &rnd_dec);
    class_decimal2string(&rnd_dec, 0, str);
    str_length= str->length();
    if (rnd_dec.sign())
      str_length--;
  }
  else
  {
    double nr= args[0]->val_real();
    if ((null_value=args[0]->null_value))
      return 0;
    nr= my_double_round(nr, (int64_t) dec, false, false);
    /* Here default_charset() is right as this is not an automatic conversion */
    str->set_real(nr, dec, default_charset());
    if (nr == numeric_limits<double>::quiet_NaN())
      return str;
    str_length=str->length();
    if (nr < 0)
      str_length--;				// Don't count sign
  }
  /* We need this test to handle 'nan' values */
  if (str_length >= dec_length+4)
  {
    char *tmp,*pos;
    length= str->length()+(diff=((int)(str_length- dec_length-1))/3);
    str= copy_if_not_alloced(&tmp_str,str,length);
    str->length(length);
    tmp= (char*) str->ptr()+length - dec_length-1;
    for (pos= (char*) str->ptr()+length-1; pos != tmp; pos--)
      pos[0]= pos[-diff];
    while (diff)
    {
      *pos= *(pos - diff);
      pos--;
      *pos= *(pos - diff);
      pos--;
      *pos= *(pos - diff);
      pos--;
      pos[0]=',';
      pos--;
      diff--;
    }
  }
  return str;
}


void Item_func_format::print(String *str)
{
  str->append(STRING_WITH_LEN("format("));
  args[0]->print(str);
  str->append(',');
  args[1]->print(str);
  str->append(')');
}

} /* namespace drizzled */
