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

#include <cstdio>

#include <drizzled/current_session.h>
#include <drizzled/error.h>
#include <drizzled/function/time/typecast.h>
#include <drizzled/time_functions.h>
#include <drizzled/charset.h>

namespace drizzled {

bool Item_char_typecast::eq(const Item *item, bool binary_cmp) const
{
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM ||
      functype() != ((Item_func*)item)->functype())
    return 0;

  Item_char_typecast *cast= (Item_char_typecast*)item;
  if (cast_length != cast->cast_length ||
      cast_cs     != cast->cast_cs)
    return 0;

  if (!args[0]->eq(cast->args[0], binary_cmp))
      return 0;
  return 1;
}

void Item_typecast::print(String *str)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str);
  str->append(STRING_WITH_LEN(" as "));
  str->append(cast_type());
  str->append(')');
}


void Item_char_typecast::print(String *str)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str);
  str->append(STRING_WITH_LEN(" as char"));
  if (cast_length >= 0)
  {
    str->append('(');
    char buffer[20];
    // my_charset_bin is good enough for numbers
    String st(buffer, sizeof(buffer), &my_charset_bin);
    st.set((uint64_t)cast_length, &my_charset_bin);
    str->append(st);
    str->append(')');
  }
  if (cast_cs)
  {
    str->append(STRING_WITH_LEN(" charset "));
    str->append(cast_cs->csname);
  }
  str->append(')');
}

String *Item_char_typecast::val_str(String *str)
{
  assert(fixed == 1);
  String *res;
  uint32_t length;

  if (!charset_conversion)
  {
    if (!(res= args[0]->val_str(str)))
    {
      null_value= 1;
      return 0;
    }
  }
  else
  {
    // Convert character set if differ
    if (!(res= args[0]->val_str(&tmp_value)))
    {
      null_value= 1;
      return 0;
    }
		str->copy(res->ptr(), res->length(), cast_cs);
    res= str;
  }

  res->set_charset(cast_cs);

  /*
    Cut the tail if cast with length
    and the result is longer than cast length, e.g.
    CAST('string' AS CHAR(1))
  */
  if (cast_length >= 0)
  {
    if (res->length() > (length= (uint32_t) res->charpos(cast_length)))
    {                                           // Safe even if const arg
      char char_type[40];
      snprintf(char_type, sizeof(char_type), "%s(%lu)",
               cast_cs == &my_charset_bin ? "BINARY" : "CHAR",
               (ulong) cast_length);

      if (!res->alloced_length())
      {                                         // Don't change const str
        str_value= *res;                        // Not malloced string
        res= &str_value;
      }
      push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_TRUNCATED_WRONG_VALUE,
                          ER(ER_TRUNCATED_WRONG_VALUE), char_type,
                          res->c_ptr_safe());
      res->length((uint) length);
    }
    else if (cast_cs == &my_charset_bin && res->length() < (uint) cast_length)
    {
      if (res->alloced_length() < (uint) cast_length)
      {
        str->alloc(cast_length);
        str->copy(*res);
        res= str;
      }
      memset(res->ptr() + res->length(), 0,
             (uint) cast_length - res->length());
      res->length(cast_length);
    }
  }
  null_value= 0;
  return res;
}


void Item_char_typecast::fix_length_and_dec()
{
  uint32_t char_length;
  /*
     We always force character set conversion if cast_cs
     is a multi-byte character set. It garantees that the
     result of CAST is a well-formed string.
     For single-byte character sets we allow just to copy
     from the argument. A single-byte character sets string
     is always well-formed.

     There is a special trick to convert form a number to ucs2.
     As numbers have my_charset_bin as their character set,
     it wouldn't do conversion to ucs2 without an additional action.
     To force conversion, we should pretend to be non-binary.
     Let's choose from_cs this way:
     - If the argument in a number and cast_cs is ucs2 (i.e. mbminlen > 1),
       then from_cs is set to latin1, to perform latin1 -> ucs2 conversion.
     - If the argument is a number and cast_cs is ASCII-compatible
       (i.e. mbminlen == 1), then from_cs is set to cast_cs,
       which allows just to take over the args[0]->val_str() result
       and thus avoid unnecessary character set conversion.
     - If the argument is not a number, then from_cs is set to
       the argument's charset.
  */
  from_cs= (args[0]->result_type() == INT_RESULT ||
            args[0]->result_type() == DECIMAL_RESULT ||
            args[0]->result_type() == REAL_RESULT) ?
           (cast_cs->mbminlen == 1 ? cast_cs : &my_charset_utf8_general_ci) :
           args[0]->collation.collation;
  charset_conversion= (cast_cs->mbmaxlen > 1) ||
                      (!my_charset_same(from_cs, cast_cs) && from_cs != &my_charset_bin && cast_cs != &my_charset_bin);
  collation.set(cast_cs, DERIVATION_IMPLICIT);
  char_length= (cast_length >= 0) ? (uint32_t)cast_length :
	       (uint32_t)args[0]->max_length/from_cs->mbmaxlen;
  max_length= char_length * cast_cs->mbmaxlen;
}


String *Item_datetime_typecast::val_str(String *str)
{
  assert(fixed == 1);
  type::Time ltime;

  if (not get_arg0_date(ltime, TIME_FUZZY_DATE))
  {
    if (ltime.second_part)
    {
      ltime.convert(*str);
    }
    else
    {
      ltime.convert(*str);
    }

    return str;
  }

  null_value=1;
  return 0;
}


int64_t Item_datetime_typecast::val_int()
{
  assert(fixed == 1);
  type::Time ltime;
  if (get_arg0_date(ltime, 1))
  {
    null_value= 1;
    return 0;
  }

  int64_t tmp;
  ltime.convert(tmp);

  return tmp;
}


bool Item_date_typecast::get_date(type::Time &ltime, uint32_t )
{
  bool res= get_arg0_date(ltime, TIME_FUZZY_DATE);

  ltime.hour= ltime.minute= ltime.second= ltime.second_part= 0;
  ltime.time_type= type::DRIZZLE_TIMESTAMP_DATE;

  return res;
}


bool Item_date_typecast::get_time(type::Time &ltime)
{
  ltime.reset();

  return args[0]->null_value;
}


String *Item_date_typecast::val_str(String *str)
{
  assert(fixed == 1);
  type::Time ltime;

  if (!get_arg0_date(ltime, TIME_FUZZY_DATE))
  {
    str->alloc(type::Time::MAX_STRING_LENGTH);
    ltime.convert(*str, type::DRIZZLE_TIMESTAMP_DATE);

    return str;
  }

  null_value=1;
  return 0;
}

int64_t Item_date_typecast::val_int()
{
  assert(fixed == 1);
  type::Time ltime;

  if ((null_value= args[0]->get_date(ltime, TIME_FUZZY_DATE)))
    return 0;

  return (int64_t) (ltime.year * 10000L + ltime.month * 100 + ltime.day);
}

} /* namespace drizzled */
