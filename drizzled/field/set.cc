/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include <drizzled/mysql_priv.h>
#include <drizzled/field/set.h>

/*
   set type.
   This is a string which can have a collection of different values.
   Each string value is separated with a ','.
   For example "One,two,five"
   If one uses this string in a number context one gets the bits as a int64_t
   number.
*/

const char field_separator=',';

int Field_set::store(const char *from,uint length, const CHARSET_INFO * const cs)
{
  bool got_warning= 0;
  int err= 0;
  char *not_used;
  uint not_used2;
  uint32_t not_used_offset;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);

  /* Convert character set if necessary */
  if (String::needs_conversion(length, cs, field_charset, &not_used_offset))
  { 
    uint dummy_errors;
    tmpstr.copy(from, length, cs, field_charset, &dummy_errors);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }
  uint64_t tmp= find_set(typelib, from, length, field_charset,
                          &not_used, &not_used2, &got_warning);
  if (!tmp && length && length < 22)
  {
    /* This is for reading numbers with LOAD DATA INFILE */
    char *end;
    tmp=my_strntoull(cs,from,length,10,&end,&err);
    if (err || end != from+length ||
	tmp > (uint64_t) (((int64_t) 1 << typelib->count) - (int64_t) 1))
    {
      tmp=0;      
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    }
  }
  else if (got_warning)
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
  store_type(tmp);
  return err;
}


int Field_set::store(int64_t nr,
                     bool unsigned_val __attribute__((unused)))
{
  int error= 0;
  uint64_t max_nr= set_bits(uint64_t, typelib->count);
  if ((uint64_t) nr > max_nr)
  {
    nr&= max_nr;
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    error=1;
  }
  store_type((uint64_t) nr);
  return error;
}


String *Field_set::val_str(String *val_buffer,
			   String *val_ptr __attribute__((unused)))
{
  uint64_t tmp=(uint64_t) Field_enum::val_int();
  uint bitnr=0;

  val_buffer->length(0);
  val_buffer->set_charset(field_charset);
  while (tmp && bitnr < (uint) typelib->count)
  {
    if (tmp & 1)
    {
      if (val_buffer->length())
        val_buffer->append(&field_separator, 1, &my_charset_latin1);
      String str(typelib->type_names[bitnr],
		 typelib->type_lengths[bitnr],
		 field_charset);
      val_buffer->append(str);
    }
    tmp>>=1;
    bitnr++;
  }
  return val_buffer;
}


void Field_set::sql_type(String &res) const
{
  char buffer[255];
  String set_item(buffer, sizeof(buffer), res.charset());

  res.length(0);
  res.append(STRING_WITH_LEN("set("));

  bool flag=0;
  uint *len= typelib->type_lengths;
  for (const char **pos= typelib->type_names; *pos; pos++, len++)
  {
    uint dummy_errors;
    if (flag)
      res.append(',');
    /* convert to res.charset() == utf8, then quote */
    set_item.copy(*pos, *len, charset(), res.charset(), &dummy_errors);
    append_unescaped(&res, set_item.ptr(), set_item.length());
    flag= 1;
  }
  res.append(')');
}

