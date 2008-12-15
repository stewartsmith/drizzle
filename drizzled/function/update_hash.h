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

#ifndef DRIZZLED_FUNCTION_UPDATE_HASH_H
#define DRIZZLED_FUNCTION_UPDATE_HASH_H

#include <drizzled/function/func.h>

bool
update_hash(
  user_var_entry *entry,
  bool set_null,
  void *ptr,
  uint32_t length,
  Item_result type,
  const CHARSET_INFO * const cs,
  Derivation dv,
  bool unsigned_arg
  );


#endif /* DRIZZLED_FUNCTION_UPDATE_HASH_H */
