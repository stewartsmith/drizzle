/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_VIRTUAL_COLUMN_INFO_H
#define DRIZZLED_VIRTUAL_COLUMN_INFO_H

#include <drizzled/definitions.h>
#include <drizzled/sql_alloc.h>
#include <drizzled/lex_string.h>

class Item;

class virtual_column_info: public Sql_alloc
{
public:
  Item *expr_item;
  LEX_STRING expr_str;
  Item *item_free_list;

  virtual_column_info()
  : expr_item(0), item_free_list(0),
    field_type(DRIZZLE_TYPE_VIRTUAL),
    is_stored(false), data_inited(false)
  {
    expr_str.str= NULL;
    expr_str.length= 0;
  };
  ~virtual_column_info() {}
  enum_field_types get_real_type();
  void set_field_type(enum_field_types fld_type);
  bool get_field_stored();
  void set_field_stored(bool stored);

private:
  /*
    The following data is only updated by the parser and read
    when a Create_field object is created/initialized.
  */
  enum_field_types field_type;   /* Real field type*/
  bool is_stored;             /* Indication that the field is
                                 phisically stored in the database*/
  /*
    This flag is used to prevent other applications from
    reading and using incorrect data.
  */
  bool data_inited;
};

#endif /* DRIZZLED_VIRTUAL_COLUMN_INFO_H */
