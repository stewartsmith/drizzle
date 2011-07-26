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

#pragma once

#include <drizzled/field.h>

namespace drizzled {

/**
 * Class representing a field in a CREATE TABLE statement.
 *
 * Basically, all information for a new or altered field
 * definition is contained in the Create_field class.
 */
class CreateField :public memory::SqlAlloc
{
public:
  const char *field_name; /**< Name of the field to be created */
  const char *change; /**< If done with alter table */
  const char *after; /**< Put this new Field after this Field */
  LEX_STRING comment; /**< A comment for this field */
  Item *def; /**< Default value for the new field */
  enum enum_field_types sql_type; /**< The data type of the new field */

  enum_field_types type() const
  {
    return sql_type;
  }

  /**
   * At various stages in execution this can be length of field in bytes or
   * max number of characters.
   */
  uint32_t length;
  /**
   * The value of `length' as set by parser: is the number of characters
   * for most of the types, or of bytes for BLOBs or numeric types.
   */
  uint32_t char_length;
  uint32_t decimals;
  uint32_t flags;
  uint32_t pack_length;
  uint32_t key_length;
  Field::utype unireg_check; /**< See Field::unireg_check */
  TYPELIB *interval; /**< Which interval to use (ENUM types..) */
  List<String> interval_list;
  const charset_info_st *charset; /**< Character set for the column -- @TODO should be deleted */
  Field *field; // For alter table

  uint8_t interval_id;	// For rea_create_table
  uint32_t offset;

  CreateField() :after(0) {}
  CreateField(Field *field, Field *orig_field);
  void create_length_to_internal_length(void);

  inline enum column_format_type column_format() const
  {
    return (enum column_format_type)
      ((flags >> COLUMN_FORMAT_FLAGS) & COLUMN_FORMAT_MASK);
  }

  /**
   * Init for a tmp table field. To be extended if need be. 
   *
   * @note This is currently ONLY used in Item_sum_distinct::setup()
   */
  void init_for_tmp_table(enum_field_types sql_type_arg,
                          uint32_t max_length,
                          uint32_t decimals,
                          bool maybe_null);

  /**
    Initialize field definition for create.

    @param session                   Thread handle
    @param fld_name              Field name
    @param fld_type              Field type
    @param fld_length            Field length
    @param fld_decimals          Decimal (if any)
    @param fld_type_modifier     Additional type information
    @param fld_default_value     Field default value (if any)
    @param fld_on_update_value   The value of ON UPDATE clause
    @param fld_comment           Field comment
    @param fld_change            Field change
    @param fld_interval_list     Interval list (if any)
    @param fld_charset           Field charset

    @retval
      false on success
    @retval
      true  on error
  */
  bool init(Session *session,
            char *field_name,
            enum_field_types type,
            char *length,
            char *decimals,
            uint32_t type_modifier,
            LEX_STRING *comment,
            char *change,
            List<String> *interval_list,
            const charset_info_st * const cs,
            uint32_t uint_geom_type,
            enum column_format_type column_format);

  bool setDefaultValue(Item *default_value, Item *on_update_item);
};

std::ostream& operator<<(std::ostream& output, const CreateField &field);

} /* namespace drizzled */

