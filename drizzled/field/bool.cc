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


#include "config.h"

#include <algorithm>

#include "drizzled/field/bool.h"

#include "drizzled/error.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/session.h"
#include "drizzled/table.h"
#include "drizzled/temporal.h"

union set_true_t {
  unsigned char byte;
  bool is_true:1;

  set_true_t()
  {
    is_true= true;
  }
} set_true;

namespace drizzled
{
namespace field
{

Bool::Bool(unsigned char *ptr_arg,
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
}

int Bool::cmp(const unsigned char *a, const unsigned char *b)
{ 
  return memcmp(a, b, sizeof(unsigned char));
}

int Bool::store(const char *from, uint32_t length, const CHARSET_INFO * const )
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (length == 1)
  {
    switch (from[0])
    {
    case 'y': case 'Y':
    case 't': case 'T': // PG compatibility
      setTrue();
      return 0;

    case 'n': case 'N':
    case 'f': case 'F': // PG compatibility
      setFalse();
      return 0;

    default:
      my_error(ER_INVALID_BOOL_VALUE, MYF(0), from);
      return 1; // invalid
    }
  }
  else if ((length == 5) and (strcasecmp(from, "FALSE") == 0))
  {
    setFalse();
  }
  if ((length == 4) and (strcasecmp(from, "TRUE") == 0))
  {
    setTrue();
  }
  else if ((length == 5) and (strcasecmp(from, "FALSE") == 0))
  {
    setFalse();
  }
  else if ((length == 3) and (strcasecmp(from, "YES") == 0))
  {
    setTrue();
  }
  else if ((length == 2) and (strcasecmp(from, "NO") == 0))
  {
    setFalse();
  }
  else
  {
    my_error(ER_INVALID_BOOL_VALUE, MYF(0), from);
    return 1; // invalid
  }

  return 0;
}

int Bool::store(int64_t nr, bool )
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  nr == 0 ? setFalse() : setTrue();
  return 0;
}

int  Bool::store(double nr)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  nr == 0 ? setFalse() : setTrue();
  return 0;
}

int Bool::store_decimal(const drizzled::my_decimal*)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  my_error(ER_INVALID_BOOL_VALUE, MYF(ME_FATALERROR), " ");
  return 1;
}

void Bool::sql_type(String &res) const
{
  res.set_ascii(STRING_WITH_LEN("bool"));
}

double Bool::val_real()
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  return isTrue();
}

int64_t Bool::val_int()
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  return isTrue();
}

String *Bool::val_str(String *val_buffer, String *)
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  const CHARSET_INFO * const cs= &my_charset_bin;
  uint32_t mlength= (5) * cs->mbmaxlen;

  val_buffer->alloc(mlength);
  char *buffer=(char*) val_buffer->ptr();

  if (isTrue())
  {
    if (ansi_display)
    {
      memcpy(buffer, "YES", 3);
      val_buffer->length(3);
    }
    else
    {
      memcpy(buffer, "TRUE", 4);
      val_buffer->length(4);
    }
  }
  else
  {
    if (ansi_display)
    {
      memcpy(buffer, "NO", 2);
      val_buffer->length(2);
    }
    else
    {
      memcpy(buffer, "FALSE", 5);
      val_buffer->length(5);
    }
  }

  return val_buffer;
}

void Bool::sort_string(unsigned char *to, uint32_t length_arg)
{
  memcpy(to, ptr, length_arg);
}

void Bool::setTrue()
{
  ptr[0]= set_true.byte;
}

} /* namespace field */
} /* namespace drizzled */
