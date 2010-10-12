/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* Structs that defines the Table */

#ifndef DRIZZLED_TABLE_SHARE_INSTANCE_H
#define DRIZZLED_TABLE_SHARE_INSTANCE_H

namespace drizzled
{

class TableShareInstance : public Table
{
  TableShare _share;
  bool _has_variable_width;

public:
  TableShareInstance(TableIdentifier::Type type_arg) :
    _share(type_arg),
    _has_variable_width(false)
  {
  }

  Table *getTable()
  {
    return this;
  }

  TableShare *getMutableShare(void)
  {
    return &_share;
  }

  const TableShare *getShare(void) const
  {
    return &_share;
  }

  bool hasVariableWidth() const
  {
    return _has_variable_width;
  }

  void setVariableWidth()
  {
    _has_variable_width= true;
  }

  ~TableShareInstance()
  {
    this->free_tmp_table(this->in_use);
  }
};

} /* namespace drizzled */

#endif /* DRIZZLED_TABLE_SHARE_INSTANCE_H */
