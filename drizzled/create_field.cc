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

/**
 * @file Implementation of CreateField class
 */

#include <config.h>
#include <errno.h>
#include <float.h>
#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/field.h>
#include <drizzled/create_field.h>
#include <drizzled/field/str.h>
#include <drizzled/field/num.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/boolean.h>
#include <drizzled/field/enum.h>
#include <drizzled/field/null.h>
#include <drizzled/field/date.h>
#include <drizzled/field/decimal.h>
#include <drizzled/field/real.h>
#include <drizzled/field/double.h>
#include <drizzled/field/int32.h>
#include <drizzled/field/int64.h>
#include <drizzled/field/num.h>
#include <drizzled/field/epoch.h>
#include <drizzled/field/datetime.h>
#include <drizzled/field/varstring.h>
#include <drizzled/field/uuid.h>
#include <drizzled/temporal.h>
#include <drizzled/item/string.h>
#include <drizzled/table.h>

#include <drizzled/display.h>

#include <algorithm>

using namespace std;

namespace drizzled
{

/** Create a field suitable for create of table. */
CreateField::CreateField(Field *old_field, Field *orig_field)
{
  field= old_field;
  field_name= change= old_field->field_name;
  length= old_field->field_length;
  flags= old_field->flags;
  unireg_check= old_field->unireg_check;
  pack_length= old_field->pack_length();
  key_length= old_field->key_length();
  sql_type= old_field->real_type();
  charset= old_field->charset(); // May be NULL ptr
  comment= old_field->comment;
  decimals= old_field->decimals();

  /* Fix if the original table had 4 byte pointer blobs */
  if (flags & BLOB_FLAG)
  {
    pack_length= (pack_length - old_field->getTable()->getShare()->sizeBlobPtr() + portable_sizeof_char_ptr);
  }

  switch (sql_type) 
  {
    case DRIZZLE_TYPE_BLOB:
      sql_type= DRIZZLE_TYPE_BLOB;
      length/= charset->mbmaxlen;
      key_length/= charset->mbmaxlen;
      break;
    case DRIZZLE_TYPE_ENUM:
    case DRIZZLE_TYPE_VARCHAR:
      /* This is corrected in create_length_to_internal_length */
      length= (length+charset->mbmaxlen-1) / charset->mbmaxlen;
      break;
    default:
      break;
  }

  if (flags & ENUM_FLAG)
    interval= ((Field_enum*) old_field)->typelib;
  else
    interval= 0;
  def= 0;
  char_length= length;

  if (!(flags & (NO_DEFAULT_VALUE_FLAG)) &&
      !(flags & AUTO_INCREMENT_FLAG) &&
      old_field->ptr && orig_field &&
      (not old_field->is_timestamp() ||                /* set def only if */
       old_field->getTable()->timestamp_field != old_field ||  /* timestamp field */
       unireg_check == Field::TIMESTAMP_UN_FIELD))        /* has default val */
  {
    /* Get the value from default_values */
    ptrdiff_t diff= (ptrdiff_t) (orig_field->getTable()->getDefaultValues() - orig_field->getTable()->getInsertRecord());
    orig_field->move_field_offset(diff);	// Points now at default_values
    if (! orig_field->is_real_null())
    {
      char buff[MAX_FIELD_WIDTH];
      String tmp(buff, sizeof(buff), charset);
      String* res= orig_field->val_str_internal(&tmp);
      char* pos= (char*) memory::sql_strmake(res->ptr(), res->length());
      def= new Item_string(pos, res->length(), charset);
    }
    orig_field->move_field_offset(-diff);	// Back to getInsertRecord()
  }
}

/**
  Convert CreateField::length from number of characters to number of bytes.
*/
void CreateField::create_length_to_internal_length(void)
{
  switch (sql_type) 
  {
    case DRIZZLE_TYPE_BLOB:
    case DRIZZLE_TYPE_VARCHAR:
      length*= charset->mbmaxlen;
      key_length= length;
      pack_length= calc_pack_length(sql_type, length);
      break;
    case DRIZZLE_TYPE_ENUM:
      /* Pack_length already calculated in ::init() */
      length*= charset->mbmaxlen;
      key_length= pack_length;
      break;
    case DRIZZLE_TYPE_DECIMAL:
      key_length= pack_length=
        class_decimal_get_binary_size(class_decimal_length_to_precision(length,
                  decimals,
                  flags &
                  UNSIGNED_FLAG),
          decimals);
      break;
    default:
      key_length= pack_length= calc_pack_length(sql_type, length);
      break;
  }
}

/**
  Init for a tmp table field. To be extended if need be.
*/
void CreateField::init_for_tmp_table(enum_field_types sql_type_arg,
                                     uint32_t length_arg,
                                     uint32_t decimals_arg,
                                     bool maybe_null)
{
  field_name= "";
  sql_type= sql_type_arg;
  char_length= length= length_arg;;
  unireg_check= Field::NONE;
  interval= 0;
  charset= &my_charset_bin;
  decimals= decimals_arg & FIELDFLAG_MAX_DEC;

  if (! maybe_null)
    flags= NOT_NULL_FLAG;
  else
    flags= 0;
}

bool CreateField::init(Session *,
                        char *fld_name,
                        enum_field_types fld_type,
                        char *fld_length,
                        char *fld_decimals,
                        uint32_t fld_type_modifier,
                        LEX_STRING *fld_comment,
                        char *fld_change,
                        List<String> *fld_interval_list,
                        const charset_info_st * const fld_charset,
                        uint32_t,
                        enum column_format_type column_format_in)
{
  uint32_t sign_len= 0;
  uint32_t allowed_type_modifier= 0;
  uint32_t max_field_charlength= MAX_FIELD_CHARLENGTH;

  field= 0;
  field_name= fld_name;
  flags= fld_type_modifier;
  flags|= (((uint32_t)column_format_in & COLUMN_FORMAT_MASK) << COLUMN_FORMAT_FLAGS);
  unireg_check= (fld_type_modifier & AUTO_INCREMENT_FLAG ?
                 Field::NEXT_NUMBER : Field::NONE);
  decimals= fld_decimals ? (uint32_t)atoi(fld_decimals) : 0;
  if (decimals >= NOT_FIXED_DEC)
  {
    my_error(ER_TOO_BIG_SCALE, MYF(0), decimals, fld_name,
             NOT_FIXED_DEC-1);
    return true;
  }

  sql_type= fld_type;
  length= 0;
  change= fld_change;
  interval= 0;
  pack_length= key_length= 0;
  charset= fld_charset;
  interval_list.clear();

  comment= *fld_comment;

  if (fld_length && !(length= (uint32_t) atoi(fld_length)))
    fld_length= 0;
  sign_len= fld_type_modifier & UNSIGNED_FLAG ? 0 : 1;

  switch (fld_type) 
  {
    case DRIZZLE_TYPE_LONG:
      if (!fld_length)
        length= MAX_INT_WIDTH+sign_len;
      allowed_type_modifier= AUTO_INCREMENT_FLAG;
      break;
    case DRIZZLE_TYPE_LONGLONG:
      if (!fld_length)
        length= MAX_BIGINT_WIDTH;
      allowed_type_modifier= AUTO_INCREMENT_FLAG;
      break;
    case DRIZZLE_TYPE_NULL:
      break;
    case DRIZZLE_TYPE_DECIMAL:
      class_decimal_trim(&length, &decimals);
      if (length > DECIMAL_MAX_PRECISION)
      {
        my_error(ER_TOO_BIG_PRECISION, MYF(0), length, fld_name,
                DECIMAL_MAX_PRECISION);
        return true;
      }
      if (length < decimals)
      {
        my_error(ER_M_BIGGER_THAN_D, MYF(0), fld_name);
        return true;
      }
      length= class_decimal_precision_to_length(length, decimals, fld_type_modifier & UNSIGNED_FLAG);
      pack_length= class_decimal_get_binary_size(length, decimals);
      break;
    case DRIZZLE_TYPE_VARCHAR:
      /*
        Long VARCHAR's are automaticly converted to blobs in mysql_prepare_table
        if they don't have a default value
      */
      max_field_charlength= MAX_FIELD_VARCHARLENGTH;
      break;
    case DRIZZLE_TYPE_BLOB:
      flags|= BLOB_FLAG;
      break;
    case DRIZZLE_TYPE_DOUBLE:
      allowed_type_modifier= AUTO_INCREMENT_FLAG;
      if (!fld_length && !fld_decimals)
      {
        length= DBL_DIG+7;
        decimals= NOT_FIXED_DEC;
      }
      if (length < decimals &&
          decimals != NOT_FIXED_DEC)
      {
        my_error(ER_M_BIGGER_THAN_D, MYF(0), fld_name);
        return true;
      }
      break;
    case DRIZZLE_TYPE_MICROTIME:
      /* 
        This assert() should be correct due to absence of length
        specifiers for timestamp. Previous manipulation also wasn't
        ever called (from examining lcov)
      */
      assert(fld_type);
    case DRIZZLE_TYPE_TIMESTAMP:
      length= MicroTimestamp::MAX_STRING_LENGTH;
      break;
    case DRIZZLE_TYPE_DATE:
      length= Date::MAX_STRING_LENGTH;
      break;
    case DRIZZLE_TYPE_UUID:
      length= field::Uuid::max_string_length();
      break;
    case DRIZZLE_TYPE_BOOLEAN:
      length= field::Boolean::max_string_length();
      break;
    case DRIZZLE_TYPE_DATETIME:
      length= DateTime::MAX_STRING_LENGTH;
      break;
    case DRIZZLE_TYPE_TIME:
      length= DateTime::MAX_STRING_LENGTH;
      break;
    case DRIZZLE_TYPE_ENUM:
      {
        /* Should be safe. */
        pack_length= 4;

        List<String>::iterator it(fld_interval_list->begin());
        String *tmp;
        while ((tmp= it++))
          interval_list.push_back(tmp);
        length= 1;
        break;
    }
  }
  /* Remember the value of length */
  char_length= length;

  if (!(flags & BLOB_FLAG) &&
      ((length > max_field_charlength &&
        fld_type != DRIZZLE_TYPE_ENUM  &&
        (fld_type != DRIZZLE_TYPE_VARCHAR)) ||
       (!length && fld_type != DRIZZLE_TYPE_VARCHAR)))
  {
    my_error((fld_type == DRIZZLE_TYPE_VARCHAR) ?  ER_TOO_BIG_FIELDLENGTH : ER_TOO_BIG_DISPLAYWIDTH,
              MYF(0),
             fld_name, max_field_charlength / (charset? charset->mbmaxlen : 1));
    return true;
  }
  fld_type_modifier&= AUTO_INCREMENT_FLAG;
  if ((~allowed_type_modifier) & fld_type_modifier)
  {
    my_error(ER_WRONG_FIELD_SPEC, MYF(0), fld_name);
    return true;
  }

  return false; /* success */
}

bool CreateField::setDefaultValue(Item *default_value_item,
                                  Item *on_update_item)
{
  def= default_value_item;

  /*
    Set NO_DEFAULT_VALUE_FLAG if this field doesn't have a default value and
    it is NOT NULL, not an AUTO_INCREMENT field and not a TIMESTAMP.
  */
  if (! default_value_item
      && ! (flags & AUTO_INCREMENT_FLAG)
      && (flags & NOT_NULL_FLAG)
      && (sql_type != DRIZZLE_TYPE_TIMESTAMP
          and sql_type != DRIZZLE_TYPE_MICROTIME))
  {
    flags|= NO_DEFAULT_VALUE_FLAG;
  }
  else
  {
    flags&= ~NO_DEFAULT_VALUE_FLAG;
  }

  if (sql_type == DRIZZLE_TYPE_BLOB && default_value_item)
  {
    /* Allow empty as default value. */
    String str,*res;
    res= default_value_item->val_str(&str);
    if (res->length())
    {
      my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0), field_name);
      return true;
    }
  }

  if (sql_type == DRIZZLE_TYPE_TIMESTAMP
      || sql_type == DRIZZLE_TYPE_MICROTIME)
  {
    bool on_update_now= on_update_item
      || (unireg_check == Field::TIMESTAMP_DNUN_FIELD
          || unireg_check == Field::TIMESTAMP_UN_FIELD);

    if (default_value_item)
    {
      /* Grammar allows only NOW() value for ON UPDATE clause */
      if (default_value_item->type() == Item::FUNC_ITEM &&
          ((Item_func*)default_value_item)->functype() == Item_func::NOW_FUNC)
      {
        unireg_check= (on_update_now ? Field::TIMESTAMP_DNUN_FIELD:
                       Field::TIMESTAMP_DN_FIELD);
        /*
          We don't need default value any longer moreover it is dangerous.
          Everything handled by unireg_check further.
        */
        def= 0;
      }
      else
      {
        unireg_check= (on_update_now ? Field::TIMESTAMP_UN_FIELD:
                       Field::NONE);
      }
    }
    else
    {
      /*
        If we have default TIMESTAMP NOT NULL column without explicit DEFAULT
        or ON UPDATE values then for the sake of compatiblity we should treat
        this column as having DEFAULT NOW() ON UPDATE NOW() (when we don't
        have another TIMESTAMP column with auto-set option before this one)
        or DEFAULT 0 (in other cases).
        So here we are setting TIMESTAMP_OLD_FIELD only temporary, and will
        replace this value by TIMESTAMP_DNUN_FIELD or NONE later when
        information about all TIMESTAMP fields in table will be availiable.

        If we have TIMESTAMP NULL column without explicit DEFAULT value
        we treat it as having DEFAULT NULL attribute.
      */
      unireg_check= (on_update_now ? Field::TIMESTAMP_UN_FIELD :
                     (flags & NOT_NULL_FLAG ? Field::TIMESTAMP_OLD_FIELD :
                      Field::NONE));
    }
  }

  return false;
}

std::ostream& operator<<(std::ostream& output, const CreateField &field)
{
  output << "CreateField:(";
  output <<  field.field_name;
  output << ", ";
  output << display::type(field.type());
  output << ", { ";

  if (field.flags & NOT_NULL_FLAG)
    output << " NOT_NULL";

  if (field.flags & PRI_KEY_FLAG)
    output << ", PRIMARY KEY";

  if (field.flags & UNIQUE_KEY_FLAG)
    output << ", UNIQUE KEY";

  if (field.flags & MULTIPLE_KEY_FLAG)
    output << ", MULTIPLE KEY";

  if (field.flags & BLOB_FLAG)
    output << ", BLOB";

  if (field.flags & UNSIGNED_FLAG)
    output << ", UNSIGNED";

  if (field.flags & BINARY_FLAG)
    output << ", BINARY";
  output << "}, ";
  if (field.field)
    output << *field.field;
  output << ")";

  return output;  // for multiple << operators.
}

} /* namespace drizzled */
