/* Copyright (C) 2009 Sun Microsystems, Inc.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#include <string>

#include <drizzled/error.h>
#include <drizzled/table_list.h>
#include <drizzled/item.h>
#include <drizzled/item/field.h>
#include <drizzled/nested_join.h>
#include <drizzled/sql_lex.h>
#include <drizzled/sql_select.h>

using namespace std;

namespace drizzled {

bool TableList::set_insert_values(memory::Root *)
{
  if (table)
  {
    table->insert_values.resize(table->getShare()->rec_buff_length);
  }

  return false;
}

bool TableList::is_leaf_for_name_resolution()
{
  return (is_natural_join || is_join_columns_complete || !nested_join);
}

TableList *TableList::find_underlying_table(Table *table_to_find)
{
  /* is this real table and table which we are looking for? */
  if (table == table_to_find)
    return this;

  return NULL;
}

bool TableList::isCartesian() const
{
  return false;
}

bool TableList::placeholder()
{
  return derived || (create && !table->getDBStat()) || !table;
}

/*
 * The right-most child of a nested table reference is the first
 * element in the list of children because the children are inserted
 * in reverse order.
 */
TableList *TableList::last_leaf_for_name_resolution()
{
  TableList *cur_table_ref= this;
  NestedJoin *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  assert(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    cur_table_ref= &cur_nested_join->join_list.front();
    /*
      If the current nested is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the last operand is in the
      end of the list.
    */
    if ((cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      List<TableList>::iterator it(cur_nested_join->join_list.begin());
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
 * The left-most child of a nested table reference is the last element
 * in the list of children because the children are inserted in
 * reverse order.
 */
TableList *TableList::first_leaf_for_name_resolution()
{
  TableList *cur_table_ref= NULL;
  NestedJoin *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  assert(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    List<TableList>::iterator it(cur_nested_join->join_list.begin());
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

Item_subselect *TableList::containing_subselect()
{
  return (select_lex ? select_lex->master_unit()->item : 0);
}

bool TableList::process_index_hints(Table *tbl)
{
  /* initialize the result variables */
  tbl->keys_in_use_for_query= tbl->keys_in_use_for_group_by=
    tbl->keys_in_use_for_order_by= tbl->getShare()->keys_in_use;

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
    List_iterator <Index_hint> iter(index_hints->begin());

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
      uint32_t pos= 0;

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
      if (not tbl->getShare()->doesKeyNameExist(hint->key_name.str, hint->key_name.length, pos))
      {
        my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), hint->key_name.str, alias);
        return 1;
      }
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

void TableList::print(Session *session, String *str)
{
  if (nested_join)
  {
    str->append('(');
    print_join(session, str, &nested_join->join_list);
    str->append(')');
  }
  else
  {
    const char *cmp_name;                         // Name to compare with alias
    if (derived)
    {
      // A derived table
      str->append('(');
      derived->print(str);
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
      str->append_identifier(table_name, table_name_length);
      cmp_name= table_name;
    }
    if (my_strcasecmp(table_alias_charset, cmp_name, alias))
    {

      if (alias && alias[0])
      {
        str->append(' ');

        string t_alias(alias);
        transform(t_alias.begin(), t_alias.end(), t_alias.begin(), ::tolower);
        str->append_identifier(t_alias.c_str(), t_alias.length());
      }
    }

    if (index_hints)
    {
      List<Index_hint>::iterator it(index_hints->begin());
      while (Index_hint* hint= it++)
      {
        str->append (STRING_WITH_LEN(" "));
        hint->print (session, str);
      }
    }
  }
}

} /* namespace drizzled */
