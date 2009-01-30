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

#include "drizzled/server_includes.h"
#include CSTDINT_H
#include "drizzled/temporal.h"
#include "drizzled/error.h"
#include "drizzled/function/time/dayofmonth.h"

int64_t Item_func_dayofmonth::val_int()
{
  assert(fixed);

  if (args[0]->is_null())
  {
    /* For NULL argument, we return a NULL result */
    null_value= true;
    return 0;
  }

  /* Grab the first argument as a DateTime object */
  drizzled::DateTime temporal;
  Item_result arg0_result_type= args[0]->result_type();
  
  switch (arg0_result_type)
  {
    case STRING_RESULT:
      {
        char buff[40];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res= args[0]->val_str(&tmp);
        if (! temporal.from_string(res->c_ptr(), res->length()))
        {
          /* 
          * Could not interpret the function argument as a temporal value, 
          * so throw an error and return 0
          */
          my_error(ER_INVALID_DATETIME_VALUE, MYF(0), res->c_ptr());
          return 0;
        }
      }
      break;
    case INT_RESULT:
      if (! temporal.from_int64_t(args[0]->val_int()))
      {
        /* 
        * Could not interpret the function argument as a temporal value, 
        * so throw an error and return 0
        */
        null_value= true;
        char buff[40];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res;

        res= args[0]->val_str(&tmp);

        my_error(ER_INVALID_DATETIME_VALUE, MYF(0), res->c_ptr());
        return 0;
      }
      break;
    default:
      {
        /* 
        * Could not interpret the function argument as a temporal value, 
        * so throw an error and return 0
        */
        null_value= true;
        char buff[40];
        String tmp(buff,sizeof(buff), &my_charset_utf8_bin);
        String *res;

        res= args[0]->val_str(&tmp);

        my_error(ER_INVALID_DATETIME_VALUE, MYF(0), res->c_ptr());
        return 0;
      }
  }
  return (int64_t) temporal.days();
}
