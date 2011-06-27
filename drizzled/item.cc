/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/show.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/cache_row.h>
#include <drizzled/item/type_holder.h>
#include <drizzled/item/sum.h>
#include <drizzled/item/copy_string.h>
#include <drizzled/function/str/conv_charset.h>
#include <drizzled/sql_base.h>
#include <drizzled/util/convert.h>
#include <drizzled/plugin/client.h>
#include <drizzled/time_functions.h>
#include <drizzled/field/str.h>
#include <drizzled/field/num.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/date.h>
#include <drizzled/field/datetime.h>
#include <drizzled/field/decimal.h>
#include <drizzled/field/double.h>
#include <drizzled/field/enum.h>
#include <drizzled/field/epoch.h>
#include <drizzled/field/int32.h>
#include <drizzled/field/int64.h>
#include <drizzled/field/microtime.h>
#include <drizzled/field/null.h>
#include <drizzled/field/real.h>
#include <drizzled/field/size.h>
#include <drizzled/field/time.h>
#include <drizzled/field/varstring.h>
#include <drizzled/current_session.h>
#include <drizzled/session.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/item/ref.h>
#include <drizzled/item/subselect.h>
#include <drizzled/sql_lex.h>
#include <drizzled/system_variables.h>

#include <cstdio>
#include <math.h>
#include <algorithm>
#include <float.h>

using namespace std;

namespace drizzled
{

const String my_null_string("NULL", 4, default_charset_info);

bool Item::is_expensive_processor(unsigned char *)
{
  return false;
}

void Item::fix_after_pullout(Select_Lex *, Item **)
{}

Field *Item::tmp_table_field(Table *)
{
  return NULL;
}

const char *Item::full_name(void) const
{
  return name ? name : "???";
}

int64_t Item::val_int_endpoint(bool, bool *)
{
  assert(0);
  return 0;
}

/** @todo Make this functions class dependent */
bool Item::val_bool()
{
  switch(result_type()) 
  {
    case INT_RESULT:
      return val_int() != 0;

    case DECIMAL_RESULT:
    {
      type::Decimal decimal_value;
      type::Decimal *val= val_decimal(&decimal_value);
      if (val)
        return not val->isZero();
      return false;
    }

    case REAL_RESULT:
    case STRING_RESULT:
      return val_real() != 0.0;

    case ROW_RESULT:
      assert(0);
      abort();
  }

  assert(0);
  abort();
}

String *Item::val_string_from_real(String *str)
{
  double nr= val_real();
  if (null_value)
    return NULL;

  str->set_real(nr, decimals, &my_charset_bin);
  return str;
}

String *Item::val_string_from_int(String *str)
{
  int64_t nr= val_int();
  if (null_value)
    return NULL;

  str->set_int(nr, unsigned_flag, &my_charset_bin);
  return str;
}

String *Item::val_string_from_decimal(String *str)
{
  type::Decimal dec_buf, *dec= val_decimal(&dec_buf);
  if (null_value)
    return NULL;

  class_decimal_round(E_DEC_FATAL_ERROR, dec, decimals, false, &dec_buf);
  class_decimal2string(&dec_buf, 0, str);
  return str;
}

type::Decimal *Item::val_decimal_from_real(type::Decimal *decimal_value)
{
  double nr= val_real();
  if (null_value)
    return NULL;

  double2_class_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
  return (decimal_value);
}

type::Decimal *Item::val_decimal_from_int(type::Decimal *decimal_value)
{
  int64_t nr= val_int();
  if (null_value)
    return NULL;

  int2_class_decimal(E_DEC_FATAL_ERROR, nr, unsigned_flag, decimal_value);
  return decimal_value;
}

type::Decimal *Item::val_decimal_from_string(type::Decimal *decimal_value)
{
  String *res;
  if (!(res= val_str(&str_value)))
    return NULL;

  if (decimal_value->store(E_DEC_FATAL_ERROR & ~E_DEC_BAD_NUM,
                     res->ptr(), 
                     res->length(), 
                     res->charset()) & E_DEC_BAD_NUM)
  {
    push_warning_printf(&getSession(), 
                        DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "DECIMAL",
                        str_value.c_ptr());
  }
  return decimal_value;
}

type::Decimal *Item::val_decimal_from_date(type::Decimal *decimal_value)
{
  assert(fixed);
  type::Time ltime;
  if (get_date(ltime, TIME_FUZZY_DATE))
  {
    decimal_value->set_zero();
    null_value= 1;                               // set NULL, stop processing
    return NULL;
  }
  return date2_class_decimal(&ltime, decimal_value);
}

type::Decimal *Item::val_decimal_from_time(type::Decimal *decimal_value)
{
  assert(fixed);
  type::Time ltime;
  if (get_time(ltime))
  {
    decimal_value->set_zero();
    return NULL;
  }
  return date2_class_decimal(&ltime, decimal_value);
}

double Item::val_real_from_decimal()
{
  /* Note that fix_fields may not be called for Item_avg_field items */
  double result;
  type::Decimal value_buff, *dec_val= val_decimal(&value_buff);
  if (null_value)
    return 0.0;
  class_decimal2double(E_DEC_FATAL_ERROR, dec_val, &result);
  return result;
}

int64_t Item::val_int_from_decimal()
{
  /* Note that fix_fields may not be called for Item_avg_field items */
  int64_t result;
  type::Decimal value, *dec_val= val_decimal(&value);

  if (null_value)
    return 0;
  dec_val->val_int32(E_DEC_FATAL_ERROR, unsigned_flag, &result);

  return result;
}

bool Item::save_time_in_field(Field *field)
{
  type::Time ltime;

  if (get_time(ltime))
    return set_field_to_null(field);

  field->set_notnull();

  return field->store_time(ltime, type::DRIZZLE_TIMESTAMP_TIME);
}

bool Item::save_date_in_field(Field *field)
{
  type::Time ltime;

  if (get_date(ltime, TIME_FUZZY_DATE))
    return set_field_to_null(field);

  field->set_notnull();

  return field->store_time(ltime, type::DRIZZLE_TIMESTAMP_DATETIME);
}

/**
 * Check if the Item is null and stores the NULL or the
 * result value in the field accordingly.
 */
int Item::save_str_value_in_field(Field *field, String *result)
{
  if (null_value)
    return set_field_to_null(field);

  field->set_notnull();

  return field->store(result->ptr(), result->length(), collation.collation);
}

Item::Item():
  is_expensive_cache(-1),
  name(0), 
  name_length(0),
  orig_name(0), 
  max_length(0), 
  marker(0),
  decimals(0),
  fixed(false),
  maybe_null(false),
  null_value(false),
  unsigned_flag(false), 
  with_sum_func(false),
  is_autogenerated_name(true),
  with_subselect(false),
  collation(&my_charset_bin, DERIVATION_COERCIBLE),
  _session(*current_session)
{
  cmp_context= (Item_result)-1;

  /* Put item in free list so that we can free all items at end */
  next= getSession().free_list;
  getSession().free_list= this;

  /*
    Item constructor can be called during execution other then SQL_COM
    command => we should check session->lex().current_select on zero (session->lex
    can be uninitialised)
  */
  if (getSession().lex().current_select)
  {
    enum_parsing_place place= getSession().lex().current_select->parsing_place;
    if (place == SELECT_LIST || place == IN_HAVING)
      getSession().lex().current_select->select_n_having_items++;
  }
}

Item::Item(Session *session, Item *item):
  is_expensive_cache(-1),
  str_value(item->str_value),
  name(item->name),
  name_length(item->name_length),
  orig_name(item->orig_name),
  max_length(item->max_length),
  marker(item->marker),
  decimals(item->decimals),
  fixed(item->fixed),
  maybe_null(item->maybe_null),
  null_value(item->null_value),
  unsigned_flag(item->unsigned_flag),
  with_sum_func(item->with_sum_func),
  is_autogenerated_name(item->is_autogenerated_name),
  with_subselect(item->with_subselect),
  collation(item->collation),
  cmp_context(item->cmp_context),
  _session(*session)
{
  /* Put this item in the session's free list */
  next= getSession().free_list;
  getSession().free_list= this;
}

uint32_t Item::float_length(uint32_t decimals_par) const
{
  return decimals != NOT_FIXED_DEC ? (DBL_DIG+2+decimals_par) : DBL_DIG+8;
}

uint32_t Item::decimal_precision() const
{
  Item_result restype= result_type();

  if ((restype == DECIMAL_RESULT) || (restype == INT_RESULT))
    return min(class_decimal_length_to_precision(max_length, decimals, unsigned_flag),
               (uint32_t) DECIMAL_MAX_PRECISION);
  return min(max_length, (uint32_t) DECIMAL_MAX_PRECISION);
}

int Item::decimal_int_part() const
{
  return class_decimal_int_part(decimal_precision(), decimals);
}

void Item::print(String *str)
{
  str->append(full_name());
}

void Item::print_item_w_name(String *str)
{
  print(str);

  if (name)
  {
    str->append(STRING_WITH_LEN(" AS "));
    str->append_identifier(name, (uint32_t) strlen(name));
  }
}

void Item::split_sum_func(Session *, Item **, List<Item> &)
{}

void Item::cleanup()
{
  fixed= false;
  marker= 0;
  if (orig_name)
    name= orig_name;
  return;
}

void Item::rename(char *new_name)
{
  /*
    we can compare pointers to names here, because if name was not changed,
    pointer will be same
  */
  if (! orig_name && new_name != name)
    orig_name= name;
  name= new_name;
}

Item* Item::transform(Item_transformer transformer, unsigned char *arg)
{
  return (this->*transformer)(arg);
}

bool Item::check_cols(uint32_t c)
{
  if (c != 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return true;
  }
  return false;
}

void Item::set_name(const char *str, uint32_t length, const charset_info_st * const cs)
{
  if (!length)
  {
    /* Empty string, used by AS or internal function like last_insert_id() */
    name= (char*) str;
    name_length= 0;
    return;
  }
  if (cs->ctype)
  {
    uint32_t orig_len= length;
    while (length && ! my_isgraph(cs, *str))
    {
      /* Fix problem with yacc */
      length--;
      str++;
    }
    if (orig_len != length && ! is_autogenerated_name)
    {
      if (length == 0)
        push_warning_printf(&getSession(), 
                            DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_NAME_BECOMES_EMPTY, 
                            ER(ER_NAME_BECOMES_EMPTY),
                            str + length - orig_len);
      else
        push_warning_printf(&getSession(),
                            DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_REMOVED_SPACES, 
                            ER(ER_REMOVED_SPACES),
                            str + length - orig_len);
    }
  }
  name= memory::sql_strmake(str, length);
}

bool Item::eq(const Item *item, bool) const
{
  /*
    Note, that this is never true if item is a Item_param:
    for all basic constants we have special checks, and Item_param's
    type() can be only among basic constant types.
  */
  return type() == item->type() && 
         name && 
         item->name &&
         ! my_strcasecmp(system_charset_info, name, item->name);
}

Item *Item::safe_charset_converter(const charset_info_st * const tocs)
{
  Item_func_conv_charset *conv= new Item_func_conv_charset(this, tocs, 1);
  return conv->safe ? conv : NULL;
}

bool Item::get_date(type::Time &ltime,uint32_t fuzzydate)
{
  do
  {
    if (is_null())
    {
      break;
    }
    else if (result_type() == STRING_RESULT)
    {
      char buff[type::Time::MAX_STRING_LENGTH];
      String tmp(buff,sizeof(buff), &my_charset_bin),*res;
      if (!(res=val_str(&tmp)) ||
          str_to_datetime_with_warn(&getSession(), res->ptr(), res->length(),
                                    &ltime, fuzzydate) <= type::DRIZZLE_TIMESTAMP_ERROR)
      {
        break;
      }
    }
    else
    {
      int64_t value= val_int();
      type::datetime_t date_value;

      ltime.convert(date_value, value, fuzzydate);

      if (not type::is_valid(date_value))
      {
        char buff[DECIMAL_LONGLONG_DIGITS], *end;
        end= internal::int64_t10_to_str(value, buff, -10);
        make_truncated_value_warning(&getSession(), DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                     buff, (int) (end-buff), type::DRIZZLE_TIMESTAMP_NONE, NULL);
        break;
      }
    }

    return false;
  } while (0);

  ltime.reset();

  return true;
}

bool Item::get_time(type::Time &ltime)
{
  char buff[type::Time::MAX_STRING_LENGTH];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  if (!(res=val_str(&tmp)) or
      str_to_time_with_warn(&getSession(), res->ptr(), res->length(), &ltime))
  {
    ltime.reset();

    return true;
  }

  return false;
}

bool Item::get_date_result(type::Time &ltime,uint32_t fuzzydate)
{
  return get_date(ltime, fuzzydate);
}

bool Item::is_null()
{
  return false;
}

void Item::update_null_value ()
{
  (void) val_int();
}

void Item::top_level_item(void)
{}

void Item::set_result_field(Field *)
{}

bool Item::is_result_field(void)
{
  return false;
}

bool Item::is_bool_func(void)
{
  return false;
}

void Item::save_in_result_field(bool)
{}

void Item::no_rows_in_result(void)
{}

Item *Item::copy_or_same(Session *)
{
  return this;
}

Item *Item::copy_andor_structure(Session *)
{
  return this;
}

Item *Item::real_item(void)
{
  return this;
}

const Item *Item::real_item(void) const
{
  return this;
}

Item *Item::get_tmp_table_item(Session *session)
{
  return copy_or_same(session);
}

const charset_info_st *Item::default_charset()
{
  return current_session->variables.getCollation();
}

const charset_info_st *Item::compare_collation()
{
  return NULL;
}

bool Item::walk(Item_processor processor, bool, unsigned char *arg)
{
  return (this->*processor)(arg);
}

Item* Item::compile(Item_analyzer analyzer, 
                    unsigned char **arg_p,
                    Item_transformer transformer, 
                    unsigned char *arg_t)
{
  if ((this->*analyzer) (arg_p))
    return ((this->*transformer) (arg_t));
  return NULL;
}

void Item::traverse_cond(Cond_traverser traverser, void *arg, traverse_order)
{
  (*traverser)(this, arg);
}

bool Item::remove_dependence_processor(unsigned char *)
{
  return false;
}

bool Item::remove_fixed(unsigned char *)
{
  fixed= false;
  return false;
}

bool Item::collect_item_field_processor(unsigned char *)
{
  return false;
}

bool Item::find_item_in_field_list_processor(unsigned char *)
{
  return false;
}

bool Item::change_context_processor(unsigned char *)
{
  return false;
}

bool Item::register_field_in_read_map(unsigned char *)
{
  return false;
}

bool Item::subst_argument_checker(unsigned char **arg)
{
  if (*arg)
    *arg= NULL;
  return true;
}

Item *Item::equal_fields_propagator(unsigned char *)
{
  return this;
}

bool Item::set_no_const_sub(unsigned char *)
{
  return false;
}

Item *Item::replace_equal_field(unsigned char *)
{
  return this;
}

uint32_t Item::cols()
{
  return 1;
}

Item* Item::element_index(uint32_t)
{
  return this;
}

Item** Item::addr(uint32_t)
{
  return NULL;
}

bool Item::null_inside()
{
  return false;
}

void Item::bring_value()
{}

Item *Item::neg_transformer(Session *)
{
  return NULL;
}

Item *Item::update_value_transformer(unsigned char *)
{
  return this;
}

void Item::delete_self()
{
  cleanup();
  delete this;
}

bool Item::result_as_int64_t()
{
  return false;
}

bool Item::is_expensive()
{
  if (is_expensive_cache < 0)
    is_expensive_cache= walk(&Item::is_expensive_processor, 0,
                             (unsigned char*)0);
  return test(is_expensive_cache);
}

/*
 need a special class to adjust printing : references to aggregate functions
 must not be printed as refs because the aggregate functions that are added to
 the front of select list are not printed as well.
*/
class Item_aggregate_ref : public Item_ref
{
public:
  Item_aggregate_ref(Name_resolution_context *context_arg, Item **item,
                  const char *table_name_arg, const char *field_name_arg)
    :Item_ref(context_arg, item, table_name_arg, field_name_arg) {}

  virtual inline void print (String *str)
  {
    if (ref)
      (*ref)->print(str);
    else
      Item_ident::print(str);
  }
};

void Item::split_sum_func(Session *session, Item **ref_pointer_array,
                          List<Item> &fields, Item **ref,
                          bool skip_registered)
{
  /* An item of type Item_sum  is registered <=> ref_by != 0 */
  if (type() == SUM_FUNC_ITEM && 
      skip_registered &&
      ((Item_sum *) this)->ref_by)
    return;

  if ((type() != SUM_FUNC_ITEM && with_sum_func) ||
      (type() == FUNC_ITEM &&
       (((Item_func *) this)->functype() == Item_func::ISNOTNULLTEST_FUNC ||
        ((Item_func *) this)->functype() == Item_func::TRIG_COND_FUNC)))
  {
    /* Will split complicated items and ignore simple ones */
    split_sum_func(session, ref_pointer_array, fields);
  }
  else if ((type() == SUM_FUNC_ITEM || (used_tables() & ~PARAM_TABLE_BIT)) &&
           type() != SUBSELECT_ITEM &&
           type() != REF_ITEM)
  {
    /*
      Replace item with a reference so that we can easily calculate
      it (in case of sum functions) or copy it (in case of fields)

      The test above is to ensure we don't do a reference for things
      that are constants (PARAM_TABLE_BIT is in effect a constant)
      or already referenced (for example an item in HAVING)
      Exception is Item_direct_view_ref which we need to convert to
      Item_ref to allow fields from view being stored in tmp table.
    */
    Item_aggregate_ref *item_ref;
    uint32_t el= fields.size();
    Item *real_itm= real_item();

    ref_pointer_array[el]= real_itm;
    item_ref= new Item_aggregate_ref(&session->lex().current_select->context, ref_pointer_array + el, 0, name);
    if (type() == SUM_FUNC_ITEM)
      item_ref->depended_from= ((Item_sum *) this)->depended_from();
    fields.push_front(real_itm);
    *ref= item_ref;
  }
}

/*
  Functions to convert item to field (for send_fields)
*/
bool Item::fix_fields(Session *, Item **)
{
  /* We do not check fields which are fixed during construction */
  assert(! fixed || basic_const_item());
  fixed= true;
  return false;
}

void mark_as_dependent(Session *session, Select_Lex *last, Select_Lex *current,
                              Item_ident *resolved_item,
                              Item_ident *mark_item)
{
  const char *db_name= (resolved_item->db_name ?
                        resolved_item->db_name : "");
  const char *table_name= (resolved_item->table_name ?
                           resolved_item->table_name : "");
  /* store pointer on Select_Lex from which item is dependent */
  if (mark_item)
    mark_item->depended_from= last;
  current->mark_as_dependent(last);
  if (session->lex().describe & DESCRIBE_EXTENDED)
  {
    char warn_buff[DRIZZLE_ERRMSG_SIZE];
    snprintf(warn_buff, sizeof(warn_buff), ER(ER_WARN_FIELD_RESOLVED),
            db_name, (db_name[0] ? "." : ""),
            table_name, (table_name [0] ? "." : ""),
            resolved_item->field_name,
	    current->select_number, last->select_number);
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
		 ER_WARN_FIELD_RESOLVED, warn_buff);
  }
}

void mark_select_range_as_dependent(Session *session,
                                    Select_Lex *last_select,
                                    Select_Lex *current_sel,
                                    Field *found_field, Item *found_item,
                                    Item_ident *resolved_item)
{
  /*
    Go from current SELECT to SELECT where field was resolved (it
    have to be reachable from current SELECT, because it was already
    done once when we resolved this field and cached result of
    resolving)
  */
  Select_Lex *previous_select= current_sel;
  for (; previous_select->outer_select() != last_select;
       previous_select= previous_select->outer_select())
  {
    Item_subselect *prev_subselect_item= previous_select->master_unit()->item;
    prev_subselect_item->used_tables_cache|= OUTER_REF_TABLE_BIT;
    prev_subselect_item->const_item_cache= false;
  }
  {
    Item_subselect *prev_subselect_item= previous_select->master_unit()->item;
    Item_ident *dependent= resolved_item;
    if (found_field == view_ref_found)
    {
      Item::Type type= found_item->type();
      prev_subselect_item->used_tables_cache|= found_item->used_tables();
      dependent= ((type == Item::REF_ITEM || type == Item::FIELD_ITEM) ?
                  (Item_ident*) found_item :
                  0);
    }
    else
      prev_subselect_item->used_tables_cache|= found_field->getTable()->map;
    prev_subselect_item->const_item_cache= false;
    mark_as_dependent(session, last_select, current_sel, resolved_item,
                      dependent);
  }
}

/**
  Search a GROUP BY clause for a field with a certain name.

  Search the GROUP BY list for a column named as find_item. When searching
  preference is given to columns that are qualified with the same table (and
  database) name as the one being searched for.

  @param find_item     the item being searched for
  @param group_list    GROUP BY clause

  @return
    - the found item on success
    - NULL if find_item is not in group_list
*/
static Item** find_field_in_group_list(Session *session, Item *find_item, Order *group_list)
{
  const char *db_name;
  const char *table_name;
  const char *field_name;
  Order *found_group= NULL;
  int found_match_degree= 0;
  Item_ident *cur_field;
  int cur_match_degree= 0;
  char name_buff[NAME_LEN+1];

  if (find_item->type() == Item::FIELD_ITEM ||
      find_item->type() == Item::REF_ITEM)
  {
    db_name= ((Item_ident*) find_item)->db_name;
    table_name= ((Item_ident*) find_item)->table_name;
    field_name= ((Item_ident*) find_item)->field_name;
  }
  else
    return NULL;

  if (db_name)
  {
    /* Convert database to lower case for comparison */
    strncpy(name_buff, db_name, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db_name= name_buff;
  }

  assert(field_name != 0);

  for (Order *cur_group= group_list ; cur_group ; cur_group= cur_group->next)
  {
    if ((*(cur_group->item))->real_item()->type() == Item::FIELD_ITEM)
    {
      cur_field= (Item_ident*) *cur_group->item;
      cur_match_degree= 0;

      assert(cur_field->field_name != 0);

      if (! my_strcasecmp(system_charset_info, cur_field->field_name, field_name))
        ++cur_match_degree;
      else
        continue;

      if (cur_field->table_name && table_name)
      {
        /* If field_name is qualified by a table name. */
        if (my_strcasecmp(table_alias_charset, cur_field->table_name, table_name))
          /* Same field names, different tables. */
          return NULL;

        ++cur_match_degree;
        if (cur_field->db_name && db_name)
        {
          /* If field_name is also qualified by a database name. */
          if (my_strcasecmp(system_charset_info, cur_field->db_name, db_name))
          {
            /* Same field names, different databases. */
            return NULL;
          }
          ++cur_match_degree;
        }
      }

      if (cur_match_degree > found_match_degree)
      {
        found_match_degree= cur_match_degree;
        found_group= cur_group;
      }
      else if (found_group &&
               (cur_match_degree == found_match_degree) &&
               ! (*(found_group->item))->eq(cur_field, 0))
      {
        /*
          If the current resolve candidate matches equally well as the current
          best match, they must reference the same column, otherwise the field
          is ambiguous.
        */
        my_error(ER_NON_UNIQ_ERROR, MYF(0), find_item->full_name(), session->where());
        return NULL;
      }
    }
  }

  if (found_group)
    return found_group->item;

  return NULL;
}

Item** resolve_ref_in_select_and_group(Session *session, Item_ident *ref, Select_Lex *select)
{
  Item **group_by_ref= NULL;
  Item **select_ref= NULL;
  Order *group_list= (Order*) select->group_list.first;
  bool ambiguous_fields= false;
  uint32_t counter;
  enum_resolution_type resolution;

  /*
    Search for a column or derived column named as 'ref' in the SELECT
    clause of the current select.
  */
  if (!(select_ref= find_item_in_list(session,
                                      ref, *(select->get_item_list()),
                                      &counter, REPORT_EXCEPT_NOT_FOUND,
                                      &resolution)))
    return NULL; /* Some error occurred. */
  if (resolution == RESOLVED_AGAINST_ALIAS)
    ref->alias_name_used= true;

  /* If this is a non-aggregated field inside HAVING, search in GROUP BY. */
  if (select->having_fix_field && !ref->with_sum_func && group_list)
  {
    group_by_ref= find_field_in_group_list(session, ref, group_list);

    /* Check if the fields found in SELECT and GROUP BY are the same field. */
    if (group_by_ref && (select_ref != not_found_item) &&
        !((*group_by_ref)->eq(*select_ref, 0)))
    {
      ambiguous_fields= true;
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_NON_UNIQ_ERROR,
                          ER(ER_NON_UNIQ_ERROR), ref->full_name(),
                          session->where());

    }
  }

  if (select_ref != not_found_item || group_by_ref)
  {
    if (select_ref != not_found_item && !ambiguous_fields)
    {
      assert(*select_ref != 0);
      if (!select->ref_pointer_array[counter])
      {
        my_error(ER_ILLEGAL_REFERENCE, MYF(0),
                 ref->name, "forward reference in item list");
        return NULL;
      }
      assert((*select_ref)->fixed);
      return (select->ref_pointer_array + counter);
    }
    if (group_by_ref)
      return group_by_ref;
    assert(false);
    return NULL; /* So there is no compiler warning. */
  }

  return (Item**) not_found_item;
}

void Item::init_make_field(SendField *tmp_field,
                           enum enum_field_types field_type_arg)
{
  char *empty_name= (char*) "";
  tmp_field->db_name=	empty_name;
  tmp_field->org_table_name= empty_name;
  tmp_field->org_col_name= empty_name;
  tmp_field->table_name= empty_name;
  tmp_field->col_name= name;
  tmp_field->charsetnr= collation.collation->number;
  tmp_field->flags= (maybe_null ? 0 : NOT_NULL_FLAG) |
                    (my_binary_compare(collation.collation) ?
                      BINARY_FLAG : 0);
  tmp_field->type= field_type_arg;
  tmp_field->length= max_length;
  tmp_field->decimals= decimals;
}

void Item::make_field(SendField *tmp_field)
{
  init_make_field(tmp_field, field_type());
}

enum_field_types Item::string_field_type() const
{
  enum_field_types f_type= DRIZZLE_TYPE_VARCHAR;
  if (max_length >= 65536)
    f_type= DRIZZLE_TYPE_BLOB;
  return f_type;
}

enum_field_types Item::field_type() const
{
  switch (result_type()) {
  case STRING_RESULT:  
    return string_field_type();
  case INT_RESULT:     
    return DRIZZLE_TYPE_LONGLONG;
  case DECIMAL_RESULT: 
    return DRIZZLE_TYPE_DECIMAL;
  case REAL_RESULT:    
    return DRIZZLE_TYPE_DOUBLE;
  case ROW_RESULT:
    assert(0);
  }

  abort();
}

bool Item::is_datetime()
{
  return field::isDateTime(field_type());
}

String *Item::check_well_formed_result(String *str, bool send_error)
{
  /* Check whether we got a well-formed string */
  const charset_info_st * const cs= str->charset();
  int well_formed_error;
  uint32_t wlen= cs->cset->well_formed_len(cs,
                                       str->ptr(), str->ptr() + str->length(),
                                       str->length(), &well_formed_error);
  if (wlen < str->length())
  {
    char hexbuf[7];
    enum DRIZZLE_ERROR::enum_warning_level level;
    uint32_t diff= str->length() - wlen;
    set_if_smaller(diff, 3U);
    (void) drizzled_string_to_hex(hexbuf, str->ptr() + wlen, diff);
    if (send_error)
    {
      my_error(ER_INVALID_CHARACTER_STRING, MYF(0),
               cs->csname,  hexbuf);
      return NULL;
    }
    {
      level= DRIZZLE_ERROR::WARN_LEVEL_ERROR;
      null_value= 1;
      str= 0;
    }
    push_warning_printf(&getSession(), level, ER_INVALID_CHARACTER_STRING,
                        ER(ER_INVALID_CHARACTER_STRING), cs->csname, hexbuf);
  }
  return str;
}

bool Item::eq_by_collation(Item *item, bool binary_cmp, const charset_info_st * const cs)
{
  const charset_info_st *save_cs= 0;
  const charset_info_st *save_item_cs= 0;
  if (collation.collation != cs)
  {
    save_cs= collation.collation;
    collation.collation= cs;
  }
  if (item->collation.collation != cs)
  {
    save_item_cs= item->collation.collation;
    item->collation.collation= cs;
  }
  bool res= eq(item, binary_cmp);
  if (save_cs)
    collation.collation= save_cs;
  if (save_item_cs)
    item->collation.collation= save_item_cs;
  return res;
}

Field *Item::make_string_field(Table *table)
{
  Field *field;
  assert(collation.collation);
  if (max_length/collation.collation->mbmaxlen > CONVERT_IF_BIGGER_TO_BLOB)
  {
    field= new Field_blob(max_length, maybe_null, name,
                          collation.collation);
  }
  else
  {
    table->setVariableWidth();
    field= new Field_varstring(max_length, maybe_null, name,
                               collation.collation);
  }

  if (field)
    field->init(table);
  return field;
}

Field *Item::tmp_table_field_from_field_type(Table *table, bool)
{
  /*
    The field functions defines a field to be not null if null_ptr is not 0
  */
  unsigned char *null_ptr= maybe_null ? (unsigned char*) "" : 0;
  Field *field= NULL;

  switch (field_type()) {
  case DRIZZLE_TYPE_DECIMAL:
    field= new Field_decimal((unsigned char*) 0,
                                 max_length,
                                 null_ptr,
                                 0,
                                 Field::NONE,
                                 name,
                                 decimals);
    break;
  case DRIZZLE_TYPE_LONG:
    field= new field::Int32((unsigned char*) 0, max_length, null_ptr, 0, Field::NONE, name);
    break;
  case DRIZZLE_TYPE_LONGLONG:
    field= new field::Int64((unsigned char*) 0, max_length, null_ptr, 0, Field::NONE, name);
    break;
  case DRIZZLE_TYPE_DOUBLE:
    field= new Field_double((unsigned char*) 0, max_length, null_ptr, 0, Field::NONE,
                            name, decimals, 0, unsigned_flag);
    break;
  case DRIZZLE_TYPE_NULL:
    field= new Field_null((unsigned char*) 0, max_length, name);
    break;
  case DRIZZLE_TYPE_DATE:
    field= new Field_date(maybe_null, name);
    break;

  case DRIZZLE_TYPE_MICROTIME:
    field= new field::Microtime(maybe_null, name);
    break;

  case DRIZZLE_TYPE_TIMESTAMP:
    field= new field::Epoch(maybe_null, name);
    break;
  case DRIZZLE_TYPE_DATETIME:
    field= new Field_datetime(maybe_null, name);
    break;
  case DRIZZLE_TYPE_TIME:
    field= new field::Time(maybe_null, name);
    break;
  case DRIZZLE_TYPE_BOOLEAN:
  case DRIZZLE_TYPE_UUID:
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_VARCHAR:
    return make_string_field(table);
  case DRIZZLE_TYPE_BLOB:
      field= new Field_blob(max_length, maybe_null, name, collation.collation);
    break;					// Blob handled outside of case
  }
  assert(field);

  if (field)
    field->init(table);
  return field;
}

/*
  This implementation can lose str_value content, so if the
  Item uses str_value to store something, it should
  reimplement it's ::save_in_field() as Item_string, for example, does
*/
int Item::save_in_field(Field *field, bool no_conversions)
{
  int error;
  if (result_type() == STRING_RESULT)
  {
    String *result;
    const charset_info_st * const cs= collation.collation;
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff, sizeof(buff), cs);
    result=val_str(&str_value);
    if (null_value)
    {
      str_value.set_quick(0, 0, cs);
      return set_field_to_null_with_conversions(field, no_conversions);
    }

    /* NOTE: If null_value == false, "result" must be not NULL.  */

    field->set_notnull();
    error=field->store(result->ptr(),result->length(),cs);
    str_value.set_quick(0, 0, cs);
  }
  else if (result_type() == REAL_RESULT &&
           field->result_type() == STRING_RESULT)
  {
    double nr= val_real();
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error= field->store(nr);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr= val_real();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store(nr);
  }
  else if (result_type() == DECIMAL_RESULT)
  {
    type::Decimal decimal_value;
    type::Decimal *value= val_decimal(&decimal_value);
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store_decimal(value);
  }
  else
  {
    int64_t nr=val_int();
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(nr, unsigned_flag);
  }
  return error;
}

/**
  Check if an item is a constant one and can be cached.

  @param arg [out] TRUE <=> Cache this item.

  @return TRUE  Go deeper in item tree.
  @return FALSE Don't go deeper in item tree.
*/

bool Item::cache_const_expr_analyzer(unsigned char **arg)
{
  bool *cache_flag= (bool*)*arg;
  if (!*cache_flag)
  {
    Item *item= real_item();
    /*
      Cache constant items unless it's a basic constant, constant field or
      a subselect (they use their own cache).
    */
    if (const_item() &&
        !(item->basic_const_item() || item->type() == Item::FIELD_ITEM ||
          item->type() == SUBSELECT_ITEM ||
           /*
             Do not cache GET_USER_VAR() function as its const_item() may
             return TRUE for the current thread but it still may change
             during the execution.
           */
          (item->type() == Item::FUNC_ITEM &&
           ((Item_func*)item)->functype() == Item_func::GUSERVAR_FUNC)))
      *cache_flag= true;
    return true;
  }
  return false;
}

/**
  Cache item if needed.

  @param arg   TRUE <=> Cache this item.

  @return cache if cache needed.
  @return this otherwise.
*/

Item* Item::cache_const_expr_transformer(unsigned char *arg)
{
  if (*(bool*)arg)
  {
    *((bool*)arg)= false;
    Item_cache *cache= Item_cache::get_cache(this);
    if (!cache)
      return NULL;
    cache->setup(this);
    cache->store(this);
    return cache;
  }
  return this;
}

void Item::send(plugin::Client *client, String *buffer)
{
  switch (field_type())
  {
  case DRIZZLE_TYPE_DATE:
  case DRIZZLE_TYPE_NULL:
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_BLOB:
  case DRIZZLE_TYPE_VARCHAR:
  case DRIZZLE_TYPE_BOOLEAN:
  case DRIZZLE_TYPE_UUID:
  case DRIZZLE_TYPE_DECIMAL:
    {
      if (String* res=val_str(buffer))
        client->store(res->ptr(), res->length());
      break;
    }
  case DRIZZLE_TYPE_LONG:
    {
      int64_t nr= val_int();
      if (!null_value)
        client->store((int32_t)nr);
      break;
    }
  case DRIZZLE_TYPE_LONGLONG:
    {
      int64_t nr= val_int();
      if (!null_value)
      {
        if (unsigned_flag)
          client->store((uint64_t)nr);
        else
          client->store((int64_t)nr);
      }
      break;
    }
  case DRIZZLE_TYPE_DOUBLE:
    {
      double nr= val_real();
      if (!null_value)
        client->store(nr, decimals, buffer);
      break;
    }
  case DRIZZLE_TYPE_TIME:
    {
      type::Time tm;
      get_time(tm);
      if (not null_value)
        client->store(&tm);
      break;
    }
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_MICROTIME:
  case DRIZZLE_TYPE_TIMESTAMP:
    {
      type::Time tm;
      get_date(tm, TIME_FUZZY_DATE);
      if (!null_value)
        client->store(&tm);
      break;
    }
  }
  if (null_value)
    client->store();
}

uint32_t Item::max_char_length() const
{
  return max_length / collation.collation->mbmaxlen;
}

void Item::fix_length_and_charset(uint32_t max_char_length_arg, charset_info_st *cs)
{ 
  max_length= char_to_byte_length_safe(max_char_length_arg, cs->mbmaxlen);
  collation.collation= cs;
}

void Item::fix_char_length(uint32_t max_char_length_arg)
{ 
  max_length= char_to_byte_length_safe(max_char_length_arg, collation.collation->mbmaxlen);
}

void Item::fix_char_length_uint64_t(uint64_t max_char_length_arg)
{ 
  uint64_t max_result_length= max_char_length_arg *
    collation.collation->mbmaxlen;

  if (max_result_length >= MAX_BLOB_WIDTH)
  { 
    max_length= MAX_BLOB_WIDTH;
    maybe_null= false;
  }
  else
  {
    max_length= max_result_length;
  }
}

void Item::fix_length_and_charset_datetime(uint32_t max_char_length_arg)
{ 
  collation.set(&my_charset_bin);
  fix_char_length(max_char_length_arg);
}

Item_result item_cmp_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT && b == STRING_RESULT)
    return STRING_RESULT;

  if (a == INT_RESULT && b == INT_RESULT)
    return INT_RESULT;
  else if (a == ROW_RESULT || b == ROW_RESULT)
    return ROW_RESULT;

  if ((a == INT_RESULT || a == DECIMAL_RESULT) &&
      (b == INT_RESULT || b == DECIMAL_RESULT))
    return DECIMAL_RESULT;

  return REAL_RESULT;
}

void resolve_const_item(Session *session, Item **ref, Item *comp_item)
{
  Item *item= *ref;
  Item *new_item= NULL;
  if (item->basic_const_item())
    return; /* Can't be better */
  Item_result res_type=item_cmp_type(comp_item->result_type(),
				     item->result_type());
  char *name=item->name; /* Alloced by memory::sql_alloc */

  switch (res_type) {
  case STRING_RESULT:
    {
      char buff[MAX_FIELD_WIDTH];
      String tmp(buff,sizeof(buff),&my_charset_bin),*result;
      result=item->val_str(&tmp);
      if (item->null_value)
        new_item= new Item_null(name);
      else
      {
        uint32_t length= result->length();
        char *tmp_str= memory::sql_strmake(result->ptr(), length);
        new_item= new Item_string(name, tmp_str, length, result->charset());
      }
      break;
    }
  case INT_RESULT:
    {
      int64_t result=item->val_int();
      uint32_t length=item->max_length;
      bool null_value=item->null_value;
      new_item= (null_value ? (Item*) new Item_null(name) :
                 (Item*) new Item_int(name, result, length));
      break;
    }
  case ROW_RESULT:
    if (item->type() == Item::ROW_ITEM && comp_item->type() == Item::ROW_ITEM)
    {
      /*
        Substitute constants only in Item_rows. Don't affect other Items
        with ROW_RESULT (eg Item_singlerow_subselect).

        For such Items more optimal is to detect if it is constant and replace
        it with Item_row. This would optimize queries like this:
        SELECT * FROM t1 WHERE (a,b) = (SELECT a,b FROM t2 LIMIT 1);
      */
      Item_row *item_row= (Item_row*) item;
      Item_row *comp_item_row= (Item_row*) comp_item;
      uint32_t col;
      new_item= 0;
      /*
        If item and comp_item are both Item_rows and have same number of cols
        then process items in Item_row one by one.
        We can't ignore NULL values here as this item may be used with <=>, in
        which case NULL's are significant.
      */
      assert(item->result_type() == comp_item->result_type());
      assert(item_row->cols() == comp_item_row->cols());
      col= item_row->cols();
      while (col-- > 0)
        resolve_const_item(session, item_row->addr(col),
                           comp_item_row->element_index(col));
      break;
    }
    /* Fallthrough */
  case REAL_RESULT:
    {						// It must REAL_RESULT
      double result= item->val_real();
      uint32_t length=item->max_length,decimals=item->decimals;
      bool null_value=item->null_value;
      new_item= (null_value ? (Item*) new Item_null(name) : (Item*)
                 new Item_float(name, result, decimals, length));
      break;
    }
  case DECIMAL_RESULT:
    {
      type::Decimal decimal_value;
      type::Decimal *result= item->val_decimal(&decimal_value);
      uint32_t length= item->max_length, decimals= item->decimals;
      bool null_value= item->null_value;
      new_item= (null_value ?
                 (Item*) new Item_null(name) :
                 (Item*) new Item_decimal(name, result, length, decimals));
      break;
    }
  }

  if (new_item)
    *ref= new_item;
}

bool field_is_equal_to_item(Field *field,Item *item)
{

  Item_result res_type=item_cmp_type(field->result_type(),
				     item->result_type());
  if (res_type == STRING_RESULT)
  {
    char item_buff[MAX_FIELD_WIDTH];
    char field_buff[MAX_FIELD_WIDTH];
    String item_tmp(item_buff,sizeof(item_buff),&my_charset_bin),*item_result;
    String field_tmp(field_buff,sizeof(field_buff),&my_charset_bin);
    item_result=item->val_str(&item_tmp);
    if (item->null_value)
      return 1;					// This must be true
    field->val_str_internal(&field_tmp);
    return not stringcmp(&field_tmp,item_result);
  }

  if (res_type == INT_RESULT)
    return 1;					// Both where of type int

  if (res_type == DECIMAL_RESULT)
  {
    type::Decimal item_buf, *item_val,
               field_buf, *field_val;
    item_val= item->val_decimal(&item_buf);
    if (item->null_value)
      return 1;					// This must be true
    field_val= field->val_decimal(&field_buf);
    return !class_decimal_cmp(item_val, field_val);
  }

  double result= item->val_real();
  if (item->null_value)
    return 1;

  return result == field->val_real();
}

void dummy_error_processor(Session *, void *)
{}

/**
  Create field for temporary table using type of given item.

  @param session                   Thread handler
  @param item                  Item to create a field for
  @param table                 Temporary table
  @param copy_func             If set and item is a function, store copy of
                               item in this array
  @param modify_item           1 if item->result_field should point to new
                               item. This is relevent for how fill_record()
                               is going to work:
                               If modify_item is 1 then fill_record() will
                               update the record in the original table.
                               If modify_item is 0 then fill_record() will
                               update the temporary table
  @param convert_blob_length   If >0 create a varstring(convert_blob_length)
                               field instead of blob.

  @retval
    0  on error
  @retval
    new_created field
*/
static Field *create_tmp_field_from_item(Session *,
                                         Item *item, Table *table,
                                         Item ***copy_func, bool modify_item,
                                         uint32_t convert_blob_length)
{
  bool maybe_null= item->maybe_null;
  Field *new_field= NULL;

  switch (item->result_type()) {
  case REAL_RESULT:
    new_field= new Field_double(item->max_length, maybe_null,
                                item->name, item->decimals, true);
    break;

  case INT_RESULT:
    /*
      Select an integer type with the minimal fit precision.
      MY_INT32_NUM_DECIMAL_DIGITS is sign inclusive, don't consider the sign.
      Values with MY_INT32_NUM_DECIMAL_DIGITS digits may or may not fit into
      Int32 -> make them field::Int64.
    */
    if (item->unsigned_flag)
    {
      new_field= new field::Size(item->max_length, maybe_null,
                                  item->name, item->unsigned_flag);
    }
    else if (item->max_length >= (MY_INT32_NUM_DECIMAL_DIGITS - 1))
    {
      new_field= new field::Int64(item->max_length, maybe_null,
                                  item->name, item->unsigned_flag);
    }
    else
    {
      new_field= new field::Int32(item->max_length, maybe_null,
                                  item->name, item->unsigned_flag);
    }

    break;

  case STRING_RESULT:
    assert(item->collation.collation);

    /*
      DATE/TIME fields have STRING_RESULT result type.
      To preserve type they needed to be handled separately.
    */
    if (field::isDateTime(item->field_type()))
    {
      new_field= item->tmp_table_field_from_field_type(table, 1);
      /*
        Make sure that the blob fits into a Field_varstring which has
        2-byte lenght.
      */
    }
    else if (item->max_length/item->collation.collation->mbmaxlen > 255 &&
             convert_blob_length <= Field_varstring::MAX_SIZE &&
             convert_blob_length)
    {
      table->setVariableWidth();
      new_field= new Field_varstring(convert_blob_length, maybe_null,
                                     item->name, item->collation.collation);
    }
    else
    {
      new_field= item->make_string_field(table);
    }
    new_field->set_derivation(item->collation.derivation);
    break;

  case DECIMAL_RESULT:
    {
      uint8_t dec= item->decimals;
      uint8_t intg= ((Item_decimal *) item)->decimal_precision() - dec;
      uint32_t len= item->max_length;

      /*
        Trying to put too many digits overall in a DECIMAL(prec,dec)
        will always throw a warning. We must limit dec to
        DECIMAL_MAX_SCALE however to prevent an assert() later.
      */

      if (dec > 0)
      {
        signed int overflow;

        dec= min(dec, (uint8_t)DECIMAL_MAX_SCALE);

        /*
          If the value still overflows the field with the corrected dec,
          we'll throw out decimals rather than integers. This is still
          bad and of course throws a truncation warning.
          +1: for decimal point
        */

        overflow= class_decimal_precision_to_length(intg + dec, dec,
                                                 item->unsigned_flag) - len;

        if (overflow > 0)
          dec= max(0, dec - overflow);            // too long, discard fract
        else
          len-= item->decimals - dec;             // corrected value fits
      }

      new_field= new Field_decimal(len,
                                   maybe_null,
                                   item->name,
                                   dec,
                                   item->unsigned_flag);
      break;
    }

  case ROW_RESULT:
    // This case should never be choosen
    assert(0);
    abort();
  }

  if (new_field)
    new_field->init(table);

  if (copy_func && item->is_result_field())
    *((*copy_func)++) = item;			// Save for copy_funcs

  if (modify_item)
    item->set_result_field(new_field);

  if (item->type() == Item::NULL_ITEM)
    new_field->is_created_from_null_item= true;

  return new_field;
}

Field *create_tmp_field(Session *session,
                        Table *table,
                        Item *item,
                        Item::Type type,
                        Item ***copy_func,
                        Field **from_field,
                        Field **default_field,
                        bool group,
                        bool modify_item,
                        bool make_copy_field,
                        uint32_t convert_blob_length)
{
  Field *result;
  Item::Type orig_type= type;
  Item *orig_item= 0;

  if (type != Item::FIELD_ITEM &&
      item->real_item()->type() == Item::FIELD_ITEM)
  {
    orig_item= item;
    item= item->real_item();
    type= Item::FIELD_ITEM;
  }

  switch (type) {
  case Item::SUM_FUNC_ITEM:
  {
    Item_sum *item_sum=(Item_sum*) item;
    result= item_sum->create_tmp_field(group, table, convert_blob_length);
    if (!result)
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
    return result;
  }
  case Item::FIELD_ITEM:
  case Item::DEFAULT_VALUE_ITEM:
  {
    Item_field *field= (Item_field*) item;
    bool orig_modify= modify_item;
    if (orig_type == Item::REF_ITEM)
      modify_item= 0;
    /*
      If item have to be able to store NULLs but underlaid field can't do it,
      create_tmp_field_from_field() can't be used for tmp field creation.
    */
    if (field->maybe_null && !field->field->maybe_null())
    {
      result= create_tmp_field_from_item(session, item, table, NULL,
                                         modify_item, convert_blob_length);
      *from_field= field->field;
      if (result && modify_item)
        field->result_field= result;
    }
    else
    {
      result= create_tmp_field_from_field(session, (*from_field= field->field),
                                          orig_item ? orig_item->name :
                                          item->name,
                                          table,
                                          modify_item ? field :
                                          NULL,
                                          convert_blob_length);
    }
    if (orig_type == Item::REF_ITEM && orig_modify)
      ((Item_ref*)orig_item)->set_result_field(result);
    if (field->field->eq_def(result))
      *default_field= field->field;
    return result;
  }
  /* Fall through */
  case Item::FUNC_ITEM:
    /* Fall through */
  case Item::COND_ITEM:
  case Item::FIELD_AVG_ITEM:
  case Item::FIELD_STD_ITEM:
  case Item::SUBSELECT_ITEM:
    /* The following can only happen with 'CREATE TABLE ... SELECT' */
  case Item::PROC_ITEM:
  case Item::INT_ITEM:
  case Item::REAL_ITEM:
  case Item::DECIMAL_ITEM:
  case Item::STRING_ITEM:
  case Item::REF_ITEM:
  case Item::NULL_ITEM:
  case Item::VARBIN_ITEM:
    if (make_copy_field)
    {
      assert(((Item_result_field*)item)->result_field);
      *from_field= ((Item_result_field*)item)->result_field;
    }
    return create_tmp_field_from_item(session, item, table,
                                      (make_copy_field ? 0 : copy_func),
                                       modify_item, convert_blob_length);
  case Item::TYPE_HOLDER:
    result= ((Item_type_holder *)item)->make_field_by_type(table);
    result->set_derivation(item->collation.derivation);
    return result;
  default:					// Dosen't have to be stored
    return NULL;
  }
}

std::ostream& operator<<(std::ostream& output, const Item &item)
{
  switch (item.type())
  {
  case drizzled::Item::SUBSELECT_ITEM :
  case drizzled::Item::FIELD_ITEM :
  case drizzled::Item::SUM_FUNC_ITEM :
  case drizzled::Item::STRING_ITEM :
  case drizzled::Item::INT_ITEM :
  case drizzled::Item::REAL_ITEM :
  case drizzled::Item::NULL_ITEM :
  case drizzled::Item::VARBIN_ITEM :
  case drizzled::Item::COPY_STR_ITEM :
  case drizzled::Item::FIELD_AVG_ITEM :
  case drizzled::Item::DEFAULT_VALUE_ITEM :
  case drizzled::Item::PROC_ITEM :
  case drizzled::Item::COND_ITEM :
  case drizzled::Item::REF_ITEM :
  case drizzled::Item::FIELD_STD_ITEM :
  case drizzled::Item::FIELD_VARIANCE_ITEM :
  case drizzled::Item::INSERT_VALUE_ITEM :
  case drizzled::Item::ROW_ITEM:
  case drizzled::Item::CACHE_ITEM :
  case drizzled::Item::TYPE_HOLDER :
  case drizzled::Item::PARAM_ITEM :
  case drizzled::Item::DECIMAL_ITEM :
  case drizzled::Item::FUNC_ITEM :
  case drizzled::Item::BOOLEAN_ITEM :
    {
      output << "Item:(";
      output <<  item.full_name();
      output << ", ";
      output << drizzled::display::type(item.type());
      output << ")";
    }
    break;
  }

  return output;  // for multiple << operators.
}

} /* namespace drizzled */
