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

#include <drizzled/memory/sql_alloc.h>
#include <drizzled/sql_string.h>


namespace drizzled {

/**
 * A class for quick copying data to fields
 */
class CopyField :public memory::SqlAlloc
{
  /**
    Convenience definition of a copy function returned by
    get_copy_func.
  */
  typedef void Copy_func(CopyField*);
  Copy_func *get_copy_func(Field *to, Field *from);

public:
  unsigned char *from_ptr;
  unsigned char *to_ptr;
  unsigned char *from_null_ptr;
  unsigned char *to_null_ptr;
  bool *null_row;
  uint32_t from_bit;
  uint32_t to_bit;
  uint32_t from_length;
  uint32_t to_length;
  Field *from_field;
  Field *to_field;
  String tmp;					// For items

  CopyField() :
    from_ptr(0),
    to_ptr(0),
    from_null_ptr(0),
    to_null_ptr(0),
    null_row(0),
    from_bit(0),
    to_bit(0),
    from_length(0),
    to_length(0),
    from_field(0),
    to_field(0)
  {}

  ~CopyField()
  {}

  void set(Field *to,Field *from,bool save);	// Field to field
  void set(unsigned char *to,Field *from);		// Field to string
  void (*do_copy)(CopyField *);
  void (*do_copy2)(CopyField *);		// Used to handle null values
};

} /* namespace drizzled */

