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

/**
 * SARG stands for search argument. A sargable predicate is one of the form
 * (or which can be put in to the form) "column comparison-operator value".
 * SARGS are expressed as a boolean expression of such predicates in
 * disjunctive normal form. For more information, consult the original paper
 * in which this term was introduced: Access Path Selection in a Relational
 * Database Management System by Selinger et al
 *
 * This class is used to collect info on potentially sargable predicates in
 * order to check whether they become sargable after reading const tables.
 * We form a bitmap of indexes that can be used for sargable predicates.
 * Only such indexes are involved in range analysis.
 */
class SargableParam
{
public:
  SargableParam()
    :
      field(NULL),
      arg_value(NULL),
      num_values(0)
  {}

  SargableParam(Field *in_field,
                Item **in_arg_value,
                uint32_t in_num_values)
    :
      field(in_field),
      arg_value(in_arg_value),
      num_values(in_num_values)
  {}

  SargableParam(const SargableParam &rhs)
    :
      field(rhs.field),
      arg_value(rhs.arg_value),
      num_values(rhs.num_values)
  {}

  SargableParam &operator=(const SargableParam &rhs)
  {
    if (this == &rhs)
    {
      return *this;
    }
    field= rhs.field;
    arg_value= rhs.arg_value;
    num_values= rhs.num_values;
    return *this;
  }

  Field *getField()
  {
    return field;
  }

  uint32_t getNumValues() const
  {
    return num_values;
  }

  bool isConstItem(uint32_t index)
  {
    return (arg_value[index]->const_item());
  }

private:

  /**
   * Field agsinst which to check sargability.
   */
  Field *field;

  /**
   * Values of potential keys for lookups.
   */
  Item **arg_value;

  /**
   * Number of values in the arg_value array.
   */
  uint32_t num_values;
};

} /* end namespace optimizer */

} /* end namespace drizzled */

