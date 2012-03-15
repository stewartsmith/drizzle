/*
  Original copyright header listed below. This comes via rsync.
  Any additional changes are provided via the same license as the original.

  Copyright (C) 2011 Muhammad Umair

*/
/*
 * Copyright (C) 1996-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include "config.h"

#include <algorithm>

#include <drizzled/field/ipv6.h>

#include <drizzled/error.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/session.h>
#include <drizzled/table.h>

namespace drizzled
{
namespace field
{

IPv6::IPv6(unsigned char *ptr_arg,
           uint32_t len_arg,
           unsigned char *null_ptr_arg,
           unsigned char null_bit_arg,
           const char *field_name_arg) :
  Field(ptr_arg, len_arg,
        null_ptr_arg,
        null_bit_arg,
        Field::NONE,
        field_name_arg)
{
}

int IPv6::cmp(const unsigned char *a, const unsigned char *b)
{
  return memcmp(a, b, max_string_length());
}
  
int IPv6::store(const char *from, uint32_t, const charset_info_st * const )
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  type::IPv6 ptr_address;

  if (not ptr_address.inet_pton(from))
  {
    my_error(ER_INVALID_IPV6_VALUE, MYF(ME_FATALERROR));
    return 1;
  }

 ptr_address.store_object(ptr);

  return 0;
}

int IPv6::store(int64_t , bool )
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  my_error(ER_INVALID_IPV6_VALUE, MYF(ME_FATALERROR));
  return 1;
}

int IPv6::store_decimal(const drizzled::type::Decimal*)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  my_error(ER_INVALID_IPV6_VALUE, MYF(ME_FATALERROR));
  return 1;
}

void IPv6::sql_type(String &res) const
{
  res.set_ascii(STRING_WITH_LEN("ipv6"));
}

double IPv6::val_real() const
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  my_error(ER_INVALID_IPV6_VALUE, MYF(ME_FATALERROR));
  return 0;
}

int64_t IPv6::val_int() const
{
  ASSERT_COLUMN_MARKED_FOR_READ;
  my_error(ER_INVALID_IPV6_VALUE, MYF(ME_FATALERROR));
  return 0;
}

String *IPv6::val_str(String *val_buffer, String *) const
{
  const charset_info_st * const cs= &my_charset_bin;
  uint32_t mlength= (type::IPv6::IPV6_BUFFER_LENGTH)  * cs->mbmaxlen;  
  type::IPv6 ptr_address;

  val_buffer->alloc(mlength);
  char *buffer=(char*) val_buffer->ptr();
 
  ASSERT_COLUMN_MARKED_FOR_READ;

  ptr_address.restore_object(ptr);  
  ptr_address.inet_ntop(buffer);

  val_buffer->length(type::IPv6::IPV6_DISPLAY_LENGTH);

  return val_buffer;
}

void IPv6::sort_string(unsigned char *to, uint32_t length_arg)
{
  assert(length_arg == type::IPv6::LENGTH);
  memcpy(to, ptr, length_arg);
}

} /* namespace field */
} /* namespace drizzled */

