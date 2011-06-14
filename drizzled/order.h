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

#include <drizzled/field.h>

namespace drizzled {

/* Order clause list element */
class Order{
public:
  struct Order *next;
  Item **item;          /* Point at item in select fields */
  Item *item_ptr;       /* Storage for initial item */
  Item **item_copy;     /* For SPs; the original item ptr */
  int  counter;         /* position in SELECT list, correct
                           only if counter_used is true*/
  bool asc;             /* true if ascending */
  bool free_me;         /* true if item isn't shared  */
  bool in_field_list;   /* true if in select field list */
  bool counter_used;    /* parameter was counter of columns */
  Field  *field;        /* If tmp-table group */
  char   *buff;         /* If tmp-table group */
  table_map used, depend_map;

  Order():
    next(NULL),
    item(NULL),
    item_ptr(NULL),
    item_copy(NULL),
    counter(0),
    asc(false),
    free_me(false),
    in_field_list(false),
    counter_used(false),
    field(NULL),
    buff(NULL),
    used(0),
    depend_map(0)
  {}
};
} /* namespace drizzled */

