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

#ifndef DRIZZLED_FUNCTIONS_MASTER_POS_WAIT_H
#define DRIZZLED_FUNCTIONS_MASTER_POS_WAIT_H

#include <drizzled/functions/func.h>
#include <drizzled/functions/int.h>
#include <drizzled/slave.h>

class Item_master_pos_wait :public Item_int_func
{
  String value;
public:
  Item_master_pos_wait(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_master_pos_wait(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  int64_t val_int();
  const char *func_name() const { return "master_pos_wait"; }
  void fix_length_and_dec() { max_length=21; maybe_null=1;}
  bool check_vcol_func_processor(unsigned char *)
  { return true; }
};


#endif /* DRIZZLED_FUNCTIONS_MASTER_POS_WAIT_H */
