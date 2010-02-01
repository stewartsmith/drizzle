/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include <plugin/data_engine/function.h>

void Tool::add_field(const char *label,
                     uint32_t field_length)
{
  add_field(label, Tool::STRING, field_length);
}

void Tool::add_field(const char *label,
                     Tool::ColumnType type,
                     bool is_default_null)
{
  add_field(label, type, 5, is_default_null);
}

void Tool::add_field(const char *label,
                     Tool::ColumnType type,
                     uint32_t field_length,
                     bool is_default_null)
{
  drizzled::message::Table::Field *field;
  drizzled::message::Table::Field::FieldOptions *field_options;
  drizzled::message::Table::Field::FieldConstraints *field_constraints;

  field= proto.add_field();
  field->set_name(label);

  field_options= field->mutable_options();
  field_constraints= field->mutable_constraints();
  field_options->set_default_null(is_default_null);
  field_constraints->set_is_nullable(is_default_null);

  switch (type) 
  {
  default:
  case Tool::BOOLEAN:
    field_length= 5;
  case Tool::STRING:
    drizzled::message::Table::Field::StringFieldOptions *string_field_options;
    field->set_type(drizzled::message::Table::Field::VARCHAR);

    string_field_options= field->mutable_string_options();
    string_field_options->set_length(field_length);

    break;
  case Tool::NUMBER:
    field->set_type(drizzled::message::Table::Field::BIGINT);
    break;
  }
}

void Tool::add_field(const char *label,
                     drizzled::message::Table::Field::FieldType type,
                     uint32_t length)
{
  drizzled::message::Table::Field *field;
  drizzled::message::Table::Field::FieldOptions *field_options;
  drizzled::message::Table::Field::FieldConstraints *field_constraints;

  field= proto.add_field();
  field->set_name(label);
  field->set_type(type);

  field_options= field->mutable_options();
  field_constraints= field->mutable_constraints();
  field_options->set_default_null(false);
  field_constraints->set_is_nullable(false);

  switch (type) 
  {
  case drizzled::message::Table::Field::VARCHAR:
    {
      drizzled::message::Table::Field::StringFieldOptions *string_field_options;

      string_field_options= field->mutable_string_options();
      string_field_options->set_length(length);

      break;
    }
  default:
    break;
  }
}
