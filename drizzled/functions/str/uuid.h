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

#ifndef DRIZZLED_FUNCTIONS_STR_UUID_H
#define DRIZZLED_FUNCTIONS_STR_UUID_H

#include <drizzled/functions/str/strfunc.h> 

#define UUID_LENGTH (8+1+4+1+4+1+4+1+12)
class Item_func_uuid: public Item_str_func
{
public:
  Item_func_uuid(): Item_str_func() {}
  void fix_length_and_dec() {
    collation.set(system_charset_info);
    /*
       NOTE! uuid() should be changed to use 'ascii'
       charset when hex(), format(), md5(), etc, and implicit
       number-to-string conversion will use 'ascii'
    */
    max_length= UUID_LENGTH * system_charset_info->mbmaxlen;
  }
  const char *func_name() const{ return "uuid"; }
  String *val_str(String *);
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

#endif /* DRIZZLED_FUNCTIONS_STR_UUID_H */
