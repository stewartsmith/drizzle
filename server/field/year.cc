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

#include "drizzle/server/field/year.h"

/****************************************************************************
** year type
** Save in a byte the year 0, 1901->2155
** Can handle 2 byte or 4 byte years!
****************************************************************************/

int Field_year::store(const char *from, uint len,CHARSET_INFO *cs)
{
  char *end;
  int error;
  int64_t nr= cs->cset->strntoull10rnd(cs, from, len, 0, &end, &error);

  if (nr < 0 || (nr >= 100 && nr <= 1900) || nr > 2155 || 
      error == MY_ERRNO_ERANGE)
  {
    *ptr=0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    return 1;
  }
  if (table->in_use->count_cuted_fields && 
      (error= check_int(cs, from, len, end, error)))
  {
    if (error == 1)  /* empty or incorrect string */
    {
      *ptr= 0;
      return 1;
    }
    error= 1;
  }

  if (nr != 0 || len != 4)
  {
    if (nr < YY_PART_YEAR)
      nr+=100;					// 2000 - 2069
    else if (nr > 1900)
      nr-= 1900;
  }
  *ptr= (char) (uchar) nr;
  return error;
}


int Field_year::store(double nr)
{
  if (nr < 0.0 || nr >= 2155.0)
  {
    (void) Field_year::store((int64_t) -1, false);
    return 1;
  }
  return Field_year::store((int64_t) nr, false);
}


int Field_year::store(int64_t nr,
                      bool unsigned_val __attribute__((__unused__)))
{
  if (nr < 0 || (nr >= 100 && nr <= 1900) || nr > 2155)
  {
    *ptr= 0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    return 1;
  }
  if (nr != 0 || field_length != 4)		// 0000 -> 0; 00 -> 2000
  {
    if (nr < YY_PART_YEAR)
      nr+=100;					// 2000 - 2069
    else if (nr > 1900)
      nr-= 1900;
  }
  *ptr= (char) (uchar) nr;
  return 0;
}


bool Field_year::send_binary(Protocol *protocol)
{
  uint64_t tmp= Field_year::val_int();
  return protocol->store_short(tmp);
}


double Field_year::val_real(void)
{
  return (double) Field_year::val_int();
}


int64_t Field_year::val_int(void)
{
  int tmp= (int) ptr[0];
  if (field_length != 4)
    tmp%=100;					// Return last 2 char
  else if (tmp)
    tmp+=1900;
  return (int64_t) tmp;
}


String *Field_year::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  val_buffer->alloc(5);
  val_buffer->length(field_length);
  char *to=(char*) val_buffer->ptr();
  sprintf(to,field_length == 2 ? "%02d" : "%04d",(int) Field_year::val_int());
  return val_buffer;
}


void Field_year::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*)res.ptr(),res.alloced_length(),
			  "year(%d)",(int) field_length));
}

