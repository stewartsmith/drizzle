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

#include <drizzled/join_table.h>

namespace drizzled
{
namespace optimizer
{

/**
 * Information about a position of table within a join order. Used in join
 * optimization.
 */
class Position
{
public:

  Position()
    :
      records_read(0),
      read_time(0),
      table(NULL),
      key(NULL),
      ref_depend_map(0)
  {}

  Position(double in_records_read,
           double in_read_time,
           JoinTable *in_table,
           KeyUse *in_key,
           table_map in_ref_depend_map)
    :
      records_read(in_records_read),
      read_time(in_read_time),
      table(in_table),
      key(in_key),
      ref_depend_map(in_ref_depend_map)
  {}

  Position(const Position &rhs)
    :
      records_read(rhs.records_read),
      read_time(rhs.read_time),
      table(rhs.table),
      key(rhs.key),
      ref_depend_map(rhs.ref_depend_map)
  {}

  Position &operator=(const Position &rhs)
  {
    if (this == &rhs)
    {
      return *this;
    }
    records_read= rhs.records_read;
    read_time= rhs.read_time;
    table= rhs.table;
    key= rhs.key;
    ref_depend_map= rhs.ref_depend_map;
    return *this;
  }

  /**
   * Determine whether the table this particular position is representing in
   * the query plan is a const table or not. A constant table is defined as
   * (taken from the MySQL optimizer internals document on MySQL forge):
   *
   * 1) A table with zero rows, or with only one row
   * 2) A table expression that is restricted with a WHERE condition
   *
   * Based on the definition above, when records_read is set to 1.0 in the
   * Position class, it infers that this position in the partial plan
   * represents a const table.
   *
   * @return true if this position represents a const table; false otherwise
   */
  bool isConstTable() const
  {
    return (records_read < 2.0);
  }

  double getFanout() const
  {
    return records_read;
  }

  void setFanout(double in_records_read)
  {
    records_read= in_records_read;
  }

  double getCost() const
  {
    return read_time;
  }

  JoinTable *getJoinTable()
  {
    return table;
  }

  /**
   * Check to see if the table attached to the JoinTable for this position
   * has an index that can produce an ordering. 
   *
   * @return true if the table attached to the JoinTable for this position
   * does not have an index that can produce an ordering; false otherwise
   */
  bool hasTableForSorting(Table *cmp_table) const
  {
    return (cmp_table != table->table);
  }

  bool examinePosition(table_map found_ref);

  KeyUse *getKeyUse()
  {
    return key;
  }

  table_map getRefDependMap()
  {
    return ref_depend_map;
  }

  void clearRefDependMap()
  {
    ref_depend_map= 0;
  }

private:

  /**
    The "fanout": number of output rows that will be produced (after
    pushed down selection condition is applied) per each row combination of
    previous tables. The value is an in-precise estimate.
  */
  double records_read;

  /**
    Cost accessing the table in course of the entire complete join execution,
    i.e. cost of one access method use (e.g. 'range' or 'ref' scan ) times
    number the access method will be invoked.
  */
  double read_time;

  JoinTable *table;

  /**
    NULL  -  'index' or 'range' or 'index_merge' or 'ALL' access is used.
    Other - [eq_]ref[_or_null] access is used. Pointer to {t.keypart1 = expr}
  */
  KeyUse *key;

  /** If ref-based access is used: bitmap of tables this table depends on  */
  table_map ref_depend_map;

};

} /* end namespace optimizer */

} /* end namespace drizzled */

