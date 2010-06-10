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

#ifndef DRIZZLED_TABLE_PLACEHOLDER_H
#define DRIZZLED_TABLE_PLACEHOLDER_H

namespace drizzled
{

class TablePlaceholder : public Table
{
  TableShare private_share;
  std::vector<char> key_buff;

public:
  TablePlaceholder(const char *key, uint32_t key_length) :
    Table()
  {
    is_placeholder_created= true;
    setShare(&private_share);

    key_buff.resize(key_length);

    memcpy(&key_buff[0], key, key_length);
    getMutableShare()->set_table_cache_key(&key_buff[0], key_length);
    getMutableShare()->setType(message::Table::INTERNAL);  // for intern_close_table
    locked_by_name= true;
  }
};

} /* namespace drizzled */

#endif /* DRIZZLED_TABLE_PLACEHOLDER_H */
