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

#ifndef DRIZZLE_SERVER_FIELD_NULL
#define DRIZZLE_SERVER_FIELD_NULL

/* 
  Everything saved in this will disappear. It will always return NULL 
 */

class Field_null :public Field_str {
  static uchar null[1];
public:
  Field_null(uchar *ptr_arg, uint32_t len_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     CHARSET_INFO *cs)
    :Field_str(ptr_arg, len_arg, null, 1,
	       unireg_check_arg, field_name_arg, cs)
    {}
  enum_field_types type() const { return DRIZZLE_TYPE_NULL;}
  int  store(const char *to __attribute__((unused)),
             uint length __attribute__((unused)),
             CHARSET_INFO *cs __attribute__((unused)))
  { null[0]=1; return 0; }
  int store(double nr __attribute__((unused)))
  { null[0]=1; return 0; }
  int store(int64_t nr __attribute__((unused)),
            bool unsigned_val __attribute__((unused)))
  { null[0]=1; return 0; }
  int store_decimal(const my_decimal *d __attribute__((unused)))
  { null[0]=1; return 0; }
  int reset(void)
  { return 0; }
  double val_real(void)
  { return 0.0;}
  int64_t val_int(void)
  { return 0;}
  my_decimal *val_decimal(my_decimal *) { return 0; }
  String *val_str(String *value __attribute__((unused)),
                  String *value2)
  { value2->length(0); return value2;}
  int cmp(const uchar *a __attribute__((unused)),
          const uchar *b __attribute__((unused))) { return 0;}
  void sort_string(uchar *buff __attribute__((unused)),
                   uint length __attribute__((unused)))  {}
  uint32_t pack_length() const { return 0; }
  void sql_type(String &str) const;
  uint size_of() const { return sizeof(*this); }
  uint32_t max_display_length() { return 4; }
};

#endif

