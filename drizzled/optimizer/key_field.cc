/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#include <config.h>

#include <drizzled/sql_select.h>
#include <drizzled/nested_join.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/table.h>
#include <drizzled/optimizer/key_field.h>
#include <drizzled/optimizer/key_use.h>
#include <drizzled/sql_lex.h>
#include <drizzled/item/subselect.h>

#include <vector>

using namespace std;

namespace drizzled
{

void optimizer::add_key_part(DYNAMIC_ARRAY *keyuse_array,
                             optimizer::KeyField *key_field)
{
  Field *field= key_field->getField();
  Table *form= field->getTable();

  if (key_field->isEqualityCondition() &&
      ! (key_field->getOptimizeFlags() & KEY_OPTIMIZE_EXISTS))
  {
    for (uint32_t key= 0; key < form->sizeKeys(); key++)
    {
      if (! (form->keys_in_use_for_query.test(key)))
        continue;

      uint32_t key_parts= (uint32_t) form->key_info[key].key_parts;
      for (uint32_t part= 0; part < key_parts; part++)
      {
        if (field->eq(form->key_info[key].key_part[part].field))
        {
          optimizer::KeyUse keyuse(field->getTable(),
                                   key_field->getValue(),
                                   key_field->getValue()->used_tables(),
                                   key,
                                   part,
                                   key_field->getOptimizeFlags() & KEY_OPTIMIZE_REF_OR_NULL,
                                   1 << part,
                                   0,
                                   key_field->rejectNullValues(),
                                   key_field->getConditionalGuard());
          keyuse_array->push_back(&keyuse);
        }
      }
    }
  }
}

void optimizer::add_key_fields_for_nj(Join *join,
                                      TableList *nested_join_table,
                                      optimizer::KeyField **end,
                                      uint32_t *and_level,
                                      vector<optimizer::SargableParam> &sargables)
{
  List<TableList>::iterator li(nested_join_table->getNestedJoin()->join_list.begin());
  List<TableList>::iterator li2(nested_join_table->getNestedJoin()->join_list.begin());
  bool have_another= false;
  table_map tables= 0;
  TableList *table;
  assert(nested_join_table->getNestedJoin());

  while ((table= li++) || (have_another && (li=li2, have_another=false,
                                            (table= li++))))
  {
    if (table->getNestedJoin())
    {
      if (! table->on_expr)
      {
        /* It's a semi-join nest. Walk into it as if it wasn't a nest */
        have_another= true;
        li2= li;
        li= List<TableList>::iterator(table->getNestedJoin()->join_list.begin());
      }
      else
        add_key_fields_for_nj(join, table, end, and_level, sargables);
    }
    else
      if (! table->on_expr)
        tables|= table->table->map;
  }
  if (nested_join_table->on_expr)
  {
    add_key_fields(join,
                   end,
                   and_level,
                   nested_join_table->on_expr,
                   tables,
                   sargables);
  }
}

optimizer::KeyField *optimizer::merge_key_fields(optimizer::KeyField *start,
                                                 optimizer::KeyField *new_fields,
                                                 optimizer::KeyField *end,
                                                 uint32_t and_level)
{
  if (start == new_fields)
    return start;				// Impossible or
  if (new_fields == end)
    return start;				// No new fields, skip all

  optimizer::KeyField *first_free= new_fields;

  /* Mark all found fields in old array */
  for (; new_fields != end; new_fields++)
  {
    for (optimizer::KeyField *old= start; old != first_free; old++)
    {
      if (old->getField() == new_fields->getField())
      {
        /*
          NOTE: below const_item() call really works as "!used_tables()", i.e.
          it can return false where it is feasible to make it return true.

          The cause is as follows: Some of the tables are already known to be
          const tables (the detection code is in make_join_statistics(),
          above the update_ref_and_keys() call), but we didn't propagate
          information about this: Table::const_table is not set to true, and
          Item::update_used_tables() hasn't been called for each item.
          The result of this is that we're missing some 'ref' accesses.
          TODO: OptimizerTeam: Fix this
        */
        if (! new_fields->getValue()->const_item())
        {
          /*
            If the value matches, we can use the key reference.
            If not, we keep it until we have examined all new values
          */
          if (old->getValue()->eq(new_fields->getValue(), old->getField()->binary()))
          {
            old->setLevel(and_level);
            old->setOptimizeFlags(((old->getOptimizeFlags() &
                                    new_fields->getOptimizeFlags() &
                                    KEY_OPTIMIZE_EXISTS) |
                                   ((old->getOptimizeFlags() |
                                     new_fields->getOptimizeFlags()) &
                                    KEY_OPTIMIZE_REF_OR_NULL)));
            old->setRejectNullValues(old->rejectNullValues() &&
                                     new_fields->rejectNullValues());
          }
        }
        else if (old->isEqualityCondition() &&
                 new_fields->isEqualityCondition() &&
                 old->getValue()->eq_by_collation(new_fields->getValue(),
                                                  old->getField()->binary(),
                                                  old->getField()->charset()))

        {
          old->setLevel(and_level);
          old->setOptimizeFlags(((old->getOptimizeFlags() &
                                  new_fields->getOptimizeFlags() &
                                  KEY_OPTIMIZE_EXISTS) |
                                 ((old->getOptimizeFlags() |
                                   new_fields->getOptimizeFlags()) &
                                 KEY_OPTIMIZE_REF_OR_NULL)));
          old->setRejectNullValues(old->rejectNullValues() &&
                                   new_fields->rejectNullValues());
        }
        else if (old->isEqualityCondition() &&
                 new_fields->isEqualityCondition() &&
                 ((old->getValue()->const_item() &&
                   old->getValue()->is_null()) ||
                   new_fields->getValue()->is_null()))
        {
          /* field = expression OR field IS NULL */
          old->setLevel(and_level);
          old->setOptimizeFlags(KEY_OPTIMIZE_REF_OR_NULL);
          /*
            Remember the NOT NULL value unless the value does not depend
            on other tables.
           */
          if (! old->getValue()->used_tables() &&
              old->getValue()->is_null())
          {
            old->setValue(new_fields->getValue());
          }
          /* The referred expression can be NULL: */
          old->setRejectNullValues(false);
        }
        else
        {
          /*
            We are comparing two different const.  In this case we can't
            use a key-lookup on this so it's better to remove the value
            and let the range optimzier handle it
          */
          if (old == --first_free)		// If last item
            break;
          *old= *first_free;			// Remove old value
          old--;				// Retry this value
        }
      }
    }
  }
  /* Remove all not used items */
  for (optimizer::KeyField *old= start; old != first_free;)
  {
    if (old->getLevel() != and_level)
    {						// Not used in all levels
      if (old == --first_free)
        break;
      *old= *first_free;			// Remove old value
      continue;
    }
    old++;
  }
  return first_free;
}

void optimizer::add_key_field(optimizer::KeyField **key_fields,
                              uint32_t and_level,
                              Item_func *cond,
                              Field *field,
                              bool eq_func,
                              Item **value,
                              uint32_t num_values,
                              table_map usable_tables,
                              vector<optimizer::SargableParam> &sargables)
{
  uint32_t exists_optimize= 0;
  if (! (field->flags & PART_KEY_FLAG))
  {
    // Don't remove column IS NULL on a LEFT JOIN table
    if (! eq_func || (*value)->type() != Item::NULL_ITEM ||
        ! field->getTable()->maybe_null || field->null_ptr)
      return;					// Not a key. Skip it
    exists_optimize= KEY_OPTIMIZE_EXISTS;
    assert(num_values == 1);
  }
  else
  {
    table_map used_tables= 0;
    bool optimizable= 0;
    for (uint32_t i= 0; i < num_values; i++)
    {
      used_tables|= (value[i])->used_tables();
      if (! ((value[i])->used_tables() & (field->getTable()->map | RAND_TABLE_BIT)))
        optimizable= 1;
    }
    if (! optimizable)
      return;
    if (! (usable_tables & field->getTable()->map))
    {
      if (! eq_func || (*value)->type() != Item::NULL_ITEM ||
          ! field->getTable()->maybe_null || field->null_ptr)
        return;					// Can't use left join optimize
      exists_optimize= KEY_OPTIMIZE_EXISTS;
    }
    else
    {
      JoinTable *stat= field->getTable()->reginfo.join_tab;
      key_map possible_keys= field->key_start;
      possible_keys&= field->getTable()->keys_in_use_for_query;
      stat[0].keys|= possible_keys;             // Add possible keys

      /*
        Save the following cases:
        Field op constant
        Field LIKE constant where constant doesn't start with a wildcard
        Field = field2 where field2 is in a different table
        Field op formula
        Field IS NULL
        Field IS NOT NULL
        Field BETWEEN ...
        Field IN ...
      */
      stat[0].key_dependent|= used_tables;

      bool is_const= 1;
      for (uint32_t i= 0; i < num_values; i++)
      {
        if (! (is_const&= value[i]->const_item()))
          break;
      }
      if (is_const)
        stat[0].const_keys|= possible_keys;
      else if (! eq_func)
      {
        /*
          Save info to be able check whether this predicate can be
          considered as sargable for range analisis after reading const tables.
          We do not save info about equalities as update_const_equal_items
          will take care of updating info on keys from sargable equalities.
        */
        optimizer::SargableParam tmp(field, value, num_values);
        sargables.push_back(tmp);
      }
      /*
        We can't always use indexes when comparing a string index to a
        number. cmp_type() is checked to allow compare of dates to numbers.
        eq_func is NEVER true when num_values > 1
       */
      if (! eq_func)
      {
        /*
          Additional optimization: if we're processing
          "t.key BETWEEN c1 AND c1" then proceed as if we were processing
          "t.key = c1".
          TODO: This is a very limited fix. A more generic fix is possible.
          There are 2 options:
          A) Make equality propagation code be able to handle BETWEEN
             (including cases like t1.key BETWEEN t2.key AND t3.key)
          B) Make range optimizer to infer additional "t.key = c" equalities
             and use them in equality propagation process (see details in
             OptimizerKBAndTodo)
        */
        if ((cond->functype() != Item_func::BETWEEN) ||
            ((Item_func_between*) cond)->negated ||
            ! value[0]->eq(value[1], field->binary()))
          return;
        eq_func= true;
      }

      if (field->result_type() == STRING_RESULT)
      {
        if ((*value)->result_type() != STRING_RESULT)
        {
          if (field->cmp_type() != (*value)->result_type())
            return;
        }
        else
        {
          /*
            We can't use indexes if the effective collation
            of the operation differ from the field collation.
          */
          if (field->cmp_type() == STRING_RESULT &&
              ((Field_str*)field)->charset() != cond->compare_collation())
            return;
        }
      }
    }
  }
  /*
    For the moment eq_func is always true. This slot is reserved for future
    extensions where we want to remembers other things than just eq comparisons
  */
  assert(eq_func);
  /* Store possible eq field */
  (*key_fields)->setField(field);
  (*key_fields)->setEqualityConditionUsed(eq_func);
  (*key_fields)->setValue(*value);
  (*key_fields)->setLevel(and_level);
  (*key_fields)->setOptimizeFlags(exists_optimize);
  /*
    If the condition has form "tbl.keypart = othertbl.field" and
    othertbl.field can be NULL, there will be no matches if othertbl.field
    has NULL value.
    We use null_rejecting in add_not_null_conds() to add
    'othertbl.field IS NOT NULL' to tab->select_cond.
  */
  (*key_fields)->setRejectNullValues((cond->functype() == Item_func::EQ_FUNC ||
                                      cond->functype() == Item_func::MULT_EQUAL_FUNC) &&
                                     ((*value)->type() == Item::FIELD_ITEM) &&
                                     ((Item_field*)*value)->field->maybe_null());
  (*key_fields)->setConditionalGuard(NULL);
  (*key_fields)++;
}

void optimizer::add_key_equal_fields(optimizer::KeyField **key_fields,
                                     uint32_t and_level,
                                     Item_func *cond,
                                     Item_field *field_item,
                                     bool eq_func,
                                     Item **val,
                                     uint32_t num_values,
                                     table_map usable_tables,
                                     vector<optimizer::SargableParam> &sargables)
{
  Field *field= field_item->field;
  add_key_field(key_fields, and_level, cond, field,
                eq_func, val, num_values, usable_tables, sargables);
  Item_equal *item_equal= field_item->item_equal;
  if (item_equal)
  {
    /*
      Add to the set of possible key values every substitution of
      the field for an equal field included into item_equal
    */
    Item_equal_iterator it(item_equal->begin());
    Item_field *item;
    while ((item= it++))
    {
      if (! field->eq(item->field))
      {
        add_key_field(key_fields, and_level, cond, item->field,
                      eq_func, val, num_values, usable_tables,
                      sargables);
      }
    }
  }
}

void optimizer::add_key_fields(Join *join,
                               optimizer::KeyField **key_fields,
                               uint32_t *and_level,
                               COND *cond,
                               table_map usable_tables,
                               vector<optimizer::SargableParam> &sargables)
{
  if (cond->type() == Item_func::COND_ITEM)
  {
    List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
    optimizer::KeyField *org_key_fields= *key_fields;

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item= li++))
      {
        add_key_fields(join,
                       key_fields,
                       and_level,
                       item,
                       usable_tables,
                       sargables);
      }
      for (; org_key_fields != *key_fields; org_key_fields++)
        org_key_fields->setLevel(*and_level);
    }
    else
    {
      (*and_level)++;
      add_key_fields(join,
                     key_fields,
                     and_level,
                     li++,
                     usable_tables,
                     sargables);
      Item *item;
      while ((item= li++))
      {
        optimizer::KeyField *start_key_fields= *key_fields;
        (*and_level)++;
        add_key_fields(join,
                       key_fields,
                       and_level,
                       item,
                       usable_tables,
                       sargables);
        *key_fields= merge_key_fields(org_key_fields, start_key_fields,
                                      *key_fields, ++(*and_level));
      }
    }
    return;
  }

  /*
    Subquery optimization: Conditions that are pushed down into subqueries
    are wrapped into Item_func_trig_cond. We process the wrapped condition
    but need to set cond_guard for KeyUse elements generated from it.
  */
  {
    if (cond->type() == Item::FUNC_ITEM &&
        ((Item_func*)cond)->functype() == Item_func::TRIG_COND_FUNC)
    {
      Item *cond_arg= ((Item_func*)cond)->arguments()[0];
      if (! join->group_list &&
          ! join->order &&
          join->unit->item &&
          join->unit->item->substype() == Item_subselect::IN_SUBS &&
          ! join->unit->is_union())
      {
        optimizer::KeyField *save= *key_fields;
        add_key_fields(join,
                       key_fields,
                       and_level,
                       cond_arg,
                       usable_tables,
                       sargables);
        /* Indicate that this ref access candidate is for subquery lookup */
        for (; save != *key_fields; save++)
          save->setConditionalGuard(((Item_func_trig_cond*)cond)->get_trig_var());
      }
      return;
    }
  }

  /* If item is of type 'field op field/constant' add it to key_fields */
  if (cond->type() != Item::FUNC_ITEM)
    return;
  Item_func *cond_func= (Item_func*) cond;
  switch (cond_func->select_optimize())
  {
  case Item_func::OPTIMIZE_NONE:
    break;
  case Item_func::OPTIMIZE_KEY:
  {
    Item **values;
    // BETWEEN, IN, NE
    if (cond_func->key_item()->real_item()->type() == Item::FIELD_ITEM &&
        ! (cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      values= cond_func->arguments() + 1;
      if (cond_func->functype() == Item_func::NE_FUNC &&
          cond_func->arguments()[1]->real_item()->type() == Item::FIELD_ITEM &&
          ! (cond_func->arguments()[0]->used_tables() & OUTER_REF_TABLE_BIT))
        values--;
      assert(cond_func->functype() != Item_func::IN_FUNC ||
             cond_func->argument_count() != 2);
      add_key_equal_fields(key_fields, *and_level, cond_func,
                           (Item_field*) (cond_func->key_item()->real_item()),
                           0, values,
                           cond_func->argument_count()-1,
                           usable_tables, sargables);
    }
    if (cond_func->functype() == Item_func::BETWEEN)
    {
      values= cond_func->arguments();
      for (uint32_t i= 1 ; i < cond_func->argument_count(); i++)
      {
        Item_field *field_item;
        if (cond_func->arguments()[i]->real_item()->type() == Item::FIELD_ITEM
            &&
            ! (cond_func->arguments()[i]->used_tables() & OUTER_REF_TABLE_BIT))
        {
          field_item= (Item_field *) (cond_func->arguments()[i]->real_item());
          add_key_equal_fields(key_fields, *and_level, cond_func,
                               field_item, 0, values, 1, usable_tables,
                               sargables);
        }
      }
    }
    break;
  }
  case Item_func::OPTIMIZE_OP:
  {
    bool equal_func= (cond_func->functype() == Item_func::EQ_FUNC ||
		                  cond_func->functype() == Item_func::EQUAL_FUNC);

    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM &&
        ! (cond_func->arguments()[0]->used_tables() & OUTER_REF_TABLE_BIT))
    {
      add_key_equal_fields(key_fields, *and_level, cond_func,
                           (Item_field*) (cond_func->arguments()[0])->real_item(),
                           equal_func,
                           cond_func->arguments()+1, 1, usable_tables,
                           sargables);
    }
    if (cond_func->arguments()[1]->real_item()->type() == Item::FIELD_ITEM &&
        cond_func->functype() != Item_func::LIKE_FUNC &&
        ! (cond_func->arguments()[1]->used_tables() & OUTER_REF_TABLE_BIT))
    {
      add_key_equal_fields(key_fields, *and_level, cond_func,
                           (Item_field*) (cond_func->arguments()[1])->real_item(), equal_func,
                           cond_func->arguments(),1,usable_tables,
                           sargables);
    }
    break;
  }
  case Item_func::OPTIMIZE_NULL:
    /* column_name IS [NOT] NULL */
    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM &&
        ! (cond_func->used_tables() & OUTER_REF_TABLE_BIT))
    {
      Item *tmp= new Item_null;
      if (unlikely(! tmp))                       // Should never be true
        return;
      add_key_equal_fields(key_fields, *and_level, cond_func,
                           (Item_field*) (cond_func->arguments()[0])->real_item(),
                           cond_func->functype() == Item_func::ISNULL_FUNC,
                           &tmp, 1, usable_tables, sargables);
    }
    break;
  case Item_func::OPTIMIZE_EQUAL:
    Item_equal *item_equal= (Item_equal *) cond;
    Item *const_item= item_equal->get_const();
    Item_equal_iterator it(item_equal->begin());
    Item_field *item;
    if (const_item)
    {
      /*
        For each field field1 from item_equal consider the equality
        field1=const_item as a condition allowing an index access of the table
        with field1 by the keys value of field1.
      */
      while ((item= it++))
      {
        add_key_field(key_fields, *and_level, cond_func, item->field,
                      true, &const_item, 1, usable_tables, sargables);
      }
    }
    else
    {
      /*
        Consider all pairs of different fields included into item_equal.
        For each of them (field1, field1) consider the equality
        field1=field2 as a condition allowing an index access of the table
        with field1 by the keys value of field2.
      */
      Item_equal_iterator fi(item_equal->begin());
      while ((item= fi++))
      {
        Field *field= item->field;
        while ((item= it++))
        {
          if (! field->eq(item->field))
          {
            add_key_field(key_fields, *and_level, cond_func, field,
                          true, (Item **) &item, 1, usable_tables,
                          sargables);
          }
        }
        it= item_equal->begin();
      }
    }
    break;
  }
}

} /* namespace drizzled */
