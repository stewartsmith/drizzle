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
#include <drizzled/field_iterator.h>
#include <drizzled/table_list.h>
#include <drizzled/session.h>
#include <drizzled/sql_lex.h>
#include <drizzled/table.h>

namespace drizzled {

const char *Field_iterator_table::name()
{
  return (*ptr)->field_name;
}


void Field_iterator_table::set(TableList *table)
{
  ptr= table->table->getFields();
}


void Field_iterator_table::set_table(Table *table)
{
  ptr= table->getFields();
}


Item *Field_iterator_table::create_item(Session *session)
{
  return new Item_field(session, &session->lex().current_select->context, *ptr);
}


void Field_iterator_natural_join::set(TableList *table_ref)
{
  assert(table_ref->join_columns);
  column_ref_it= table_ref->join_columns->begin();
  cur_column_ref= column_ref_it++;
}


void Field_iterator_natural_join::next()
{
  cur_column_ref= column_ref_it++;
  assert(!cur_column_ref || ! cur_column_ref->table_field ||
              cur_column_ref->table_ref->table ==
              cur_column_ref->table_field->getTable());
}


void Field_iterator_table_ref::set_field_iterator()
{
  /*
    If the table reference we are iterating over is a natural join, or it is
    an operand of a natural join, and TableList::join_columns contains all
    the columns of the join operand, then we pick the columns from
    TableList::join_columns, instead of the  orginial container of the
    columns of the join operator.
  */
  if (table_ref->is_join_columns_complete)
  {
    field_it= &natural_join_it;
  }
  /* This is a base table or stored view. */
  else
  {
    assert(table_ref->table);
    field_it= &table_field_it;
  }
  field_it->set(table_ref);
  return;
}


void Field_iterator_table_ref::set(TableList *table)
{
  assert(table);
  first_leaf= table->first_leaf_for_name_resolution();
  last_leaf=  table->last_leaf_for_name_resolution();
  assert(first_leaf && last_leaf);
  table_ref= first_leaf;
  set_field_iterator();
}


void Field_iterator_table_ref::next()
{
  /* Move to the next field in the current table reference. */
  field_it->next();
  /*
    If all fields of the current table reference are exhausted, move to
    the next leaf table reference.
  */
  if (field_it->end_of_fields() && table_ref != last_leaf)
  {
    table_ref= table_ref->next_name_resolution_table;
    assert(table_ref);
    set_field_iterator();
  }
}


const char *Field_iterator_table_ref::table_name()
{
  if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->table_name();

  assert(!strcmp(table_ref->getTableName(),
                 table_ref->table->getShare()->getTableName()));
  return table_ref->getTableName();
}


const char *Field_iterator_table_ref::db_name()
{
  if (table_ref->is_natural_join)
    return natural_join_it.column_ref()->db_name();

  /*
    Test that TableList::db is the same as TableShare::db to
    ensure consistency. 
  */
  assert(!strcmp(table_ref->getSchemaName(), table_ref->table->getShare()->getSchemaName()));
  return table_ref->getSchemaName();
}



/*
  Create new or return existing column reference to a column of a
  natural/using join.

  SYNOPSIS
    Field_iterator_table_ref::get_or_create_column_ref()
    parent_table_ref  the parent table reference over which the
                      iterator is iterating

  DESCRIPTION
    Create a new natural join column for the current field of the
    iterator if no such column was created, or return an already
    created natural join column. The former happens for base tables or
    views, and the latter for natural/using joins. If a new field is
    created, then the field is added to 'parent_table_ref' if it is
    given, or to the original table referene of the field if
    parent_table_ref == NULL.

  NOTES
    This method is designed so that when a Field_iterator_table_ref
    walks through the fields of a table reference, all its fields
    are created and stored as follows:
    - If the table reference being iterated is a stored table, view or
      natural/using join, store all natural join columns in a list
      attached to that table reference.
    - If the table reference being iterated is a nested join that is
      not natural/using join, then do not materialize its result
      fields. This is OK because for such table references
      Field_iterator_table_ref iterates over the fields of the nested
      table references (recursively). In this way we avoid the storage
      of unnecessay copies of result columns of nested joins.

  RETURN
    #     Pointer to a column of a natural join (or its operand)
    NULL  No memory to allocate the column
*/

Natural_join_column *
Field_iterator_table_ref::get_or_create_column_ref(TableList *parent_table_ref)
{
  Natural_join_column *nj_col;
  bool is_created= true;
  uint32_t field_count=0;
  TableList *add_table_ref= parent_table_ref ?
                             parent_table_ref : table_ref;

  if (field_it == &table_field_it)
  {
    /* The field belongs to a stored table. */
    Field *tmp_field= table_field_it.field();
    nj_col= new Natural_join_column(tmp_field, table_ref);
    field_count= table_ref->table->getShare()->sizeFields();
  }
  else
  {
    /*
      The field belongs to a NATURAL join, therefore the column reference was
      already created via one of the two constructor calls above. In this case
      we just return the already created column reference.
    */
    assert(table_ref->is_join_columns_complete);
    is_created= false;
    nj_col= natural_join_it.column_ref();
    assert(nj_col);
  }
  assert(!nj_col->table_field ||
              nj_col->table_ref->table == nj_col->table_field->getTable());

  /*
    If the natural join column was just created add it to the list of
    natural join columns of either 'parent_table_ref' or to the table
    reference that directly contains the original field.
  */
  if (is_created)
  {
    /* Make sure not all columns were materialized. */
    assert(!add_table_ref->is_join_columns_complete);
    if (!add_table_ref->join_columns)
    {
      /* Create a list of natural join columns on demand. */
      add_table_ref->join_columns= new List<Natural_join_column>;
      add_table_ref->is_join_columns_complete= false;
    }
    add_table_ref->join_columns->push_back(nj_col);
    /*
      If new fields are added to their original table reference, mark if
      all fields were added. We do it here as the caller has no easy way
      of knowing when to do it.
      If the fields are being added to parent_table_ref, then the caller
      must take care to mark when all fields are created/added.
    */
    if (!parent_table_ref &&
        add_table_ref->join_columns->size() == field_count)
      add_table_ref->is_join_columns_complete= true;
  }

  return nj_col;
}


/*
  Return an existing reference to a column of a natural/using join.

  SYNOPSIS
    Field_iterator_table_ref::get_natural_column_ref()

  DESCRIPTION
    The method should be called in contexts where it is expected that
    all natural join columns are already created, and that the column
    being retrieved is a Natural_join_column.

  RETURN
    #     Pointer to a column of a natural join (or its operand)
    NULL  No memory to allocate the column
*/

Natural_join_column *
Field_iterator_table_ref::get_natural_column_ref()
{
  Natural_join_column *nj_col;

  assert(field_it == &natural_join_it);
  /*
    The field belongs to a NATURAL join, therefore the column reference was
    already created via one of the two constructor calls above. In this case
    we just return the already created column reference.
  */
  nj_col= natural_join_it.column_ref();
  assert(nj_col &&
              (!nj_col->table_field ||
               nj_col->table_ref->table == nj_col->table_field->getTable()));
  return nj_col;
}

} /* namespace drizzled */
