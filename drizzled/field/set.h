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

#ifndef DRIZZLE_SERVER_FIELD_SET
#define DRIZZLE_SERVER_FIELD_SET

#include "mysql_priv.h"

class Field_set :public Field_enum {
public:
  Field_set(uchar *ptr_arg, uint32_t len_arg, uchar *null_ptr_arg,
	    uchar null_bit_arg,
	    enum utype unireg_check_arg, const char *field_name_arg,
	    uint32_t packlength_arg,
	    TYPELIB *typelib_arg, CHARSET_INFO *charset_arg)
    :Field_enum(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
		    unireg_check_arg, field_name_arg,
                packlength_arg,
                typelib_arg,charset_arg)
    {
      flags=(flags & ~ENUM_FLAG) | SET_FLAG;
    }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr) { return Field_set::store((int64_t) nr, false); }
  int  store(int64_t nr, bool unsigned_val);

  virtual bool zero_pack() const { return 1; }
  String *val_str(String*,String *);
  void sql_type(String &str) const;
  enum_field_types real_type() const { return DRIZZLE_TYPE_SET; }
  bool has_charset(void) const { return true; }
};

#endif

