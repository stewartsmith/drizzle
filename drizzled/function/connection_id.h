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

#ifndef DRIZZLED_FUNCTION_CONNECTION_ID_H
#define DRIZZLED_FUNCTION_CONNECTION_ID_H

#include <drizzled/function/math/int.h>
#include <drizzled/function/func.h>

class Item_func_connection_id :public Item_int_func
{
  int64_t value;

public:
  Item_func_connection_id() {}
  const char *func_name() const { return "connection_id"; }
  void fix_length_and_dec();
  bool fix_fields(Session *session, Item **ref);
  int64_t val_int() { assert(fixed == 1); return value; }
  bool check_vcol_func_processor(unsigned char *)
  { return true; }
};

#endif /* DRIZZLED_FUNCTION_CONNECTION_ID_H */
