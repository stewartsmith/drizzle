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

#include <drizzled/field/double.h>
#include <drizzled/drizzled_error_messages.h>

/****************************************************************************
  double precision floating point numbers
****************************************************************************/

int Field_double::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int error;
  char *end;
  double nr= my_strntod(cs,(char*) from, len, &end, &error);
  if (error || (!len || (((uint) (end-from) != len) && table->in_use->count_cuted_fields)))
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                (error ? ER_WARN_DATA_OUT_OF_RANGE : ER_WARN_DATA_TRUNCATED), 1);
    error= error ? 1 : 2;
  }
  Field_double::store(nr);
  return error;
}


int Field_double::store(double nr)
{
  int error= truncate(&nr, DBL_MAX);

#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    float8store(ptr,nr);
  }
  else
#endif
    doublestore(ptr,nr);
  return error;
}


int Field_double::store(int64_t nr, bool unsigned_val)
{
  return Field_double::store(unsigned_val ? uint64_t2double((uint64_t) nr) :
                             (double) nr);
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
  
  if (isnan(res))
  {
    res= 0;
    set_null();
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    goto end;
  }
  else if (unsigned_flag && res < 0)
  {
    res= 0;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    goto end;
  }

  if (!not_fixed)
  {
    uint order= field_length - dec;
    uint step= array_elements(log_10) - 1;
    max_value= 1.0;
    for (; order > step; order-= step)
      max_value*= log_10[step];
    max_value*= log_10[order];
    max_value-= 1.0 / log_10[dec];

    /* Check for infinity so we don't get NaN in calculations */
    if (!my_isinf(res))
    {
      double tmp= rint((res - floor(res)) * log_10[dec]) / log_10[dec];
      res= floor(res) + tmp;
    }
  }
  
  if (res < -max_value)
  {
   res= -max_value;
   set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
  }
  else if (res > max_value)
  {
    res= max_value;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
  }
  else
    error= 0;

end:
  *nr= res;
  return error;
}


int Field_real::store_decimal(const my_decimal *dm)
{
  double dbl;
  my_decimal2double(E_DEC_FATAL_ERROR, dm, &dbl);
  return store(dbl);
}

double Field_double::val_real(void)
{
  double j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    float8get(j,ptr);
  }
  else
#endif
    doubleget(j,ptr);
  return j;
}

int64_t Field_double::val_int(void)
{
  double j;
  int64_t res;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    float8get(j,ptr);
  }
  else
#endif
    doubleget(j,ptr);
  /* Check whether we fit into int64_t range */
  if (j <= (double) INT64_MIN)
  {
    res= (int64_t) INT64_MIN;
    goto warn;
  }
  if (j >= (double) (uint64_t) INT64_MAX)
  {
    res= (int64_t) INT64_MAX;
    goto warn;
  }
  return (int64_t) rint(j);

warn:
  {
    char buf[DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE];
    String tmp(buf, sizeof(buf), &my_charset_latin1), *str;
    str= val_str(&tmp, 0);
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "INTEGER",
                        str->c_ptr());
  }
  return res;
}


my_decimal *Field_real::val_decimal(my_decimal *decimal_value)
{
  double2my_decimal(E_DEC_FATAL_ERROR, val_real(), decimal_value);
  return decimal_value;
}


String *Field_double::val_str(String *val_buffer,
			      String *val_ptr __attribute__((unused)))
{
  double nr;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    float8get(nr,ptr);
  }
  else
#endif
    doubleget(nr,ptr);

  uint to_length=max(field_length, DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE);
  val_buffer->alloc(to_length);
  char *to=(char*) val_buffer->ptr();
  size_t len;

  if (dec >= NOT_FIXED_DEC)
    len= my_gcvt(nr, MY_GCVT_ARG_DOUBLE, to_length - 1, to, NULL);
  else
    len= my_fcvt(nr, dec, to, NULL);

  val_buffer->length((uint) len);

  return val_buffer;
}

bool Field_double::send_binary(Protocol *protocol)
{
  return protocol->store((double) Field_double::val_real(), dec, (String*) 0);
}


int Field_double::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  double a,b;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    float8get(a,a_ptr);
    float8get(b,b_ptr);
  }
  else
#endif
  {
    doubleget(a, a_ptr);
    doubleget(b, b_ptr);
  }
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}


#define DBL_EXP_DIG (sizeof(double)*8-DBL_MANT_DIG)

/* The following should work for IEEE */

void Field_double::sort_string(uchar *to,uint length __attribute__((unused)))
{
  double nr;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    float8get(nr,ptr);
  }
  else
#endif
    doubleget(nr,ptr);
  change_double_for_sort(nr, to);
}


/**
   Save the field metadata for double fields.

   Saves the pack length in the first byte of the field metadata array
   at index of *metadata_ptr.

   @param   metadata_ptr   First byte of field metadata

   @returns number of bytes written to metadata_ptr
*/
int Field_double::do_save_field_metadata(uchar *metadata_ptr)
{
  *metadata_ptr= pack_length();
  return 1;
}


void Field_double::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  if (dec == NOT_FIXED_DEC)
  {
    res.set_ascii(STRING_WITH_LEN("double"));
  }
  else
  {
    res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			    "double(%d,%d)",(int) field_length,dec));
  }
  add_unsigned(res);
}

