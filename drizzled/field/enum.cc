/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
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
#include <boost/lexical_cast.hpp>
#include <drizzled/field/enum.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/typelib.h>

#include <sstream>
#include <string>

namespace drizzled {

/****************************************************************************
** enum type.
** This is a string which only can have a selection of different values.
** If one uses this string in a number context one gets the type number.
****************************************************************************/

void Field_enum::store_type(uint64_t value)
{
  value--; /* we store as starting from 0, although SQL starts from 1 */

#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
  {
    int4store(ptr, (unsigned short) value);
  }
  else
#endif
    longstore(ptr, (unsigned short) value);
}

/**
 * Given a supplied string, looks up the string in the internal typelib
 * and stores the found key.  Upon not finding an entry in the typelib,
 * we always throw an error.
 */
int Field_enum::store(const char *from, uint32_t length, const charset_info_st * const)
{
  uint32_t tmp;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  /* Remove end space */
  length= field_charset->cset->lengthsp(field_charset, from, length);
  tmp= typelib->find_type2(from, length, field_charset);
  if (! tmp)
  {
    if (length < 6) /* Can't be more than 99999 enums */
    {
      /* This is for reading numbers with LOAD DATA INFILE */
      /* Convert the string to an integer using stringstream */
      std::stringstream ss;
      ss << from;
      ss >> tmp;

      if (tmp == 0 || tmp > typelib->count)
      {
        my_error(ER_INVALID_ENUM_VALUE, MYF(ME_FATALERROR), from);
        return 1;
      }
    }
    else
    {
      my_error(ER_INVALID_ENUM_VALUE, MYF(ME_FATALERROR), from);
      return 1;
    }
  }
  store_type((uint64_t) tmp);
  return 0;
}

int Field_enum::store(double from)
{
  return Field_enum::store((int64_t) from, false);
}

/**
 * @note MySQL allows 0 values, saying that 0 is "the index of the
 * blank string error", whatever that means.  Uhm, Drizzle doesn't
 * allow this.  To store an ENUM column value using an integer, you
 * must specify the 1-based index of the enum column definition's
 * key.
 */
int Field_enum::store(int64_t from, bool)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (from <= 0 || (uint64_t) from > typelib->count)
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from));

    my_error(ER_INVALID_ENUM_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 1;
  }
  store_type((uint64_t) (uint32_t) from);
  return 0;
}

double Field_enum::val_real(void) const
{
  return (double) Field_enum::val_int();
}

int64_t Field_enum::val_int(void) const
{
  ASSERT_COLUMN_MARKED_FOR_READ;

  uint16_t tmp;
#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
    tmp= sint4korr(ptr);
  else
#endif
    longget(tmp,ptr);
  return ((int64_t) tmp) + 1; /* SQL is from 1, we store from 0 */
}

String *Field_enum::val_str(String *, String *val_ptr) const
{
  uint32_t tmp=(uint32_t) Field_enum::val_int();

  ASSERT_COLUMN_MARKED_FOR_READ;

  if (not tmp || tmp > typelib->count)
  {
    val_ptr->set("", 0, field_charset);
  }
  else
  {
    val_ptr->set((const char*) typelib->type_names[tmp-1], typelib->type_lengths[tmp-1], field_charset);
  }

  return val_ptr;
}

int Field_enum::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  unsigned char *old= ptr;
  ptr= (unsigned char*) a_ptr;
  uint64_t a= Field_enum::val_int();
  ptr= (unsigned char*) b_ptr;
  uint64_t b= Field_enum::val_int();
  ptr= old;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_enum::sort_string(unsigned char *to,uint32_t )
{
  uint64_t value=Field_enum::val_int()-1; /* SQL is 1 based, stored as 0 based*/
  to+=pack_length() -1;
  for (uint32_t i=0 ; i < pack_length() ; i++)
  {
    *to-- = (unsigned char) (value & 255);
    value>>=8;
  }
}

Field *Field_enum::new_field(memory::Root *root, Table *new_table,
                             bool keep_type)
{
  Field_enum *res= (Field_enum*) Field::new_field(root, new_table, keep_type);
  if (res)
  {
    res->typelib= typelib->copy_typelib(*root);
  }
  return res;
}

} /* namespace drizzled */
