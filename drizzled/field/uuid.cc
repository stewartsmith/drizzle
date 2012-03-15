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

#include <uuid/uuid.h>

#include <drizzled/field/uuid.h>

#include <drizzled/error.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/session.h>
#include <drizzled/table.h>

namespace drizzled
{
namespace field
{

Uuid::Uuid(unsigned char *ptr_arg,
           uint32_t len_arg,
           unsigned char *null_ptr_arg,
           unsigned char null_bit_arg,
           const char *field_name_arg) :
  Field(ptr_arg, len_arg,
        null_ptr_arg,
        null_bit_arg,
        Field::NONE,
        field_name_arg),
  is_set(false)
{
}

int Uuid::cmp(const unsigned char *a, const unsigned char *b)
{ 
  return memcmp(a, b, sizeof(uuid_t));
}

int Uuid::store(const char *from, uint32_t length, const charset_info_st * const )
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  type::Uuid uu;

  if (is_set)
  {
    is_set= false;
    return 0;
  }

  if (length != type::Uuid::DISPLAY_LENGTH)
  {
    my_error(ER_INVALID_UUID_VALUE, MYF(ME_FATALERROR));
    return 1;
  }

  if (uu.parse(from))
  {
    my_error(ER_INVALID_UUID_VALUE, MYF(ME_FATALERROR));
    return 1;
  }

  uu.pack(ptr);

  return 0;
}

int Uuid::store(int64_t , bool )
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  my_error(ER_INVALID_UUID_VALUE, MYF(ME_FATALERROR));
  return 1;
}

int Uuid::store_decimal(const drizzled::type::Decimal*)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  my_error(ER_INVALID_UUID_VALUE, MYF(ME_FATALERROR));
  return 1;
}

double Uuid::val_real() const
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  my_error(ER_INVALID_UUID_VALUE, MYF(ME_FATALERROR));
  return 0;
}

int64_t Uuid::val_int() const
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  my_error(ER_INVALID_UUID_VALUE, MYF(ME_FATALERROR));
  return 0;
}

#ifdef NOT_YET
void Uuid::generate()
{
  uuid_t uu;
  uuid_generate_time(uu);
  memcpy(ptr, uu, sizeof(uuid_t));
  is_set= true;
}

void Uuid::set(const unsigned char *arg)
{
  memcpy(ptr, arg, sizeof(uuid_t));
  is_set= true;
}
#endif

String *Uuid::val_str(String *val_buffer, String *) const
{
  const charset_info_st * const cs= &my_charset_bin;
  uint32_t mlength= (type::Uuid::DISPLAY_BUFFER_LENGTH) * cs->mbmaxlen;
  type::Uuid uu;

  val_buffer->alloc(mlength);
  char *buffer=(char*) val_buffer->ptr();

  ASSERT_COLUMN_MARKED_FOR_READ;

  uu.unpack(ptr);
  uu.unparse(buffer);

  val_buffer->length(type::Uuid::DISPLAY_LENGTH);

  return val_buffer;
}

void Uuid::sort_string(unsigned char *to, uint32_t length_arg)
{
  assert(length_arg == type::Uuid::LENGTH);
  memcpy(to, ptr, length_arg);
}

bool Uuid::get_date(type::Time &ltime, uint32_t ) const
{
  type::Uuid uu;

  uu.unpack(ptr);

  if (uu.isTimeType())
  {
    struct timeval ret_tv;

    memset(&ret_tv, 0, sizeof(struct timeval));

    uu.time(ret_tv);

    ltime.store(ret_tv);

    return false;
  }
  ltime.reset();

  return true;
}

bool Uuid::get_time(type::Time &ltime) const
{
  return get_date(ltime, 0);
}

} /* namespace field */
} /* namespace drizzled */
