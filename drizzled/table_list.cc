/* Copyright (C) 2009 Sun Microsystems

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <drizzled/error.h>
#include <drizzled/table_list.h>
#include <drizzled/item.h>
#include <drizzled/item/field.h>
#include <drizzled/nested_join.h>
#include <drizzled/sql_lex.h>
#include <drizzled/server_includes.h>
#include <drizzled/sql_select.h>

#include <string>

using namespace std;

class Item;
class Item_field;

/*
  Create a table cache key

  SYNOPSIS
  create_table_def_key()
  key			Create key here (must be of size MAX_DBKEY_LENGTH)
  table_list		Table definition

  IMPLEMENTATION
  The table cache_key is created from:
  db_name + \0
  table_name + \0

  if the table is a tmp table, we add the following to make each tmp table
  unique on the slave:

  4 bytes for master thread id
  4 bytes pseudo thread id

  RETURN
  Length of key
*/

uint32_t TableList::create_table_def_key(char *key)
{
  uint32_t key_length;
  char *key_pos= key;

  key_pos= strcpy(key_pos, db) + strlen(db);
  key_pos= strcpy(key_pos+1, table_name) +
    strlen(table_name);
  key_length= (uint32_t)(key_pos-key)+1;

  return key_length;
}


/*
  Set insert_values buffer

  SYNOPSIS
    set_insert_values()
    mem_root   memory pool for allocating

  RETURN
    false - OK
    TRUE  - out of memory
*/

bool TableList::set_insert_values(MEM_ROOT *mem_root)
{
  if (table)
  {
    if (!table->insert_values &&
        !(table->insert_values= (unsigned char *)alloc_root(mem_root,
                                                   table->s->rec_buff_length)))
      return true;
  }

  return false;
}


/*
  Test if this is a leaf with respect to name resolution.

  SYNOPSIS
    TableList::is_leaf_for_name_resolution()

  DESCRIPTION
    A table reference is a leaf with respect to name resolution if
    it is either a leaf node in a nested join tree (table, view,
    schema table, subquery), or an inner node that represents a
    NATURAL/USING join, or a nested join with materialized join
    columns.

  RETURN
    TRUE if a leaf, false otherwise.
*/
bool TableList::is_leaf_for_name_resolution()
{
  return (is_natural_join || is_join_columns_complete || !nested_join);
}



/*
  Create Item_field for each column in the table.

  SYNPOSIS
    Table::fill_item_list()
      item_list          a pointer to an empty list used to store items

  DESCRIPTION
    Create Item_field object for each column in the table and
    initialize it with the corresponding Field. New items are
    created in the current Session memory root.

  RETURN VALUE
    0                    success
    1                    out of memory
*/

bool Table::fill_item_list(List<Item> *item_list) const
{
  /*
    All Item_field's created using a direct pointer to a field
    are fixed in Item_field constructor.
  */
  for (Field **ptr= field; *ptr; ptr++)
  {
    Item_field *item= new Item_field(*ptr);
    if (!item || item_list->push_back(item))
      return true;
  }
  return false;
}


/*
  Find underlying base tables (TableList) which represent given
  table_to_find (Table)

  SYNOPSIS
    TableList::find_underlying_table()
    table_to_find table to find

  RETURN
    0  table is not found
    found table reference
*/

TableList *TableList::find_underlying_table(Table *table_to_find)
{
  /* is this real table and table which we are looking for? */
  if (table == table_to_find)
    return this;

  return NULL;
}


bool TableList::placeholder()
{
  return derived || schema_table || (create && !table->getDBStat()) || !table;
}


/*
  Retrieve the last (right-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    TableList::last_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the right-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The right-most child of a nested table reference is the first
    element in the list of children because the children are inserted
    in reverse order.

  RETURN
    - If 'this' is a nested table reference - the right-most child of
      the tree rooted in 'this',
    - else - 'this'
*/

TableList *TableList::last_leaf_for_name_resolution()
{
  TableList *cur_table_ref= this;
  nested_join_st *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  assert(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    cur_table_ref= cur_nested_join->join_list.head();
    /*
      If the current nested is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the last operand is in the
      end of the list.
    */
    if ((cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      List_iterator_fast<TableList> it(cur_nested_join->join_list);
      TableList *next;
      cur_table_ref= it++;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


/*
  Retrieve the first (left-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    TableList::first_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the left-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The left-most child of a nested table reference is the last element
    in the list of children because the children are inserted in
    reverse order.

  RETURN
    If 'this' is a nested table reference - the left-most child of
      the tree rooted in 'this',
    else return 'this'
*/

TableList *TableList::first_leaf_for_name_resolution()
{
  TableList *cur_table_ref= NULL;
  nested_join_st *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  assert(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    List_iterator_fast<TableList> it(cur_nested_join->join_list);
    cur_table_ref= it++;
    /*
      If the current nested join is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the first operand is
      already at the front of the list. Otherwise the first operand
      is in the end of the list of join operands.
    */
    if (!(cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      TableList *next;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


/*
  Return subselect that contains the FROM list this table is taken from

  SYNOPSIS
    TableList::containing_subselect()

  RETURN
    Subselect item for the subquery that contains the FROM list
    this table is taken from if there is any
    0 - otherwise

*/

Item_subselect *TableList::containing_subselect()
{
  return (select_lex ? select_lex->master_unit()->item : 0);
}


/*
  Compiles the tagged hints list and fills up the bitmasks.

  SYNOPSIS
    process_index_hints()
      table         the Table to operate on.

  DESCRIPTION
    The parser collects the index hints for each table in a "tagged list"
    (TableList::index_hints). Using the information in this tagged list
    this function sets the members Table::keys_in_use_for_query,
    Table::keys_in_use_for_group_by, Table::keys_in_use_for_order_by,
    Table::force_index and Table::covering_keys.

    Current implementation of the runtime does not allow mixing FORCE INDEX
    and USE INDEX, so this is checked here. Then the FORCE INDEX list
    (if non-empty) is appended to the USE INDEX list and a flag is set.

    Multiple hints of the same kind are processed so that each clause
    is applied to what is computed in the previous clause.
    For example:
        USE INDEX (i1) USE INDEX (i2)
    is equivalent to
        USE INDEX (i1,i2)
    and means "consider only i1 and i2".

    Similarly
        USE INDEX () USE INDEX (i1)
    is equivalent to
        USE INDEX (i1)
    and means "consider only the index i1"

    It is OK to have the same index several times, e.g. "USE INDEX (i1,i1)" is
    not an error.

    Different kind of hints (USE/FORCE/IGNORE) are processed in the following
    order:
      1. All indexes in USE (or FORCE) INDEX are added to the mask.
      2. All IGNORE INDEX

    e.g. "USE INDEX i1, IGNORE INDEX i1, USE INDEX i1" will not use i1 at all
    as if we had "USE INDEX i1, USE INDEX i1, IGNORE INDEX i1".

    As an optimization if there is a covering index, and we have
    IGNORE INDEX FOR GROUP/order_st, and this index is used for the JOIN part,
    then we have to ignore the IGNORE INDEX FROM GROUP/order_st.

  RETURN VALUE
    false                no errors found
    TRUE                 found and reported an error.
*/
bool TableList::process_index_hints(Table *tbl)
{
  /* initialize the result variables */
  tbl->keys_in_use_for_query= tbl->keys_in_use_for_group_by=
    tbl->keys_in_use_for_order_by= tbl->s->keys_in_use;

  /* index hint list processing */
  if (index_hints)
  {
    key_map index_join[INDEX_HINT_FORCE + 1];
    key_map index_order[INDEX_HINT_FORCE + 1];
    key_map index_group[INDEX_HINT_FORCE + 1];
    Index_hint *hint;
    int type;
    bool have_empty_use_join= false, have_empty_use_order= false,
         have_empty_use_group= false;
    List_iterator <Index_hint> iter(*index_hints);

    /* initialize temporary variables used to collect hints of each kind */
    for (type= INDEX_HINT_IGNORE; type <= INDEX_HINT_FORCE; type++)
    {
      index_join[type].reset();
      index_order[type].reset();
      index_group[type].reset();
    }

    /* iterate over the hints list */
    while ((hint= iter++))
    {
      uint32_t pos;

      /* process empty USE INDEX () */
      if (hint->type == INDEX_HINT_USE && !hint->key_name.str)
      {
        if (hint->clause & INDEX_HINT_MASK_JOIN)
        {
          index_join[hint->type].reset();
          have_empty_use_join= true;
        }
        if (hint->clause & INDEX_HINT_MASK_ORDER)
        {
          index_order[hint->type].reset();
          have_empty_use_order= true;
        }
        if (hint->clause & INDEX_HINT_MASK_GROUP)
        {
          index_group[hint->type].reset();
          have_empty_use_group= true;
        }
        continue;
      }

      /*
        Check if an index with the given name exists and get his offset in
        the keys bitmask for the table
      */
      if (tbl->s->keynames.type_names == 0 ||
          (pos= find_type(&tbl->s->keynames, hint->key_name.str,
                          hint->key_name.length, 1)) <= 0)
      {
        my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), hint->key_name.str, alias);
        return 1;
      }

      pos--;

      /* add to the appropriate clause mask */
      if (hint->clause & INDEX_HINT_MASK_JOIN)
        index_join[hint->type].set(pos);
      if (hint->clause & INDEX_HINT_MASK_ORDER)
        index_order[hint->type].set(pos);
      if (hint->clause & INDEX_HINT_MASK_GROUP)
        index_group[hint->type].set(pos);
    }

    /* cannot mix USE INDEX and FORCE INDEX */
    if ((index_join[INDEX_HINT_FORCE].any() ||
         index_order[INDEX_HINT_FORCE].any() ||
         index_group[INDEX_HINT_FORCE].any()) &&
        (index_join[INDEX_HINT_USE].any() ||  have_empty_use_join ||
         index_order[INDEX_HINT_USE].any() || have_empty_use_order ||
         index_group[INDEX_HINT_USE].any() || have_empty_use_group))
    {
      my_error(ER_WRONG_USAGE, MYF(0), index_hint_type_name[INDEX_HINT_USE],
               index_hint_type_name[INDEX_HINT_FORCE]);
      return 1;
    }

    /* process FORCE INDEX as USE INDEX with a flag */
    if (index_join[INDEX_HINT_FORCE].any() ||
        index_order[INDEX_HINT_FORCE].any() ||
        index_group[INDEX_HINT_FORCE].any())
    {
      tbl->force_index= true;
      index_join[INDEX_HINT_USE]|= index_join[INDEX_HINT_FORCE];
      index_order[INDEX_HINT_USE]|= index_order[INDEX_HINT_FORCE];
      index_group[INDEX_HINT_USE]|= index_group[INDEX_HINT_FORCE];
    }

    /* apply USE INDEX */
    if (index_join[INDEX_HINT_USE].any() || have_empty_use_join)
      tbl->keys_in_use_for_query&= index_join[INDEX_HINT_USE];
    if (index_order[INDEX_HINT_USE].any() || have_empty_use_order)
      tbl->keys_in_use_for_order_by&= index_order[INDEX_HINT_USE];
    if (index_group[INDEX_HINT_USE].any() || have_empty_use_group)
      tbl->keys_in_use_for_group_by&= index_group[INDEX_HINT_USE];

    /* apply IGNORE INDEX */
    key_map_subtract(tbl->keys_in_use_for_query, index_join[INDEX_HINT_IGNORE]);
    key_map_subtract(tbl->keys_in_use_for_order_by, index_order[INDEX_HINT_IGNORE]);
    key_map_subtract(tbl->keys_in_use_for_group_by, index_group[INDEX_HINT_IGNORE]);
  }

  /* make sure covering_keys don't include indexes disabled with a hint */
  tbl->covering_keys&= tbl->keys_in_use_for_query;
  return 0;
}


/**
  Print table as it should be in join list.

  @param str   string where table should be printed
*/
void TableList::print(Session *session, String *str, enum_query_type query_type)
{
  if (nested_join)
  {
    str->append('(');
    print_join(session, str, &nested_join->join_list, query_type);
    str->append(')');
  }
  else
  {
    const char *cmp_name;                         // Name to compare with alias
    if (derived)
    {
      // A derived table
      str->append('(');
      derived->print(str, query_type);
      str->append(')');
      cmp_name= "";                               // Force printing of alias
    }
    else
    {
      // A normal table
      {
        str->append_identifier(db, db_length);
        str->append('.');
      }
      if (schema_table)
      {
        str->append_identifier(schema_table_name, strlen(schema_table_name));
        cmp_name= schema_table_name;
      }
      else
      {
        str->append_identifier(table_name, table_name_length);
        cmp_name= table_name;
      }
    }
    if (my_strcasecmp(table_alias_charset, cmp_name, alias))
    {

      if (alias && alias[0])
      {
        str->append(' ');

        string t_alias(alias);
        transform(t_alias.begin(), t_alias.end(),
                  t_alias.begin(), ::tolower);

        str->append_identifier(t_alias.c_str(), t_alias.length());
      }

    }

    if (index_hints)
    {
      List_iterator<Index_hint> it(*index_hints);
      Index_hint *hint;

      while ((hint= it++))
      {
        str->append (STRING_WITH_LEN(" "));
        hint->print (session, str);
      }
    }
  }
}
