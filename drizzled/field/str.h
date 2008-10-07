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

#ifndef DRIZZLE_SERVER_FIELD_STR
#define DRIZZLE_SERVER_FIELD_STR

/* base class for all string related classes */

class Field_str :public Field {
protected:
  const CHARSET_INFO *field_charset;
  enum Derivation field_derivation;
public:
  Field_str(unsigned char *ptr_arg,uint32_t len_arg, unsigned char *null_ptr_arg,
            unsigned char null_bit_arg, utype unireg_check_arg,
            const char *field_name_arg, const CHARSET_INFO * const charset);
  Item_result result_type () const { return STRING_RESULT; }
  uint32_t decimals() const { return NOT_FIXED_DEC; }
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val)=0;
  int  store_decimal(const my_decimal *);
  int  store(const char *to,uint32_t length, const CHARSET_INFO * const cs)=0;
  uint32_t size_of() const { return sizeof(*this); }
  const CHARSET_INFO *charset(void) const { return field_charset; }
  void set_charset(const CHARSET_INFO * const charset_arg) { field_charset= charset_arg; }
  enum Derivation derivation(void) const { return field_derivation; }
  virtual void set_derivation(enum Derivation derivation_arg)
  { field_derivation= derivation_arg; }
  bool binary() const { return field_charset == &my_charset_bin; }
  uint32_t max_display_length() { return field_length; }
  friend class Create_field;
  my_decimal *val_decimal(my_decimal *);
  virtual bool str_needs_quotes() { return true; }
  bool compare_str_field_flags(Create_field *new_field, uint32_t flags);
  uint32_t is_equal(Create_field *new_field);
};

#endif
