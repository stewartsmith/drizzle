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

#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/show.h>
#include <drizzled/item/ref.h>
#include <drizzled/plugin/client.h>
#include <drizzled/item/sum.h>
#include <drizzled/item/subselect.h>
#include <drizzled/sql_lex.h>

namespace drizzled {

Item_ref::Item_ref(Name_resolution_context *context_arg,
                   Item **item, const char *table_name_arg,
                   const char *field_name_arg,
                   bool alias_name_used_arg)
  :Item_ident(context_arg, NULL, table_name_arg, field_name_arg),
   result_field(0), ref(item)
{
  alias_name_used= alias_name_used_arg;
  /*
    This constructor used to create some internals references over fixed items
  */
  if (ref && *ref && (*ref)->fixed)
    set_properties();
}


/**
  Resolve the name of a reference to a column reference.

  The method resolves the column reference represented by 'this' as a column
  present in one of: GROUP BY clause, SELECT clause, outer queries. It is
  used typically for columns in the HAVING clause which are not under
  aggregate functions.

  POSTCONDITION @n
  Item_ref::ref is 0 or points to a valid item.

  @note
    The name resolution algorithm used is (where [T_j] is an optional table
    name that qualifies the column name):

  @code
        resolve_extended([T_j].col_ref_i)
        {
          Search for a column or derived column named col_ref_i [in table T_j]
          in the SELECT and GROUP clauses of Q.

          if such a column is NOT found AND    // Lookup in outer queries.
             there are outer queries
          {
            for each outer query Q_k beginning from the inner-most one
           {
              Search for a column or derived column named col_ref_i
              [in table T_j] in the SELECT and GROUP clauses of Q_k.

              if such a column is not found AND
                 - Q_k is not a group query AND
                 - Q_k is not inside an aggregate function
                 OR
                 - Q_(k-1) is not in a HAVING or SELECT clause of Q_k
              {
                search for a column or derived column named col_ref_i
                [in table T_j] in the FROM clause of Q_k;
              }
            }
          }
        }
  @endcode
  @n
    This procedure treats GROUP BY and SELECT clauses as one namespace for
    column references in HAVING. Notice that compared to
    Item_field::fix_fields, here we first search the SELECT and GROUP BY
    clauses, and then we search the FROM clause.

  @param[in]     session        current thread
  @param[in,out] reference  view column if this item was resolved to a
    view column

  @todo
    Here we could first find the field anyway, and then test this
    condition, so that we can give a better error message -
    ER_WRONG_FIELD_WITH_GROUP, instead of the less informative
    ER_BAD_FIELD_ERROR which we produce now.

  @retval
    true  if error
  @retval
    false on success
*/

bool Item_ref::fix_fields(Session *session, Item **reference)
{
  enum_parsing_place place= NO_MATTER;
  assert(fixed == 0);
  Select_Lex *current_sel= session->lex().current_select;

  if (!ref || ref == not_found_item)
  {
    if (!(ref= resolve_ref_in_select_and_group(session, this,
                                               context->select_lex)))
      goto error;             /* Some error occurred (e.g. ambiguous names). */

    if (ref == not_found_item) /* This reference was not resolved. */
    {
      Name_resolution_context *last_checked_context= context;
      Name_resolution_context *outer_context= context->outer_context;
      Field *from_field;
      ref= 0;

      if (!outer_context)
      {
        /* The current reference cannot be resolved in this query. */
        my_error(ER_BAD_FIELD_ERROR,MYF(0),
                 full_name(), session->where());
        goto error;
      }

      /*
        If there is an outer context (select), and it is not a derived table
        (which do not support the use of outer fields for now), try to
        resolve this reference in the outer select(s).

        We treat each subselect as a separate namespace, so that different
        subselects may contain columns with the same names. The subselects are
        searched starting from the innermost.
      */
      from_field= (Field*) not_found_field;

      do
      {
        Select_Lex *select= outer_context->select_lex;
        Item_subselect *prev_subselect_item=
          last_checked_context->select_lex->master_unit()->item;
        last_checked_context= outer_context;

        /* Search in the SELECT and GROUP lists of the outer select. */
        if (outer_context->resolve_in_select_list)
        {
          if (!(ref= resolve_ref_in_select_and_group(session, this, select)))
            goto error; /* Some error occurred (e.g. ambiguous names). */
          if (ref != not_found_item)
          {
            assert(*ref && (*ref)->fixed);
            prev_subselect_item->used_tables_cache|= (*ref)->used_tables();
            prev_subselect_item->const_item_cache&= (*ref)->const_item();
            break;
          }
          /*
            Set ref to 0 to ensure that we get an error in case we replaced
            this item with another item and still use this item in some
            other place of the parse tree.
          */
          ref= 0;
        }

        place= prev_subselect_item->parsing_place;
        /*
          Check table fields only if the subquery is used somewhere out of
          HAVING or the outer SELECT does not use grouping (i.e. tables are
          accessible).
          TODO:
          Here we could first find the field anyway, and then test this
          condition, so that we can give a better error message -
          ER_WRONG_FIELD_WITH_GROUP, instead of the less informative
          ER_BAD_FIELD_ERROR which we produce now.

          @todo determine if this is valid.
        */
        if ((place != IN_HAVING ||
             (!select->with_sum_func &&
              select->group_list.elements == 0)))
        {
          /*
            In case of view, find_field_in_tables() write pointer to view
            field expression to 'reference', i.e. it substitute that
            expression instead of this Item_ref
          */
          from_field= find_field_in_tables(session, this,
                                           outer_context->
                                             first_name_resolution_table,
                                           outer_context->
                                             last_name_resolution_table,
                                           reference,
                                           IGNORE_EXCEPT_NON_UNIQUE, true);
          if (! from_field)
            goto error;
          if (from_field == view_ref_found)
          {
            Item::Type refer_type= (*reference)->type();
            prev_subselect_item->used_tables_cache|=
              (*reference)->used_tables();
            prev_subselect_item->const_item_cache&=
              (*reference)->const_item();
            assert((*reference)->type() == REF_ITEM);
            mark_as_dependent(session, last_checked_context->select_lex,
                              context->select_lex, this,
                              ((refer_type == REF_ITEM ||
                                refer_type == FIELD_ITEM) ?
                               (Item_ident*) (*reference) :
                               0));
            /*
              view reference found, we substituted it instead of this
              Item, so can quit
            */
            return false;
          }
          if (from_field != not_found_field)
          {
            if (cached_table && cached_table->select_lex &&
                outer_context->select_lex &&
                cached_table->select_lex != outer_context->select_lex)
            {
              /*
                Due to cache, find_field_in_tables() can return field which
                doesn't belong to provided outer_context. In this case we have
                to find proper field context in order to fix field correcly.
              */
              do
              {
                outer_context= outer_context->outer_context;
                select= outer_context->select_lex;
                prev_subselect_item=
                  last_checked_context->select_lex->master_unit()->item;
                last_checked_context= outer_context;
              } while (outer_context && outer_context->select_lex &&
                       cached_table->select_lex != outer_context->select_lex);
            }
            prev_subselect_item->used_tables_cache|= from_field->getTable()->map;
            prev_subselect_item->const_item_cache= false;
            break;
          }
        }
        assert(from_field == not_found_field);

        /* Reference is not found => depend on outer (or just error). */
        prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
        prev_subselect_item->const_item_cache= false;

        outer_context= outer_context->outer_context;
      } while (outer_context);

      assert(from_field != 0 && from_field != view_ref_found);
      if (from_field != not_found_field)
      {
        Item_field* fld;
        fld= new Item_field(from_field);
        *reference= fld;
        mark_as_dependent(session, last_checked_context->select_lex,
                          session->lex().current_select, this, fld);
        /*
          A reference is resolved to a nest level that's outer or the same as
          the nest level of the enclosing set function : adjust the value of
          max_arg_level for the function if it's needed.
        */
        if (session->lex().in_sum_func &&
            session->lex().in_sum_func->nest_level >=
            last_checked_context->select_lex->nest_level)
          set_if_bigger(session->lex().in_sum_func->max_arg_level,
                        last_checked_context->select_lex->nest_level);
        return false;
      }
      if (ref == 0)
      {
        /* The item was not a table field and not a reference */
        my_error(ER_BAD_FIELD_ERROR, MYF(0),
                 full_name(), session->where());
        goto error;
      }
      /* Should be checked in resolve_ref_in_select_and_group(). */
      assert(*ref && (*ref)->fixed);
      mark_as_dependent(session, last_checked_context->select_lex,
                        context->select_lex, this, this);
      /*
        A reference is resolved to a nest level that's outer or the same as
        the nest level of the enclosing set function : adjust the value of
        max_arg_level for the function if it's needed.
      */
      if (session->lex().in_sum_func &&
          session->lex().in_sum_func->nest_level >=
          last_checked_context->select_lex->nest_level)
        set_if_bigger(session->lex().in_sum_func->max_arg_level,
                      last_checked_context->select_lex->nest_level);
    }
  }

  assert(*ref);
  /*
    Check if this is an incorrect reference in a group function or forward
    reference. Do not issue an error if this is:
      1. outer reference (will be fixed later by the fix_inner_refs function);
      2. an unnamed reference inside an aggregate function.
  */
  if (!((*ref)->type() == REF_ITEM &&
       ((Item_ref *)(*ref))->ref_type() == OUTER_REF) &&
      (((*ref)->with_sum_func && name &&
        !(current_sel->linkage != GLOBAL_OPTIONS_TYPE &&
          current_sel->having_fix_field)) ||
       !(*ref)->fixed))
  {
    my_error(ER_ILLEGAL_REFERENCE, MYF(0),
             name, ((*ref)->with_sum_func?
                    "reference to group function":
                    "forward reference in item list"));
    goto error;
  }

  set_properties();

  if ((*ref)->check_cols(1))
    goto error;
  return false;

error:
  context->process_error(session);
  return true;
}


void Item_ref::set_properties()
{
  max_length= (*ref)->max_length;
  maybe_null= (*ref)->maybe_null;
  decimals=   (*ref)->decimals;
  collation.set((*ref)->collation);
  /*
    We have to remember if we refer to a sum function, to ensure that
    split_sum_func() doesn't try to change the reference.
  */
  with_sum_func= (*ref)->with_sum_func;
  unsigned_flag= (*ref)->unsigned_flag;
  fixed= 1;
  if (alias_name_used)
    return;
  if ((*ref)->type() == FIELD_ITEM)
    alias_name_used= ((Item_ident *) (*ref))->alias_name_used;
  else
    alias_name_used= true; // it is not field, so it is was resolved by alias
}


void Item_ref::cleanup()
{
  Item_ident::cleanup();
  result_field= 0;
  return;
}


void Item_ref::print(String *str)
{
  if (ref)
  {
    if ((*ref)->type() != Item::CACHE_ITEM &&
        !table_name && name && alias_name_used)
    {
      str->append_identifier(name, (uint32_t) strlen(name));
    }
    else
      (*ref)->print(str);
  }
  else
    Item_ident::print(str);
}


void Item_ref::send(plugin::Client *client, String *tmp)
{
  if (result_field)
  {
    client->store(result_field);
    return;
  }
  return (*ref)->send(client, tmp);
}


double Item_ref::val_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0.0;
    return result_field->val_real();
  }
  return val_real();
}


int64_t Item_ref::val_int_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    return result_field->val_int();
  }
  return val_int();
}


String *Item_ref::str_result(String* str)
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    str->set_charset(str_value.charset());
    return result_field->val_str(str, &str_value);
  }
  return val_str(str);
}


type::Decimal *Item_ref::val_decimal_result(type::Decimal *decimal_value)
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    return result_field->val_decimal(decimal_value);
  }
  return val_decimal(decimal_value);
}


bool Item_ref::val_bool_result()
{
  if (result_field)
  {
    if ((null_value= result_field->is_null()))
      return 0;
    switch (result_field->result_type()) {
    case INT_RESULT:
      return result_field->val_int() != 0;

    case DECIMAL_RESULT:
      {
        type::Decimal decimal_value;
        type::Decimal *val= result_field->val_decimal(&decimal_value);
        if (val)
          return not val->isZero();
        return 0;
      }

    case REAL_RESULT:
    case STRING_RESULT:
      return result_field->val_real() != 0.0;

    case ROW_RESULT:
      assert(0);
    }
  }

  return val_bool();
}


double Item_ref::val_real()
{
  assert(fixed);
  double tmp=(*ref)->val_result();
  null_value=(*ref)->null_value;
  return tmp;
}


int64_t Item_ref::val_int()
{
  assert(fixed);
  int64_t tmp=(*ref)->val_int_result();
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_ref::val_bool()
{
  assert(fixed);
  bool tmp= (*ref)->val_bool_result();
  null_value= (*ref)->null_value;
  return tmp;
}


String *Item_ref::val_str(String* tmp)
{
  assert(fixed);
  tmp=(*ref)->str_result(tmp);
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_ref::is_null()
{
  assert(fixed);
  return (*ref)->is_null();
}


bool Item_ref::get_date(type::Time &ltime,uint32_t fuzzydate)
{
  return (null_value=(*ref)->get_date_result(ltime,fuzzydate));
}


type::Decimal *Item_ref::val_decimal(type::Decimal *decimal_value)
{
  type::Decimal *val= (*ref)->val_decimal_result(decimal_value);
  null_value= (*ref)->null_value;
  return val;
}

int Item_ref::save_in_field(Field *to, bool no_conversions)
{
  int res;
  assert(!result_field);
  res= (*ref)->save_in_field(to, no_conversions);
  null_value= (*ref)->null_value;
  return res;
}


void Item_ref::save_org_in_field(Field *field)
{
  (*ref)->save_org_in_field(field);
}


void Item_ref::make_field(SendField *field)
{
  (*ref)->make_field(field);
  /* Non-zero in case of a view */
  if (name)
    field->col_name= name;
  if (table_name)
    field->table_name= table_name;
  if (db_name)
    field->db_name= db_name;
}


Item *Item_ref::get_tmp_table_item(Session *session)
{
  if (!result_field)
    return (*ref)->get_tmp_table_item(session);

  Item_field *item= new Item_field(result_field);
  if (item)
  {
    item->table_name= table_name;
    item->db_name= db_name;
  }
  return item;
}

void Item_ref::fix_after_pullout(Select_Lex *new_parent, Item **)
{
  if (depended_from == new_parent)
  {
    (*ref)->fix_after_pullout(new_parent, ref);
    depended_from= NULL;
  }
}

} /* namespace drizzled */
