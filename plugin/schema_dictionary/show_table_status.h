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

#ifndef PLUGIN_SCHEMA_DICTIONARY_SHOW_TABLE_STATUS_H
#define PLUGIN_SCHEMA_DICTIONARY_SHOW_TABLE_STATUS_H

class ShowTableStatus : public ShowTables
{
public:
  ShowTableStatus() :
    ShowTables("SHOW_TABLE_STATUS")
  {
    add_field("Name");
    add_field("Engine");
    add_field("Version");
    add_field("Row_format");
    add_field("Rows");
    add_field("Avg_row_length");
    add_field("Data_length");
    add_field("Max_data_length");
    add_field("Index_length");
    add_field("Data_free");
    add_field("Auto_increment");
    add_field("Create_time");
    add_field("Update_time");
    add_field("Check_time");
    add_field("Collation");
    add_field("Checksum");
    add_field("Create_options");
    add_field("Comment");
  }

  class Generator : public ShowTables::Generator 
  {
    void fill()
    {
      /* Name */
      push(table_name());

      /* Engine */
      push(getTableProto().engine().name());

      /* Version */
      push(static_cast<int64_t>(0));

      /* Row_format */
      pushRow(getTableProto().options().row_type());

      /* Rows */
      push(static_cast<int64_t>(0));

      /* Avg_row_length */
      push(static_cast<int64_t>(0));

      /* Data_length */
      push(static_cast<int64_t>(0));

      /* Max_data_length */
      push(static_cast<int64_t>(0));

      /* Index_length */
      push(static_cast<int64_t>(0));

      /* Data_free */
      push(static_cast<int64_t>(0));

      /* Auto_increment */
      push(static_cast<int64_t>(0));

      /* Create_time */
      push(static_cast<int64_t>(0));

      /* Update_time */
      push(static_cast<int64_t>(0));

      /* Check_time */
      push(static_cast<int64_t>(0));

      /* Collation */
      push(getTableProto().options().collation());

      /* Checksum */
      push(static_cast<int64_t>(0));

      /* Create_options */
      push("");

      /* Comment */
      push("");
    }

  public:
    Generator(drizzled::Field **arg) :
      ShowTables::Generator(arg)
    { }
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

#endif /* PLUGIN_SCHEMA_DICTIONARY_SHOW_TABLE_STATUS_H */
