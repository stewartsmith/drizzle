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

#include <drizzled/item_result.h>

namespace drizzled {

/**
 * A structure used to describe sort information
 * for a field or item used in ORDER BY.
 */
class SortField 
{
public:
  Field *field;	/**< Field to sort */
  Item	*item; /**< Item if not sorting fields */
  size_t length; /**< Length of sort field */
  uint32_t suffix_length; /**< Length suffix (0-4) */
  Item_result result_type; /**< Type of item */
  bool reverse; /**< if descending sort */
  bool need_strxnfrm;	/**< If we have to use strxnfrm() */

  SortField() :
    field(0),
    item(0),
    length(0),
    suffix_length(0),
    result_type(STRING_RESULT),
    reverse(0),
    need_strxnfrm(0)
  { }

};

} /* namespace drizzled */

