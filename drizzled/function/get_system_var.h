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

#include <drizzled/function/func.h>
#include <drizzled/lex_string.h>

namespace drizzled {

Item *get_system_var(Session *session, sql_var_t var_type, LEX_STRING name,
                     LEX_STRING component);

/* A system variable */

class Item_func_get_system_var :public Item_func
{
  sys_var *var;
  sql_var_t var_type;
  LEX_STRING component;

public:
  Item_func_get_system_var(sys_var *var_arg, sql_var_t var_type_arg,
                           LEX_STRING *component_arg, const char *name_arg,
                           size_t name_len_arg);
  bool fix_fields(Session *session, Item **ref);
  /*
    Stubs for pure virtual methods. Should never be called: this
    item is always substituted with a constant in fix_fields().
  */
  double val_real()         { assert(0); return 0.0; }
  int64_t val_int()        { assert(0); return 0; }
  String* val_str(String*)  { assert(0); return 0; }
  void fix_length_and_dec() { assert(0); }
  /* TODO: fix to support views */
  const char *func_name() const { return "get_system_var"; }
};


} /* namespace drizzled */

