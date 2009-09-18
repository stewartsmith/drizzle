/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_OPTIMIZER_KEY_USE_H
#define DRIZZLED_OPTIMIZER_KEY_USE_H

#include <drizzled/server_includes.h>

#include <vector>

namespace drizzled
{
namespace optimizer
{

class KeyUse 
{
public:
  Table *table; /**< Pointer to the table this key belongs to */
  Item *val;	/**< or value if no field */
  table_map used_tables;
  uint32_t key;
  uint32_t keypart;
  uint32_t optimize; /**< 0, or KEY_OPTIMIZE_* */
  key_part_map keypart_map;
  ha_rows ref_table_rows;
  /**
    If true, the comparison this value was created from will not be
    satisfied if val has NULL 'value'.
  */
  bool null_rejecting;
  /**
    !NULL - This KeyUse was created from an equality that was wrapped into
            an Item_func_trig_cond. This means the equality (and validity of
            this KeyUse element) can be turned on and off. The on/off state
            is indicted by the pointed value:
              *cond_guard == true <=> equality condition is on
              *cond_guard == false <=> equality condition is off

    NULL  - Otherwise (the source equality can't be turned off)
  */
  bool *cond_guard;
};

} /* end namespace optimizer */

} /* end namespace drizzled */

#endif /* DRIZZLED_OPTIMIZER_KEY_USE_H */
