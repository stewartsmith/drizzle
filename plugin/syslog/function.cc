/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Mark Atwood
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

#include "config.h"

#include <drizzled/gettext.h>
#include <drizzled/session.h>

#include "function.h"
#include "wrap.h"

using namespace drizzled;

Function_syslog::Function_syslog()
  : Item_str_func()
{
  WrapSyslog::singleton().openlog(syslog_module::sysvar_ident);
}

String *Function_syslog::val_str(String *s)
{

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {
    null_value= 1;
    return 0;
  }

  int syslog_facility= WrapSyslog::getFacilityByName(args[0]->val_str(s)->c_ptr());
  int syslog_priority= WrapSyslog::getPriorityByName(args[1]->val_str(s)->c_ptr());

  if ((syslog_facility == -1) || (syslog_priority == -1))
  {
    null_value= 1;
    return 0;
  }

  char *syslog_string= args[2]->val_str(s)->c_ptr();
  if ((syslog_string == 0) || (syslog_string[0] == 0))
  {
    null_value= 1;
    return 0;
  }

  WrapSyslog::singleton().log(syslog_facility, syslog_priority, "%s", syslog_string);

  null_value= 0;
  return args[2]->val_str(s);
}

void Function_syslog::fix_length_and_dec()
{
  max_length= args[0]->max_length;
}

bool Function_syslog::check_argument_count(int n)
{
  return (n == 3);
}

