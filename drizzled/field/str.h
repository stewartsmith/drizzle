/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#pragma once

#include <drizzled/field.h>

#include <drizzled/visibility.h>

namespace drizzled
{

/* base class for all string related classes */

class DRIZZLED_API Field_str :
  public Field
{
protected:
  const charset_info_st *field_charset;
  enum Derivation field_derivation;
  int  report_if_important_data(const char *ptr, const char *end);
public:
  Field_str(unsigned char *ptr_arg,
            uint32_t len_arg,
            unsigned char *null_ptr_arg,
            unsigned char null_bit_arg,
            const char *field_name_arg,
            const charset_info_st * const charset);
  Item_result result_type () const { return STRING_RESULT; }
  uint32_t decimals() const { return NOT_FIXED_DEC; }

  using Field::store;
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val)=0;
  int  store_decimal(const type::Decimal *);
  int  store(const char *to,uint32_t length, const charset_info_st * const cs)=0;

  uint32_t size_of() const { return sizeof(*this); }
  const charset_info_st *charset(void) const { return field_charset; }
  void set_charset(const charset_info_st * const charset_arg)
  { field_charset= charset_arg; }
  enum Derivation derivation(void) const { return field_derivation; }
  virtual void set_derivation(enum Derivation derivation_arg)
  { field_derivation= derivation_arg; }
  bool binary() const { return field_charset == &my_charset_bin; }
  uint32_t max_display_length() { return field_length; }
  friend class CreateField;
  type::Decimal *val_decimal(type::Decimal *) const;
  virtual bool str_needs_quotes() { return true; }
  uint32_t max_data_length() const;
};

/*
  Report "not well formed" or "cannot convert" error
  after storing a character string info a field.

  SYNOPSIS
    check_string_copy_error()
    field                    - Field
    well_formed_error_pos    - where not well formed data was first met
    cannot_convert_error_pos - where a not-convertable character was first met
    end                      - end of the string
    cs                       - character set of the string

  NOTES
    As of version 5.0 both cases return the same error:

      "Invalid string value: 'xxx' for column 't' at row 1"

  Future versions will possibly introduce a new error message:

      "Cannot convert character string: 'xxx' for column 't' at row 1"

  RETURN
    false - If errors didn't happen
    true  - If an error happened
*/

bool check_string_copy_error(Field_str *field,
                             const char *well_formed_error_pos,
                             const char *cannot_convert_error_pos,
                             const char *end,
                             const charset_info_st * const cs);


} /* namespace drizzled */

