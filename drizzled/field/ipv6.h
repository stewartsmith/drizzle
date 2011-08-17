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

#pragma once

#include <drizzled/field.h>
#include <string>

#include <drizzled/type/ipv6.h>

namespace drizzled
{
namespace field
{

class IPv6:public Field {
  const charset_info_st *field_charset;

public:
  IPv6(unsigned char *ptr_arg,
            uint32_t len_arg,
            unsigned char *null_ptr_arg,
            unsigned char null_bit_arg,
            const char *field_name_arg);

  enum_field_types type() const { return DRIZZLE_TYPE_IPV6; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  bool zero_pack() const { return 0; }
  int  reset(void) { memset(ptr, 0, type::IPv6::LENGTH); return 0; }
  uint32_t pack_length() const { return type::IPv6::LENGTH; }
  uint32_t key_length() const { return type::IPv6::LENGTH; }

  int  store(const char *to,uint32_t length, const charset_info_st * const charset);
  int  store(int64_t nr, bool unsigned_val);
  double val_real() const;
  int64_t val_int() const;
  String *val_str(String*,String *) const;
  void sql_type(drizzled::String&)  const;
  int store_decimal(const drizzled::type::Decimal*);

  Item_result result_type () const { return STRING_RESULT; }
  int cmp(const unsigned char*, const unsigned char*);
  void sort_string(unsigned char*, uint32_t);
  uint32_t max_display_length() { return type::IPv6::IPV6_DISPLAY_LENGTH; }

  int  store(double ) { return 0; }
  inline String *val_str(String *str) { return val_str(str, str); }
  uint32_t size_of() const { return sizeof(*this); }

  static size_t max_string_length()
  {
    return type::IPv6::LENGTH;
  }
};

} /* namespace field */
} /* namespace drizzled */


