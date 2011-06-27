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
#include <drizzled/table.h>
#include <drizzled/error.h>
#include <drizzled/join.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_select.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/field.h>
#include <drizzled/item/outer_ref.h>
#include <drizzled/plugin/client.h>
#include <drizzled/item/subselect.h>
#include <drizzled/sql_lex.h>

#include <boost/dynamic_bitset.hpp>

namespace drizzled {

/**
  Store the pointer to this item field into a list if not already there.

  The method is used by Item::walk to collect all unique Item_field objects
  from a tree of Items into a set of items represented as a list.

  Item_cond::walk() and Item_func::walk() stop the evaluation of the
  processor function for its arguments once the processor returns
  true.Therefore in order to force this method being called for all item
  arguments in a condition the method must return false.

  @param arg  pointer to a List<Item_field>

  @return
    false to force the evaluation of collect_item_field_processor
    for the subsequent items.
*/

bool Item_field::collect_item_field_processor(unsigned char *arg)
{
  List<Item_field> *item_list= (List<Item_field>*) arg;
  List<Item_field>::iterator item_list_it(item_list->begin());
  Item_field *curr_item;
  while ((curr_item= item_list_it++))
  {
    if (curr_item->eq(this, 1))
      return false; /* Already in the set. */
  }
  item_list->push_back(this);
  return false;
}


/**
  Check if an Item_field references some field from a list of fields.

  Check whether the Item_field represented by 'this' references any
  of the fields in the keyparts passed via 'arg'. Used with the
  method Item::walk() to test whether any keypart in a sequence of
  keyparts is referenced in an expression.

  @param arg   Field being compared, arg must be of type Field

  @retval
    true  if 'this' references the field 'arg'
  @retval
    false otherwise
*/

bool Item_field::find_item_in_field_list_processor(unsigned char *arg)
{
  KeyPartInfo *first_non_group_part= *((KeyPartInfo **) arg);
  KeyPartInfo *last_part= *(((KeyPartInfo **) arg) + 1);
  KeyPartInfo *cur_part;

  for (cur_part= first_non_group_part; cur_part != last_part; cur_part++)
  {
    if (field->eq(cur_part->field))
      return true;
  }
  return false;
}


/*
  Mark field in read_map

  NOTES
    This is used by filesort to register used fields in a a temporary
    column read set or to register used fields in a view
*/

bool Item_field::register_field_in_read_map(unsigned char *arg)
{
  Table *table= (Table *) arg;
  if (field->getTable() == table || !table)
    field->getTable()->setReadSet(field->position());

  return 0;
}


Item_field::Item_field(Field *f)
  :Item_ident(0, NULL, f->getTable()->getAlias(), f->field_name),
   item_equal(0), no_const_subst(0),
   have_privileges(0), any_privileges(0)
{
  set_field(f);
  /*
    field_name and table_name should not point to garbage
    if this item is to be reused
  */
  orig_table_name= orig_field_name= "";
}


/**
  Constructor used inside setup_wild().

  Ensures that field, table, and database names will live as long as
  Item_field (this is important in prepared statements).
*/

Item_field::Item_field(Session *,
                       Name_resolution_context *context_arg,
                       Field *f) :
  Item_ident(context_arg,
             f->getTable()->getShare()->getSchemaName(),
             f->getTable()->getAlias(),
             f->field_name),
   item_equal(0),
   no_const_subst(0),
   have_privileges(0),
   any_privileges(0)
{
  set_field(f);
}


Item_field::Item_field(Name_resolution_context *context_arg,
                       const char *db_arg,const char *table_name_arg,
                       const char *field_name_arg) :
  Item_ident(context_arg, db_arg,table_name_arg,field_name_arg),
   field(0),
   result_field(0),
   item_equal(0),
   no_const_subst(0),
   have_privileges(0),
   any_privileges(0)
{
  Select_Lex *select= getSession().lex().current_select;
  collation.set(DERIVATION_IMPLICIT);

  if (select && select->parsing_place != IN_HAVING)
      select->select_n_where_fields++;
}

/**
  Constructor need to process subselect with temporary tables (see Item)
*/

Item_field::Item_field(Session *session, Item_field *item) :
  Item_ident(session, item),
  field(item->field),
  result_field(item->result_field),
  item_equal(item->item_equal),
  no_const_subst(item->no_const_subst),
  have_privileges(item->have_privileges),
  any_privileges(item->any_privileges)
{
  collation.set(DERIVATION_IMPLICIT);
}

void Item_field::set_field(Field *field_par)
{
  field=result_field=field_par;			// for easy coding with fields
  maybe_null=field->maybe_null();
  decimals= field->decimals();
  max_length= field_par->max_display_length();
  table_name= field_par->getTable()->getAlias();
  field_name= field_par->field_name;
  db_name= field_par->getTable()->getShare()->getSchemaName();
  alias_name_used= field_par->getTable()->alias_name_used;
  unsigned_flag=test(field_par->flags & UNSIGNED_FLAG);
  collation.set(field_par->charset(), field_par->derivation());
  fixed= 1;
}


/**
  Reset this item to point to a field from the new temporary table.
  This is used when we create a new temporary table for each execution
  of prepared statement.
*/

void Item_field::reset_field(Field *f)
{
  set_field(f);
  /* 'name' is pointing at field->field_name of old field */
  name= (char*) f->field_name;
}

/* ARGSUSED */
String *Item_field::val_str(String *str)
{
  assert(fixed == 1);
  if ((null_value=field->is_null()))
    return 0;
  str->set_charset(str_value.charset());
  return field->val_str(str,&str_value);
}


double Item_field::val_real()
{
  assert(fixed == 1);
  if ((null_value=field->is_null()))
    return 0.0;
  return field->val_real();
}


int64_t Item_field::val_int()
{
  assert(fixed == 1);
  if ((null_value=field->is_null()))
    return 0;
  return field->val_int();
}


type::Decimal *Item_field::val_decimal(type::Decimal *decimal_value)
{
  if ((null_value= field->is_null()))
    return 0;
  return field->val_decimal(decimal_value);
}


String *Item_field::str_result(String *str)
{
  if ((null_value=result_field->is_null()))
    return 0;
  str->set_charset(str_value.charset());
  return result_field->val_str(str,&str_value);
}

bool Item_field::get_date(type::Time &ltime,uint32_t fuzzydate)
{
  if ((null_value=field->is_null()) || field->get_date(ltime,fuzzydate))
  {
    ltime.reset();
    return 1;
  }
  return 0;
}

bool Item_field::get_date_result(type::Time &ltime,uint32_t fuzzydate)
{
  if ((null_value=result_field->is_null()) ||
      result_field->get_date(ltime,fuzzydate))
  {
    ltime.reset();
    return 1;
  }
  return 0;
}

bool Item_field::get_time(type::Time &ltime)
{
  if ((null_value=field->is_null()) || field->get_time(ltime))
  {
    ltime.reset();
    return 1;
  }
  return 0;
}

double Item_field::val_result()
{
  if ((null_value=result_field->is_null()))
    return 0.0;
  return result_field->val_real();
}

int64_t Item_field::val_int_result()
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_int();
}


type::Decimal *Item_field::val_decimal_result(type::Decimal *decimal_value)
{
  if ((null_value= result_field->is_null()))
    return 0;
  return result_field->val_decimal(decimal_value);
}


bool Item_field::val_bool_result()
{
  if ((null_value= result_field->is_null()))
  {
    return false;
  }

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
    return 0;                                   // Shut up compiler
  }

  assert(0);
  return 0;                                   // Shut up compiler
}


bool Item_field::eq(const Item *item, bool) const
{
  const Item *item_ptr= item->real_item();
  if (item_ptr->type() != FIELD_ITEM)
    return 0;

  const Item_field *item_field= static_cast<const Item_field *>(item_ptr);
  if (item_field->field && field)
    return item_field->field == field;
  /*
    We may come here when we are trying to find a function in a GROUP BY
    clause from the select list.
    In this case the '100 % correct' way to do this would be to first
    run fix_fields() on the GROUP BY item and then retry this function, but
    I think it's better to relax the checking a bit as we will in
    most cases do the correct thing by just checking the field name.
    (In cases where we would choose wrong we would have to generate a
    ER_NON_UNIQ_ERROR).
  */
  return (not my_strcasecmp(system_charset_info, item_field->name, field_name) &&
          (not item_field->table_name || not table_name ||
           (not my_strcasecmp(table_alias_charset, item_field->table_name, table_name) &&
            (not item_field->db_name || not db_name ||
             (item_field->db_name && not my_strcasecmp(system_charset_info, item_field->db_name, db_name))))));
}


table_map Item_field::used_tables() const
{
  if (field->getTable()->const_table)
  {
    return 0;					// const item
  }

  return (depended_from ? OUTER_REF_TABLE_BIT : field->getTable()->map);
}

enum Item_result Item_field::result_type () const
{
  return field->result_type();
}


Item_result Item_field::cast_to_int_type() const
{
  return field->cast_to_int_type();
}


enum_field_types Item_field::field_type() const
{
  return field->type();
}


void Item_field::fix_after_pullout(Select_Lex *new_parent, Item **)
{
  if (new_parent == depended_from)
    depended_from= NULL;
  Name_resolution_context *ctx= new Name_resolution_context();
  ctx->outer_context= NULL; // We don't build a complete name resolver
  ctx->select_lex= new_parent;
  ctx->first_name_resolution_table= context->first_name_resolution_table;
  ctx->last_name_resolution_table=  context->last_name_resolution_table;
  this->context=ctx;
}


bool Item_field::is_null()
{
  return field->is_null();
}


Item *Item_field::get_tmp_table_item(Session *session)
{
  Item_field *new_item= new Item_field(session, this);
  new_item->field= new_item->result_field;
  return new_item;
}

int64_t Item_field::val_int_endpoint(bool, bool *)
{
  int64_t res= val_int();
  return null_value? INT64_MIN : res;
}


/**
  Resolve the name of an outer select column reference.

  The method resolves the column reference represented by 'this' as a column
  present in outer selects that contain current select.

  In prepared statements, because of cache, find_field_in_tables()
  can resolve fields even if they don't belong to current context.
  In this case this method only finds appropriate context and marks
  current select as dependent. The found reference of field should be
  provided in 'from_field'.

  @param[in] session             current thread
  @param[in,out] from_field  found field reference or (Field*)not_found_field
  @param[in,out] reference   view column if this item was resolved to a
    view column

  @note
    This is the inner loop of Item_field::fix_fields:
  @code
        for each outer query Q_k beginning from the inner-most one
        {
          search for a column or derived column named col_ref_i
          [in table T_j] in the FROM clause of Q_k;

          if such a column is not found
            Search for a column or derived column named col_ref_i
            [in table T_j] in the SELECT and GROUP clauses of Q_k.
        }
  @endcode

  @retval
    1   column succefully resolved and fix_fields() should continue.
  @retval
    0   column fully fixed and fix_fields() should return false
  @retval
    -1  error occured
*/

int
Item_field::fix_outer_field(Session *session, Field **from_field, Item **reference)
{
  enum_parsing_place place= NO_MATTER;
  bool field_found= (*from_field != not_found_field);
  bool upward_lookup= false;

  /*
    If there are outer contexts (outer selects, but current select is
    not derived table or view) try to resolve this reference in the
    outer contexts.

    We treat each subselect as a separate namespace, so that different
    subselects may contain columns with the same names. The subselects
    are searched starting from the innermost.
  */
  Name_resolution_context *last_checked_context= context;
  Item **ref= (Item **) not_found_item;
  Select_Lex *current_sel= (Select_Lex *) session->lex().current_select;
  Name_resolution_context *outer_context= 0;
  Select_Lex *select= 0;
  /* Currently derived tables cannot be correlated */
  if (current_sel->master_unit()->first_select()->linkage !=
      DERIVED_TABLE_TYPE)
    outer_context= context->outer_context;
  for (;
       outer_context;
       outer_context= outer_context->outer_context)
  {
    select= outer_context->select_lex;
    Item_subselect *prev_subselect_item=
      last_checked_context->select_lex->master_unit()->item;
    last_checked_context= outer_context;
    upward_lookup= true;

    place= prev_subselect_item->parsing_place;
    /*
      If outer_field is set, field was already found by first call
      to find_field_in_tables(). Only need to find appropriate context.
    */
    if (field_found && outer_context->select_lex !=
        cached_table->select_lex)
      continue;
    /*
      In case of a view, find_field_in_tables() writes the pointer to
      the found view field into '*reference', in other words, it
      substitutes this Item_field with the found expression.
    */
    if (field_found || (*from_field= find_field_in_tables(session, this,
                                          outer_context->
                                            first_name_resolution_table,
                                          outer_context->
                                            last_name_resolution_table,
                                          reference,
                                          IGNORE_EXCEPT_NON_UNIQUE,
                                          true)) !=
        not_found_field)
    {
      if (*from_field)
      {
        if (*from_field != view_ref_found)
        {
          prev_subselect_item->used_tables_cache|= (*from_field)->getTable()->map;
          prev_subselect_item->const_item_cache= false;
          set_field(*from_field);
          if (!last_checked_context->select_lex->having_fix_field &&
              select->group_list.elements &&
              (place == SELECT_LIST || place == IN_HAVING))
          {
            Item_outer_ref *rf;
            /*
              If an outer field is resolved in a grouping select then it
              is replaced for an Item_outer_ref object. Otherwise an
              Item_field object is used.
              The new Item_outer_ref object is saved in the inner_refs_list of
              the outer select. Here it is only created. It can be fixed only
              after the original field has been fixed and this is done in the
              fix_inner_refs() function.
            */
            ;
            rf= new Item_outer_ref(context, this);
            *reference= rf;
            select->inner_refs_list.push_back(rf);
            rf->in_sum_func= session->lex().in_sum_func;
          }
          /*
            A reference is resolved to a nest level that's outer or the same as
            the nest level of the enclosing set function : adjust the value of
            max_arg_level for the function if it's needed.
          */
          if (session->lex().in_sum_func &&
              session->lex().in_sum_func->nest_level >= select->nest_level)
          {
            Item::Type ref_type= (*reference)->type();
            set_if_bigger(session->lex().in_sum_func->max_arg_level,
                          select->nest_level);
            set_field(*from_field);
            fixed= 1;
            mark_as_dependent(session, last_checked_context->select_lex,
                              context->select_lex, this,
                              ((ref_type == REF_ITEM ||
                                ref_type == FIELD_ITEM) ?
                               (Item_ident*) (*reference) : 0));
            return 0;
          }
        }
        else
        {
          Item::Type ref_type= (*reference)->type();
          prev_subselect_item->used_tables_cache|=
            (*reference)->used_tables();
          prev_subselect_item->const_item_cache&=
            (*reference)->const_item();
          mark_as_dependent(session, last_checked_context->select_lex,
                            context->select_lex, this,
                            ((ref_type == REF_ITEM || ref_type == FIELD_ITEM) ?
                             (Item_ident*) (*reference) :
                             0));
          /*
            A reference to a view field had been found and we
            substituted it instead of this Item (find_field_in_tables
            does it by assigning the new value to *reference), so now
            we can return from this function.
          */
          return 0;
        }
      }
      break;
    }

    /* Search in SELECT and GROUP lists of the outer select. */
    if (place != IN_WHERE && place != IN_ON)
    {
      if (!(ref= resolve_ref_in_select_and_group(session, this, select)))
        return -1; /* Some error occurred (e.g. ambiguous names). */
      if (ref != not_found_item)
      {
        assert(*ref && (*ref)->fixed);
        prev_subselect_item->used_tables_cache|= (*ref)->used_tables();
        prev_subselect_item->const_item_cache&= (*ref)->const_item();
        break;
      }
    }

    /*
      Reference is not found in this select => this subquery depend on
      outer select (or we just trying to find wrong identifier, in this
      case it does not matter which used tables bits we set)
    */
    prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
    prev_subselect_item->const_item_cache= false;
  }

  assert(ref != 0);
  if (!*from_field)
    return -1;
  if (ref == not_found_item && *from_field == not_found_field)
  {
    if (upward_lookup)
    {
      // We can't say exactly what absent table or field
      my_error(ER_BAD_FIELD_ERROR, MYF(0), full_name(), session->where());
    }
    else
    {
      /* Call find_field_in_tables only to report the error */
      find_field_in_tables(session, this,
                           context->first_name_resolution_table,
                           context->last_name_resolution_table,
                           reference, REPORT_ALL_ERRORS, true);
    }
    return -1;
  }
  else if (ref != not_found_item)
  {
    Item *save;
    Item_ref *rf;

    /* Should have been checked in resolve_ref_in_select_and_group(). */
    assert(*ref && (*ref)->fixed);
    /*
      Here, a subset of actions performed by Item_ref::set_properties
      is not enough. So we pass ptr to NULL into Item_[direct]_ref
      constructor, so no initialization is performed, and call
      fix_fields() below.
    */
    save= *ref;
    *ref= NULL;                             // Don't call set_properties()
    rf= (place == IN_HAVING ?
         new Item_ref(context, ref, (char*) table_name,
                      (char*) field_name, alias_name_used) :
         (!select->group_list.elements ?
         new Item_direct_ref(context, ref, (char*) table_name,
                             (char*) field_name, alias_name_used) :
         new Item_outer_ref(context, ref, (char*) table_name,
                            (char*) field_name, alias_name_used)));
    *ref= save;
    if (!rf)
      return -1;

    if (place != IN_HAVING && select->group_list.elements)
    {
      outer_context->select_lex->inner_refs_list.push_back((Item_outer_ref*)rf);
      ((Item_outer_ref*)rf)->in_sum_func= session->lex().in_sum_func;
    }
    *reference= rf;
    /*
      rf is Item_ref => never substitute other items (in this case)
      during fix_fields() => we can use rf after fix_fields()
    */
    assert(!rf->fixed);                // Assured by Item_ref()
    if (rf->fix_fields(session, reference) || rf->check_cols(1))
      return -1;

    mark_as_dependent(session, last_checked_context->select_lex,
                      context->select_lex, this,
                      rf);
    return 0;
  }
  else
  {
    mark_as_dependent(session, last_checked_context->select_lex,
                      context->select_lex,
                      this, (Item_ident*)*reference);
    if (last_checked_context->select_lex->having_fix_field)
    {
      Item_ref *rf;
      rf= new Item_ref(context,
                       (cached_table->getSchemaName()[0] ? cached_table->getSchemaName() : 0),
                       (char*) cached_table->alias, (char*) field_name);
      if (!rf)
        return -1;
      *reference= rf;
      /*
        rf is Item_ref => never substitute other items (in this case)
        during fix_fields() => we can use rf after fix_fields()
      */
      assert(!rf->fixed);                // Assured by Item_ref()
      if (rf->fix_fields(session, reference) || rf->check_cols(1))
        return -1;
      return 0;
    }
  }
  return 1;
}


/**
  Resolve the name of a column reference.

  The method resolves the column reference represented by 'this' as a column
  present in one of: FROM clause, SELECT clause, GROUP BY clause of a query
  Q, or in outer queries that contain Q.

  The name resolution algorithm used is (where [T_j] is an optional table
  name that qualifies the column name):

  @code
    resolve_column_reference([T_j].col_ref_i)
    {
      search for a column or derived column named col_ref_i
      [in table T_j] in the FROM clause of Q;

      if such a column is NOT found AND    // Lookup in outer queries.
         there are outer queries
      {
        for each outer query Q_k beginning from the inner-most one
        {
          search for a column or derived column named col_ref_i
          [in table T_j] in the FROM clause of Q_k;

          if such a column is not found
            Search for a column or derived column named col_ref_i
            [in table T_j] in the SELECT and GROUP clauses of Q_k.
        }
      }
    }
  @endcode

    Notice that compared to Item_ref::fix_fields, here we first search the FROM
    clause, and then we search the SELECT and GROUP BY clauses.

  @param[in]     session        current thread
  @param[in,out] reference  view column if this item was resolved to a
    view column

  @retval
    true  if error
  @retval
    false on success
*/

bool Item_field::fix_fields(Session *session, Item **reference)
{
  assert(fixed == 0);
  Field *from_field= (Field *)not_found_field;
  bool outer_fixed= false;

  if (!field)					// If field is not checked
  {
    /*
      In case of view, find_field_in_tables() write pointer to view field
      expression to 'reference', i.e. it substitute that expression instead
      of this Item_field
    */
    if ((from_field= find_field_in_tables(session, this,
                                          context->first_name_resolution_table,
                                          context->last_name_resolution_table,
                                          reference,
                                          session->lex().use_only_table_context ?
                                            REPORT_ALL_ERRORS :
                                            IGNORE_EXCEPT_NON_UNIQUE, true)) ==
        not_found_field)
    {
      int ret;
      /* Look up in current select's item_list to find aliased fields */
      if (session->lex().current_select->is_item_list_lookup)
      {
        uint32_t counter;
        enum_resolution_type resolution;
        Item** res= find_item_in_list(session,
                                      this, session->lex().current_select->item_list,
                                      &counter, REPORT_EXCEPT_NOT_FOUND,
                                      &resolution);
        if (!res)
          return 1;
        if (resolution == RESOLVED_AGAINST_ALIAS)
          alias_name_used= true;
        if (res != (Item **)not_found_item)
        {
          if ((*res)->type() == Item::FIELD_ITEM)
          {
            /*
              It's an Item_field referencing another Item_field in the select
              list.
              Use the field from the Item_field in the select list and leave
              the Item_field instance in place.
            */

            Field *new_field= (*((Item_field**)res))->field;

            if (new_field == NULL)
            {
              /* The column to which we link isn't valid. */
              my_error(ER_BAD_FIELD_ERROR, MYF(0), (*res)->name,
                       session->where());
              return 1;
            }

            set_field(new_field);
            return 0;
          }
          else
          {
            /*
              It's not an Item_field in the select list so we must make a new
              Item_ref to point to the Item in the select list and replace the
              Item_field created by the parser with the new Item_ref.
            */
            Item_ref *rf= new Item_ref(context, db_name,table_name,field_name);
            if (!rf)
              return 1;
            *reference= rf;
            /*
              Because Item_ref never substitutes itself with other items
              in Item_ref::fix_fields(), we can safely use the original
              pointer to it even after fix_fields()
             */
            return rf->fix_fields(session, reference) ||  rf->check_cols(1);
          }
        }
      }
      if ((ret= fix_outer_field(session, &from_field, reference)) < 0)
        goto error;
      outer_fixed= true;
      if (!ret)
        goto mark_non_agg_field;
    }
    else if (!from_field)
      goto error;

    if (!outer_fixed && cached_table && cached_table->select_lex &&
        context->select_lex &&
        cached_table->select_lex != context->select_lex)
    {
      int ret;
      if ((ret= fix_outer_field(session, &from_field, reference)) < 0)
        goto error;
      outer_fixed= 1;
      if (!ret)
        goto mark_non_agg_field;
    }

    /*
      if it is not expression from merged VIEW we will set this field.

      We can leave expression substituted from view for next PS/SP rexecution
      (i.e. do not register this substitution for reverting on cleanup()
      (register_item_tree_changing())), because this subtree will be
      fix_field'ed during setup_tables()->setup_underlying() (i.e. before
      all other expressions of query, and references on tables which do
      not present in query will not make problems.

      Also we suppose that view can't be changed during PS/SP life.
    */
    if (from_field == view_ref_found)
      return false;

    set_field(from_field);
    if (session->lex().in_sum_func &&
        session->lex().in_sum_func->nest_level ==
        session->lex().current_select->nest_level)
    {
      set_if_bigger(session->lex().in_sum_func->max_arg_level,
                    session->lex().current_select->nest_level);
    }
  }
  else if (session->mark_used_columns != MARK_COLUMNS_NONE)
  {
    Table *table= field->getTable();
    boost::dynamic_bitset<> *current_bitmap, *other_bitmap;
    if (session->mark_used_columns == MARK_COLUMNS_READ)
    {
      current_bitmap= table->read_set;
      other_bitmap=   table->write_set;
    }
    else
    {
      current_bitmap= table->write_set;
      other_bitmap=   table->read_set;
    }
    //if (! current_bitmap->testAndSet(field->position()))
    if (! current_bitmap->test(field->position()))
    {
      if (! other_bitmap->test(field->position()))
      {
        /* First usage of column */
        table->used_fields++;                     // Used to optimize loops
        table->covering_keys&= field->part_of_key;
      }
    }
  }
  fixed= 1;
mark_non_agg_field:
  return false;

error:
  context->process_error(session);
  return true;
}

Item *Item_field::safe_charset_converter(const charset_info_st * const tocs)
{
  no_const_subst= 1;
  return Item::safe_charset_converter(tocs);
}


void Item_field::cleanup()
{
  Item_ident::cleanup();
  /*
    Even if this object was created by direct link to field in setup_wild()
    it will be linked correctly next time by name of field and table alias.
    I.e. we can drop 'field'.
   */
  field= result_field= 0;
  null_value= false;
  return;
}


bool Item_field::result_as_int64_t()
{
  return field->can_be_compared_as_int64_t();
}


/**
  Find a field among specified multiple equalities.

  The function first searches the field among multiple equalities
  of the current level (in the cond_equal->current_level list).
  If it fails, it continues searching in upper levels accessed
  through a pointer cond_equal->upper_levels.
  The search terminates as soon as a multiple equality containing
  the field is found.

  @param cond_equal   reference to list of multiple equalities where
                      the field (this object) is to be looked for

  @return
    - First Item_equal containing the field, if success
    - 0, otherwise
*/

Item_equal *Item_field::find_item_equal(COND_EQUAL *cond_equal)
{
  Item_equal *item= 0;
  while (cond_equal)
  {
    List<Item_equal>::iterator li(cond_equal->current_level.begin());
    while ((item= li++))
    {
      if (item->contains(field))
        return item;
    }
    /*
      The field is not found in any of the multiple equalities
      of the current level. Look for it in upper levels
    */
    cond_equal= cond_equal->upper_levels;
  }
  return 0;
}


/**
  Check whether a field can be substituted by an equal item.

  The function checks whether a substitution of the field
  occurrence for an equal item is valid.

  @param arg   *arg != NULL <-> the field is in the context where
               substitution for an equal item is valid

  @note
    The following statement is not always true:
  @n
    x=y => F(x)=F(x/y).
  @n
    This means substitution of an item for an equal item not always
    yields an equavalent condition. Here's an example:
    @code
    'a'='a '
    (LENGTH('a')=1) != (LENGTH('a ')=2)
  @endcode
    Such a substitution is surely valid if either the substituted
    field is not of a STRING type or if it is an argument of
    a comparison predicate.

  @retval
    true   substitution is valid
  @retval
    false  otherwise
*/

bool Item_field::subst_argument_checker(unsigned char **arg)
{
  return (result_type() != STRING_RESULT) || (*arg);
}


/**
  Set a pointer to the multiple equality the field reference belongs to
  (if any).

  The function looks for a multiple equality containing the field item
  among those referenced by arg.
  In the case such equality exists the function does the following.
  If the found multiple equality contains a constant, then the field
  reference is substituted for this constant, otherwise it sets a pointer
  to the multiple equality in the field item.


  @param arg    reference to list of multiple equalities where
                the field (this object) is to be looked for

  @note
    This function is supposed to be called as a callback parameter in calls
    of the compile method.

  @return
    - pointer to the replacing constant item, if the field item was substituted
    - pointer to the field item, otherwise.
*/

Item *Item_field::equal_fields_propagator(unsigned char *arg)
{
  if (no_const_subst)
    return this;
  item_equal= find_item_equal((COND_EQUAL *) arg);
  Item *item= 0;
  if (item_equal)
    item= item_equal->get_const();
  /*
    Disable const propagation for items used in different comparison contexts.
    This must be done because, for example, Item_hex_string->val_int() is not
    the same as (Item_hex_string->val_str() in BINARY column)->val_int().
    We cannot simply disable the replacement in a particular context (
    e.g. <bin_col> = <int_col> AND <bin_col> = <hex_string>) since
    Items don't know the context they are in and there are functions like
    IF (<hex_string>, 'yes', 'no').
    The same problem occurs when comparing a DATE/TIME field with a
    DATE/TIME represented as an int and as a string.
  */
  if (!item ||
      (cmp_context != (Item_result)-1 && item->cmp_context != cmp_context))
    item= this;

  return item;
}


/**
  Mark the item to not be part of substitution if it's not a binary item.

  See comments in Arg_comparator::set_compare_func() for details.
*/

bool Item_field::set_no_const_sub(unsigned char *)
{
  if (field->charset() != &my_charset_bin)
    no_const_subst=1;
  return false;
}


/**
  Replace an Item_field for an equal Item_field that evaluated earlier
  (if any).

  The function returns a pointer to an item that is taken from
  the very beginning of the item_equal list which the Item_field
  object refers to (belongs to) unless item_equal contains  a constant
  item. In this case the function returns this constant item,
  (if the substitution does not require conversion).
  If the Item_field object does not refer any Item_equal object
  'this' is returned .

  @param arg   a dummy parameter, is not used here


  @note
    This function is supposed to be called as a callback parameter in calls
    of the thransformer method.

  @return
    - pointer to a replacement Item_field if there is a better equal item or
      a pointer to a constant equal item;
    - this - otherwise.
*/

Item *Item_field::replace_equal_field(unsigned char *)
{
  if (item_equal)
  {
    Item *const_item_ptr= item_equal->get_const();
    if (const_item_ptr)
    {
      if (cmp_context != (Item_result)-1 &&
          const_item_ptr->cmp_context != cmp_context)
        return this;
      return const_item_ptr;
    }
    Item_field *subst= item_equal->get_first();
    if (subst && !field->eq(subst->field))
      return subst;
  }
  return this;
}


uint32_t Item_field::max_disp_length()
{
  return field->max_display_length();
}


/* ARGSUSED */
void Item_field::make_field(SendField *tmp_field)
{
  field->make_field(tmp_field);
  assert(tmp_field->table_name != 0);
  if (name)
    tmp_field->col_name=name;			// Use user supplied name
  if (table_name)
    tmp_field->table_name= table_name;
  if (db_name)
    tmp_field->db_name= db_name;
}


/**
  Set a field's value from a item.
*/

void Item_field::save_org_in_field(Field *to)
{
  if (field->is_null())
  {
    null_value=1;
    set_field_to_null_with_conversions(to, 1);
  }
  else
  {
    to->set_notnull();
    field_conv(to,field);
    null_value=0;
  }
}

int Item_field::save_in_field(Field *to, bool no_conversions)
{
  int res;
  if (result_field->is_null())
  {
    null_value=1;
    res= set_field_to_null_with_conversions(to, no_conversions);
  }
  else
  {
    to->set_notnull();
    res= field_conv(to,result_field);
    null_value=0;
  }
  return res;
}


void Item_field::send(plugin::Client *client, String *)
{
  client->store(result_field);
}


void Item_field::update_null_value()
{
  /*
    need to set no_errors to prevent warnings about type conversion
    popping up.
  */
  Session *session= field->getTable()->in_use;
  int no_errors;

  no_errors= session->no_errors;
  session->no_errors= 1;
  Item::update_null_value();
  session->no_errors= no_errors;
}


/*
  Add the field to the select list and substitute it for the reference to
  the field.

  SYNOPSIS
    Item_field::update_value_transformer()
    select_arg      current select

  DESCRIPTION
    If the field doesn't belong to the table being inserted into then it is
    added to the select list, pointer to it is stored in the ref_pointer_array
    of the select and the field itself is substituted for the Item_ref object.
    This is done in order to get correct values from update fields that
    belongs to the SELECT part in the INSERT .. SELECT .. ON DUPLICATE KEY
    UPDATE statement.

  RETURN
    0             if error occured
    ref           if all conditions are met
    this field    otherwise
*/

Item *Item_field::update_value_transformer(unsigned char *select_arg)
{
  Select_Lex *select= (Select_Lex*)select_arg;
  assert(fixed);

  if (field->getTable() != select->context.table_list->table)
  {
    List<Item> *all_fields= &select->join->all_fields;
    Item **ref_pointer_array= select->ref_pointer_array;
    int el= all_fields->size();
    Item_ref *ref;

    ref_pointer_array[el]= (Item*)this;
    all_fields->push_front((Item*)this);
    ref= new Item_ref(&select->context, ref_pointer_array + el,
                      table_name, field_name);
    return ref;
  }
  return this;
}


void Item_field::print(String *str)
{
  if (field && field->getTable()->const_table)
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),str->charset());
    field->val_str_internal(&tmp);
    if (field->is_null())  {
      str->append("NULL");
    }
    else {
      str->append('\'');
      str->append(tmp);
      str->append('\'');
    }
    return;
  }
  Item_ident::print(str);
}


} /* namespace drizzled */
