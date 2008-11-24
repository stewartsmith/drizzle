/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_CACHED_ITEM_H
#define DRIZZLED_CACHED_ITEM_H

#include <drizzled/sql_alloc.h>
#include <drizzled/sql_string.h>

class Item;
class Session;

class Cached_item :public Sql_alloc
{
public:
  bool null_value;
  Cached_item() :null_value(0) {}
  virtual bool cmp(void)=0;
  virtual ~Cached_item(); /*line -e1509 */
};

class Cached_item_str :public Cached_item
{
  Item *item;
  String value,tmp_value;
public:
  Cached_item_str(Session *session, Item *arg);
  bool cmp(void);
  ~Cached_item_str();                           // Deallocate String:s
};


class Cached_item_real :public Cached_item
{
  Item *item;
  double value;
public:
  Cached_item_real(Item *item_par) :item(item_par),value(0.0) {}
  bool cmp(void);
};

class Cached_item_int :public Cached_item
{
  Item *item;
  int64_t value;
public:
  Cached_item_int(Item *item_par) :item(item_par),value(0) {}
  bool cmp(void);
};


class Cached_item_decimal :public Cached_item
{
  Item *item;
  my_decimal value;
public:
  Cached_item_decimal(Item *item_par);
  bool cmp(void);
};

class Cached_item_field :public Cached_item
{
  unsigned char *buff;
  Field *field;
  uint32_t length;

public:
  Cached_item_field(Field *arg_field) : field(arg_field)
  {
    field= arg_field;
    /* TODO: take the memory allocation below out of the constructor. */
    buff= (unsigned char*) sql_calloc(length=field->pack_length());
  }
  bool cmp(void);
};

Cached_item *new_Cached_item(Session *session, Item *item,
                             bool use_result_field);

#endif /* DRIZZLED_CACHED_ITEM_H */
