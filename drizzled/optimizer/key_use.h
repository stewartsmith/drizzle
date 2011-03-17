/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

namespace drizzled
{
namespace optimizer
{

class KeyUse 
{
public:

  KeyUse()
    :
      table(NULL),
      val(NULL),
      used_tables(0),
      key(0),
      keypart(0),
      optimize(0),
      keypart_map(0),
      ref_table_rows(0),
      null_rejecting(false),
      cond_guard(NULL)
  {}

  KeyUse(Table *in_table,
         Item *in_val,
         table_map in_used_tables,
         uint32_t in_key,
         uint32_t in_keypart,
         uint32_t in_optimize,
         key_part_map in_keypart_map,
         ha_rows in_ref_table_rows,
         bool in_null_rejecting,
         bool *in_cond_guard)
    :
      table(in_table),
      val(in_val),
      used_tables(in_used_tables),
      key(in_key),
      keypart(in_keypart),
      optimize(in_optimize),
      keypart_map(in_keypart_map),
      ref_table_rows(in_ref_table_rows),
      null_rejecting(in_null_rejecting),
      cond_guard(in_cond_guard)
  {}

  Table *getTable()
  {
    return table;
  }

  Item *getVal()
  {
    return val;
  }

  table_map getUsedTables()
  {
    return used_tables;
  }

  uint32_t getKey() const
  {
    return key;
  }

  uint32_t getKeypart() const
  {
    return keypart;
  }

  uint32_t getOptimizeFlags() const
  {
    return optimize;
  }

  key_part_map getKeypartMap()
  {
    return keypart_map;
  }

  ha_rows getTableRows() const
  {
    return ref_table_rows;
  }

  void setTableRows(ha_rows input)
  {
    ref_table_rows= input;
  }

  bool isNullRejected() const
  {
    return null_rejecting;
  }

  bool *getConditionalGuard()
  {
    return cond_guard;
  }

private:

  Table *table; /**< Pointer to the table this key belongs to */

  Item *val;	/**< or value if no field */

  table_map used_tables;

  uint32_t key;

  uint32_t keypart;

  uint32_t optimize; /**< 0, or KEY_OPTIMIZE_* */

  key_part_map keypart_map;

  ha_rows ref_table_rows;

  /**
   * If true, the comparison this value was created from will not be
   * satisfied if val has NULL 'value'.
   */
  bool null_rejecting;

  /**
   * !NULL - This KeyUse was created from an equality that was wrapped into
   *         an Item_func_trig_cond. This means the equality (and validity of
   *         this KeyUse element) can be turned on and off. The on/off state
   *         is indicted by the pointed value:
   *           *cond_guard == true <=> equality condition is on
   *           *cond_guard == false <=> equality condition is off
   *
   * NULL  - Otherwise (the source equality can't be turned off)
  */
  bool *cond_guard;
};

} /* end namespace optimizer */

} /* end namespace drizzled */

