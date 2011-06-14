/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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


#pragma once

#include <drizzled/dtcollation.h>
#include <drizzled/item_result.h>

namespace drizzled {

class user_var_entry
{
 public:
  user_var_entry(const char *arg, query_id_t id) :
    value(0),
    length(0),
    size(0),
    update_query_id(0),
    used_query_id(id),
    type(STRING_RESULT),
    unsigned_flag(false),
    collation(NULL, DERIVATION_IMPLICIT)
  { 
    name.str= strdup(arg);
    name.length= strlen(arg);
  }

  ~user_var_entry()
  {
    free(name.str);
    free(value);
  }

  LEX_STRING name;
  char *value;
  ulong length;
  size_t size;
  query_id_t update_query_id;
  query_id_t used_query_id;
  Item_result type;
  bool unsigned_flag;

  double val_real(bool *null_value);
  int64_t val_int(bool *null_value) const;
  String *val_str(bool *null_value, String *str, uint32_t decimals);
  type::Decimal *val_decimal(bool *null_value, type::Decimal *result);
  DTCollation collation;

  void update_hash(bool set_null, void *ptr, uint32_t length,
                   Item_result type, const charset_info_st * const cs, Derivation dv,
                   bool unsigned_arg);
};

} /* namespace drizzled */

