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

#include <float.h>

#include <algorithm>

#include <drizzled/error.h>
#include <drizzled/function/func.h>
#include <drizzled/item/sum.h>
#include <drizzled/item/type_holder.h>
#include <drizzled/field/enum.h>

using namespace std;

namespace drizzled {

Item_type_holder::Item_type_holder(Session *session, Item *item)
  :Item(session, item), enum_set_typelib(0), fld_type(get_real_type(item))
{
  assert(item->fixed);
  maybe_null= item->maybe_null;
  collation.set(item->collation);
  get_full_info(item);
  /* fix variable decimals which always is NOT_FIXED_DEC */
  if (Field::result_merge_type(fld_type) == INT_RESULT)
    decimals= 0;
  prev_decimal_int_part= item->decimal_int_part();
}


Item_result Item_type_holder::result_type() const
{
  return Field::result_merge_type(fld_type);
}


enum_field_types Item_type_holder::get_real_type(Item *item)
{
  switch(item->type())
  {
  case FIELD_ITEM:
  {
    /*
      Item_fields::field_type ask Field_type() but sometimes field return
      a different type, like for enum/set, so we need to ask real type.
    */
    Field *field= ((Item_field *) item)->field;
    enum_field_types type= field->real_type();
    if (field->is_created_from_null_item)
      return DRIZZLE_TYPE_NULL;
    return type;
  }
  case SUM_FUNC_ITEM:
  {
    /*
      Argument of aggregate function sometimes should be asked about field
      type
    */
    Item_sum *item_sum= (Item_sum *) item;
    if (item_sum->keep_field_type())
      return get_real_type(item_sum->args[0]);
    break;
  }
  case FUNC_ITEM:
    if (((Item_func *) item)->functype() == Item_func::GUSERVAR_FUNC)
    {
      /*
        There are work around of problem with changing variable type on the
        fly and variable always report "string" as field type to get
        acceptable information for client in send_field, so we make field
        type from expression type.
      */
      switch (item->result_type()) {
      case STRING_RESULT:
        return DRIZZLE_TYPE_VARCHAR;
      case INT_RESULT:
        return DRIZZLE_TYPE_LONGLONG;
      case REAL_RESULT:
        return DRIZZLE_TYPE_DOUBLE;
      case DECIMAL_RESULT:
        return DRIZZLE_TYPE_DECIMAL;
      case ROW_RESULT:
        assert(0);
        return DRIZZLE_TYPE_VARCHAR;
      }
    }
    break;
  default:
    break;
  }
  return item->field_type();
}


bool Item_type_holder::join_types(Session *, Item *item)
{
  uint32_t max_length_orig= max_length;
  uint32_t decimals_orig= decimals;
  fld_type= Field::field_type_merge(fld_type, get_real_type(item));
  {
    int item_decimals= item->decimals;
    /* fix variable decimals which always is NOT_FIXED_DEC */
    if (Field::result_merge_type(fld_type) == INT_RESULT)
      item_decimals= 0;
    decimals= max((int)decimals, item_decimals);
  }
  if (Field::result_merge_type(fld_type) == DECIMAL_RESULT)
  {
    decimals= min((int)max(decimals, item->decimals), DECIMAL_MAX_SCALE);
    int precision= min(max(prev_decimal_int_part, item->decimal_int_part())
                       + decimals, DECIMAL_MAX_PRECISION);
    unsigned_flag&= item->unsigned_flag;
    max_length= class_decimal_precision_to_length(precision, decimals,
                                               unsigned_flag);
  }

  switch (Field::result_merge_type(fld_type))
  {
  case STRING_RESULT:
    {
      const char *old_cs, *old_derivation;
      uint32_t old_max_chars= max_length / collation.collation->mbmaxlen;
      old_cs= collation.collation->name;
      old_derivation= collation.derivation_name();
      if (collation.aggregate(item->collation, MY_COLL_ALLOW_CONV))
      {
        my_error(ER_CANT_AGGREGATE_2COLLATIONS, MYF(0),
                 old_cs, old_derivation,
                 item->collation.collation->name,
                 item->collation.derivation_name(),
                 "UNION");
        return true;
      }
      /*
        To figure out max_length, we have to take into account possible
        expansion of the size of the values because of character set
        conversions.
      */
      if (collation.collation != &my_charset_bin)
      {
        max_length= max(old_max_chars * collation.collation->mbmaxlen,
                        display_length(item) /
                        item->collation.collation->mbmaxlen *
                        collation.collation->mbmaxlen);
      }
      else
        set_if_bigger(max_length, display_length(item));
      break;
    }
  case REAL_RESULT:
    {
      if (decimals != NOT_FIXED_DEC)
      {
        int delta1= max_length_orig - decimals_orig;
        int delta2= item->max_length - item->decimals;
        max_length= max(delta1, delta2) + decimals;
        if (fld_type == DRIZZLE_TYPE_DOUBLE && max_length > DBL_DIG + 2)
        {
          max_length= DBL_DIG + 7;
          decimals= NOT_FIXED_DEC;
        }
      }
      else
        max_length= DBL_DIG+7;
      break;
    }

  case INT_RESULT:
  case DECIMAL_RESULT:
  case ROW_RESULT:
    max_length= max(max_length, display_length(item));
  };
  maybe_null|= item->maybe_null;
  get_full_info(item);

  /* Remember decimal integer part to be used in DECIMAL_RESULT handleng */
  prev_decimal_int_part= decimal_int_part();

  return false;
}


uint32_t Item_type_holder::display_length(Item *item)
{
  if (item->type() == Item::FIELD_ITEM)
    return ((Item_field *)item)->max_disp_length();

  switch (item->field_type())
  {
  case DRIZZLE_TYPE_TIME:
  case DRIZZLE_TYPE_BOOLEAN:
  case DRIZZLE_TYPE_UUID:
  case DRIZZLE_TYPE_MICROTIME:
  case DRIZZLE_TYPE_TIMESTAMP:
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_DATE:
  case DRIZZLE_TYPE_VARCHAR:
  case DRIZZLE_TYPE_DECIMAL:
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_BLOB:
    return item->max_length;
  case DRIZZLE_TYPE_LONG:
    return MY_INT32_NUM_DECIMAL_DIGITS;
  case DRIZZLE_TYPE_DOUBLE:
    return 53;
  case DRIZZLE_TYPE_NULL:
    return 0;
  case DRIZZLE_TYPE_LONGLONG:
    return 20;
  }
  assert(0);
  abort();
}


Field *Item_type_holder::make_field_by_type(Table *table)
{
  /*
    The field functions defines a field to be not null if null_ptr is not 0
  */
  unsigned char *null_ptr= maybe_null ? (unsigned char*) "" : 0;
  Field *field;

  switch (fld_type) {
  case DRIZZLE_TYPE_ENUM:
    assert(enum_set_typelib);
    field= new Field_enum((unsigned char *) 0,
                          max_length,
                          null_ptr,
                          0,
                          name,
                          enum_set_typelib,
                          collation.collation);
    if (field)
      field->init(table);
    return field;
  case DRIZZLE_TYPE_NULL:
    return make_string_field(table);
  default:
    break;
  }
  return tmp_table_field_from_field_type(table, 0);
}


void Item_type_holder::get_full_info(Item *item)
{
  if (fld_type == DRIZZLE_TYPE_ENUM)
  {
    if (item->type() == Item::SUM_FUNC_ITEM &&
        (((Item_sum*)item)->sum_func() == Item_sum::MAX_FUNC ||
         ((Item_sum*)item)->sum_func() == Item_sum::MIN_FUNC))
      item = ((Item_sum*)item)->args[0];
    /*
      We can have enum/set type after merging only if we have one enum|set
      field (or MIN|MAX(enum|set field)) and number of NULL fields
    */
    assert((enum_set_typelib &&
                 get_real_type(item) == DRIZZLE_TYPE_NULL) ||
                (!enum_set_typelib &&
                 item->type() == Item::FIELD_ITEM &&
                 (get_real_type(item) == DRIZZLE_TYPE_ENUM) &&
                 ((Field_enum*)((Item_field *) item)->field)->typelib));
    if (!enum_set_typelib)
    {
      enum_set_typelib= ((Field_enum*)((Item_field *) item)->field)->typelib;
    }
  }
}


double Item_type_holder::val_real()
{
  assert(0); // should never be called
  return 0.0;
}


int64_t Item_type_holder::val_int()
{
  assert(0); // should never be called
  return 0;
}

type::Decimal *Item_type_holder::val_decimal(type::Decimal *)
{
  assert(0); // should never be called
  return 0;
}

String *Item_type_holder::val_str(String*)
{
  assert(0); // should never be called
  return 0;
}

void Item_result_field::cleanup()
{
  Item::cleanup();
  result_field= 0;
  return;
}

} /* namespace drizzled */
