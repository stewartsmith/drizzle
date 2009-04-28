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

#include "drizzled/global.h"
#include "drizzled/server_includes.h" /* @TODO remove this when header include is refactored more... */
#include "drizzled/foreign_key.h"
#include "drizzled/error.h"

Foreign_key::Foreign_key(const Foreign_key &rhs, MEM_ROOT *mem_root)
  :Key(rhs),
  ref_table(rhs.ref_table),
  ref_columns(rhs.ref_columns),
  delete_opt(rhs.delete_opt),
  update_opt(rhs.update_opt),
  match_opt(rhs.match_opt)
{
  list_copy_and_replace_each_value(ref_columns, mem_root);
}

/*
  Test if a foreign key (= generated key) is a prefix of the given key
  (ignoring key name, key type and order of columns)

  NOTES:
    This is only used to test if an index for a FOREIGN KEY exists

  IMPLEMENTATION
    We only compare field names

  RETURN
    0	Generated key is a prefix of other key
    1	Not equal
*/
bool foreign_key_prefix(Key *a, Key *b)
{
  /* Ensure that 'a' is the generated key */
  if (a->generated)
  {
    if (b->generated && a->columns.elements > b->columns.elements)
      std::swap(a, b);                       // Put shorter key in 'a'
  }
  else
  {
    if (!b->generated)
      return true;                              // No foreign key
    std::swap(a, b);                       // Put generated key in 'a'
  }

  /* Test if 'a' is a prefix of 'b' */
  if (a->columns.elements > b->columns.elements)
    return true;                                // Can't be prefix

  List_iterator<Key_part_spec> col_it1(a->columns);
  List_iterator<Key_part_spec> col_it2(b->columns);
  const Key_part_spec *col1, *col2;

#ifdef ENABLE_WHEN_INNODB_CAN_HANDLE_SWAPED_FOREIGN_KEY_COLUMNS
  while ((col1= col_it1++))
  {
    bool found= 0;
    col_it2.rewind();
    while ((col2= col_it2++))
    {
      if (*col1 == *col2)
      {
        found= true;
	break;
      }
    }
    if (!found)
      return true;                              // Error
  }
  return false;                                 // Is prefix
#else
  while ((col1= col_it1++))
  {
    col2= col_it2++;
    if (!(*col1 == *col2))
      return true;
  }
  return false;                                 // Is prefix
#endif
}

/*
  Check if the foreign key options are compatible with columns
  on which the FK is created.

  RETURN
    0   Key valid
    1   Key invalid
*/
bool Foreign_key::validate(List<Create_field> &table_fields)
{
  Create_field  *sql_field;
  Key_part_spec *column;
  List_iterator<Key_part_spec> cols(columns);
  List_iterator<Create_field> it(table_fields);
  while ((column= cols++))
  {
    it.rewind();
    while ((sql_field= it++) &&
           my_strcasecmp(system_charset_info,
                         column->field_name.str,
                         sql_field->field_name)) {}
    if (!sql_field)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
      return true;
    }
  }
  return false;
}



