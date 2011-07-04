/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#pragma once

class ShowColumns : public show_dictionary::Show
{
public:
  ShowColumns();

  bool visible() const { return false; }

  class Generator : public show_dictionary::Show::Generator 
  {
    bool is_tables_primed;
    bool is_columns_primed;

    int32_t column_iterator;
    drizzled::message::table::shared_ptr table_proto;
    drizzled::message::Table::Field column;

    bool nextColumnCore();
    bool nextColumn();

    std::string table_name;

    drizzled::message::table::shared_ptr getTableProto()
    {
      return table_proto;
    }

    bool isTablesPrimed()
    {
      return is_tables_primed;
    }

    const std::string &getTableName()
    {
      return table_name;
    }

    void pushType(drizzled::message::Table::Field::FieldType type, const std::string &collation);

    void fill();

  public:
    Generator(drizzled::Field **arg);
    bool populate();

  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

