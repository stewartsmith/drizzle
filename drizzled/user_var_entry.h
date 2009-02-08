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


#ifndef DRIZZLED_USER_VAR_ENTRY_H
#define DRIZZLED_USER_VAR_ENTRY_H


// this is needed for user_vars hash
class user_var_entry
{
 public:
  user_var_entry() {}                         /* Remove gcc warning */
  LEX_STRING name;
  char *value;
  ulong length;
  query_id_t update_query_id, used_query_id;
  Item_result type;
  bool unsigned_flag;

  double val_real(bool *null_value);
  int64_t val_int(bool *null_value) const;
  String *val_str(bool *null_value, String *str, uint32_t decimals);
  my_decimal *val_decimal(bool *null_value, my_decimal *result);
  DTCollation collation;
};

#endif /* DRIZZLED_USER_VAR_ENTRY_H */
