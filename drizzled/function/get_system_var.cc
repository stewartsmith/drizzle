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

#include <config.h>

#include <drizzled/error.h>
#include <drizzled/function/get_system_var.h>
#include <drizzled/session.h>
#include <drizzled/sys_var.h>
#include <drizzled/sql_lex.h>

namespace drizzled {

Item_func_get_system_var::Item_func_get_system_var(sys_var *var_arg, sql_var_t var_type_arg,
                       str_ref component_arg, const char *name_arg, size_t name_len_arg)
  : var(var_arg), var_type(var_type_arg), component(component_arg)
{
  /* set_name() will allocate the name */
  set_name(name_arg, name_len_arg);
}


bool Item_func_get_system_var::fix_fields(Session *session, Item **ref)
{

  /*
    Evaluate the system variable and substitute the result (a basic constant)
    instead of this item. If the variable can not be evaluated,
    the error is reported in sys_var::item().
  */
  Item *item= var->item(session, var_type);
  if (not item)
    return 1;                             // Impossible

  item->set_name(name, 0); // don't allocate a new name
  *ref= item;

  return 0;
}

Item *get_system_var(Session *session, sql_var_t var_type, str_ref name, str_ref component)
{
  str_ref *base_name, *component_name;

  if (component.empty())
  {
    base_name= &name;
    component_name= &component;                 // Empty string
  }
  else
  {
    base_name= &component;
    component_name= &name;
  }

  sys_var *var= find_sys_var(*base_name);
  if (not var)
    return NULL;
  if (not component.empty())
  {
    my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), base_name->data());
    return NULL;
  }
  session->lex().setCacheable(false);

  if (component_name->size() > MAX_SYS_VAR_LENGTH)
    component_name->assign(component_name->data(), MAX_SYS_VAR_LENGTH);

  return new Item_func_get_system_var(var, var_type, *component_name, NULL, 0);
}


} /* namespace drizzled */
