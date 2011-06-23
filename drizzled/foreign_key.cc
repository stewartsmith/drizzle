/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>

#include <string>

#include <drizzled/foreign_key.h>
#include <drizzled/error.h>
#include <drizzled/create_field.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/table_ident.h>

namespace drizzled
{

extern const charset_info_st *system_charset_info;

void add_foreign_key_to_table_message(
    message::Table *table_message,
    const char* fkey_name,
    List<Key_part_spec> &cols,
    Table_ident *table,
    List<Key_part_spec> &ref_cols,
    message::Table::ForeignKeyConstraint::ForeignKeyOption delete_opt_arg,
    message::Table::ForeignKeyConstraint::ForeignKeyOption update_opt_arg,
    message::Table::ForeignKeyConstraint::ForeignKeyMatchOption match_opt_arg)
{
  message::Table::ForeignKeyConstraint *pfkey= table_message->add_fk_constraint();
  if (fkey_name)
    pfkey->set_name(fkey_name);
  else if (table_message->has_name())
  {
    std::string name(table_message->name());
    char number[20];

    name.append("_ibfk_");
    snprintf(number, sizeof(number), "%d", table_message->fk_constraint_size());
    name.append(number);

    pfkey->set_name(name);
  }

  pfkey->set_match(match_opt_arg);
  pfkey->set_update_option(update_opt_arg);
  pfkey->set_delete_option(delete_opt_arg);

  pfkey->set_references_table_name(table->table.str);

  Key_part_spec *keypart;
  List<Key_part_spec>::iterator col_it(cols.begin());
  while ((keypart= col_it++))
  {
    pfkey->add_column_names(keypart->field_name.str);
  }

  List<Key_part_spec>::iterator ref_it(ref_cols.begin());
  while ((keypart= ref_it++))
  {
    pfkey->add_references_columns(keypart->field_name.str);
  }

}

/**
  Make a deep copy of each list element.

  @note A template function and not a template method of class List
  is employed because of explicit template instantiation:
  in server code there are explicit instantiations of List<T> and
  an explicit instantiation of a template requires that any method
  of the instantiated class used in the template can be resolved.
  Evidently not all template arguments have clone() method with
  the right signature.

  @return You must query the error state in Session for out-of-memory
  situation after calling this function.
*/

template <typename T>
void list_copy_and_replace_each_value(List<T> &list, memory::Root *mem_root)
{
  /* Make a deep copy of each element */
  typename List<T>::iterator it(list.begin());
  while (T* el= it++)
    it.replace(el->clone(mem_root));
}

Foreign_key::Foreign_key(const Foreign_key &rhs, memory::Root *mem_root)
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
    if (b->generated && a->columns.size() > b->columns.size())
      std::swap(a, b);                       // Put shorter key in 'a'
  }
  else
  {
    if (!b->generated)
      return true;                              // No foreign key
    std::swap(a, b);                       // Put generated key in 'a'
  }

  /* Test if 'a' is a prefix of 'b' */
  if (a->columns.size() > b->columns.size())
    return true;                                // Can't be prefix

  List<Key_part_spec>::iterator col_it1(a->columns.begin());
  List<Key_part_spec>::iterator col_it2(b->columns.begin());
  const Key_part_spec *col1, *col2;

#ifdef ENABLE_WHEN_INNODB_CAN_HANDLE_SWAPED_FOREIGN_KEY_COLUMNS
  while ((col1= col_it1++))
  {
    bool found= 0;
    col_it2=b->columns.begin();
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
bool Foreign_key::validate(List<CreateField> &table_fields)
{
  CreateField  *sql_field;
  Key_part_spec *column;
  List<Key_part_spec>::iterator cols(columns.begin());
  List<CreateField>::iterator it(table_fields.begin());
  while ((column= cols++))
  {
    it= table_fields.begin();
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

} /* namespace drizzled */
