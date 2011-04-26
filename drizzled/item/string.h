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

#pragma once

#include <drizzled/item/basic_constant.h>
#include <drizzled/charset.h>

namespace drizzled
{

class Item_string :public Item_basic_constant
{
public:
  Item_string(const char *str,uint32_t length,
              const charset_info_st * const cs, Derivation dv= DERIVATION_COERCIBLE)
    : m_cs_specified(false)
  {
    str_value.set_or_copy_aligned(str, length, cs);
    collation.set(cs, dv);
    /*
      We have to have a different max_length than 'length' here to
      ensure that we get the right length if we do use the item
      to create a new table. In this case max_length must be the maximum
      number of chars for a string of this type because we in CreateField::
      divide the max_length with mbmaxlen).
    */
    max_length= str_value.numchars()*cs->mbmaxlen;
    set_name(str, length, cs);
    decimals=NOT_FIXED_DEC;
    // it is constant => can be used without fix_fields (and frequently used)
    fixed= 1;
  }
  /* Just create an item and do not fill string representation */
  Item_string(const charset_info_st * const cs, Derivation dv= DERIVATION_COERCIBLE)
    : m_cs_specified(false)
  {
    collation.set(cs, dv);
    max_length= 0;
    set_name(NULL, 0, cs);
    decimals= NOT_FIXED_DEC;
    fixed= 1;
  }
  Item_string(const char *name_par, const char *str, uint32_t length,
              const charset_info_st * const cs, Derivation dv= DERIVATION_COERCIBLE)
    : m_cs_specified(false)
  {
    str_value.set_or_copy_aligned(str, length, cs);
    collation.set(cs, dv);
    max_length= str_value.numchars()*cs->mbmaxlen;
    set_name(name_par, 0, cs);
    decimals=NOT_FIXED_DEC;
    // it is constant => can be used without fix_fields (and frequently used)
    fixed= 1;
  }
  enum Type type() const { return STRING_ITEM; }
  double val_real();
  int64_t val_int();
  String *val_str(String*)
  {
    assert(fixed == 1);
    return (String*) &str_value;
  }
  type::Decimal *val_decimal(type::Decimal *);
  int save_in_field(Field *field, bool no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_VARCHAR; }
  bool basic_const_item() const { return 1; }
  bool eq(const Item *item, bool binary_cmp) const;
  Item *clone_item()
  {
    return new Item_string(name, str_value.ptr(),
    			   str_value.length(), collation.collation);
  }
  Item *safe_charset_converter(const charset_info_st * const tocs);
  inline void append(char *str, uint32_t length)
  {
    str_value.append(str, length);
    max_length= str_value.numchars() * collation.collation->mbmaxlen;
  }
  virtual void print(String *str);

  /**
    Return true if character-set-introducer was explicitly specified in the
    original query for this item (text literal).

    This operation is to be called from Item_string::print(). The idea is
    that when a query is generated (re-constructed) from the Item-tree,
    character-set-introducers should appear only for those literals, where
    they were explicitly specified by the user. Otherwise, that may lead to
    loss collation information (character set introducers implies default
    collation for the literal).

    Basically, that makes sense only for views and hopefully will be gone
    one day when we start using original query as a view definition.

    @return This operation returns the value of m_cs_specified attribute.
      @retval true if character set introducer was explicitly specified in
      the original query.
      @retval false otherwise.
  */
  inline bool is_cs_specified() const
  {
    return m_cs_specified;
  }

  /**
    Set the value of m_cs_specified attribute.

    m_cs_specified attribute shows whether character-set-introducer was
    explicitly specified in the original query for this text literal or
    not. The attribute makes sense (is used) only for views.

    This operation is to be called from the parser during parsing an input
    query.
  */
  inline void set_cs_specified(bool cs_specified)
  {
    m_cs_specified= cs_specified;
  }

private:
  bool m_cs_specified;
};


class Item_static_string_func :public Item_string
{
  const char *func_name;
public:
  Item_static_string_func(const char *name_par, const char *str, uint32_t length,
                          const charset_info_st * const cs,
                          Derivation dv= DERIVATION_COERCIBLE)
    :Item_string(NULL, str, length, cs, dv), func_name(name_par)
  {}
  Item *safe_charset_converter(const charset_info_st * const tocs);

  virtual inline void print(String *str)
  {
    str->append(func_name);
  }
};

} /* namespace drizzled */

