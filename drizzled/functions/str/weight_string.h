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

#ifndef DRIZZLED_FUNCTIONS_STR_WEIGHT_STRING_H
#define DRIZZLED_FUNCTIONS_STR_WEIGHT_STRING_H

#include <drizzled/functions/str/strfunc.h> 

class Item_func_weight_string :public Item_str_func
{
  String tmp_value;
  uint32_t flags;
  uint32_t nweights;
public:
  Item_func_weight_string(Item *a, uint32_t nweights_arg, uint32_t flags_arg)
  :Item_str_func(a) { nweights= nweights_arg; flags= flags_arg; }
  const char *func_name() const { return "weight_string"; }
  String *val_str(String *);
  void fix_length_and_dec();
  /*
    TODO: Currently this Item is not allowed for virtual columns
    only due to a bug in generating virtual column value.
  */
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

#endif /* DRIZZLED_FUNCTIONS_STR_WEIGHT_STRING_H */
