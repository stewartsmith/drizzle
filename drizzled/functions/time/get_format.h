/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef DRIZZLED_FUNCTIONS_TIME_GET_FORMAT_H
#define DRIZZLED_FUNCTIONS_TIME_GET_FORMAT_H

enum date_time_format
{
  USA_FORMAT, JIS_FORMAT, ISO_FORMAT, EUR_FORMAT, INTERNAL_FORMAT
};

class Item_func_get_format :public Item_str_func
{
public:
  const enum enum_drizzle_timestamp_type type; // keep it public
  Item_func_get_format(enum enum_drizzle_timestamp_type type_arg, Item *a)
    :Item_str_func(a), type(type_arg)
  {}
  String *val_str(String *str);
  const char *func_name() const { return "get_format"; }
  void fix_length_and_dec()
  {
    maybe_null= 1;
    decimals=0;
    max_length=17*MY_CHARSET_BIN_MB_MAXLEN;
  }
  virtual void print(String *str, enum_query_type query_type);
};



#endif /* DRIZZLED_FUNCTIONS_TIME_GET_FORMAT_H */
