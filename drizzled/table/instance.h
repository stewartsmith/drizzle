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

#ifndef DRIZZLED_TABLE_INSTANCE_H
#define DRIZZLED_TABLE_INSTANCE_H

namespace drizzled
{

namespace table
{

class Instance : public Table
{
  TableShare _share;
  bool _has_variable_width;

public:
  Instance() :
    _share(message::Table::INTERNAL),
    _has_variable_width(false)
  {
  }

  TableShare *getMutableShare(void)
  {
    return &_share;
  }

  void setShare(TableShare *)
  {
    assert(0);
  }

  const TableShare *getShare(void) const
  {
    return &_share;
  }

  bool hasVariableWidth() const
  {
    return _has_variable_width;
  }

  bool create_myisam_tmp_table(KeyInfo *keyinfo,
                               MI_COLUMNDEF *start_recinfo,
                               MI_COLUMNDEF **recinfo,
                               uint64_t options);
  void setup_tmp_table_column_bitmaps();
  void free_tmp_table(Session *session);
  bool open_tmp_table();

  void setVariableWidth()
  {
    _has_variable_width= true;
  }

  ~Instance();
};

} /* namespace table */
} /* namespace drizzled */

#endif /* DRIZZLED_TABLE_INSTANCE_H */
