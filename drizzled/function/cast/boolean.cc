/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include "config.h"

#include "drizzled/function/cast/boolean.h"
#include "drizzled/error.h"

namespace drizzled {
namespace function {
namespace cast {

void Boolean::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as boolean)"));
}

drizzled::String *Boolean::val_str(drizzled::String *value)
{
  switch (args[0]->result_type())
  {
  case STRING_RESULT:
    {
      drizzled::String _res, *res;

      if (not (res= args[0]->val_str(&_res)))
      { 
        null_value= true; 

        break;
      }
      null_value= false; 

      if (res->length() == 1)
      {
        switch (res->c_ptr()[0])
        {
        case 'y': case 'Y':
        case 't': case 'T': // PG compatibility
          return evaluate(true, value);

        case 'n': case 'N':
        case 'f': case 'F': // PG compatibility
          return evaluate(false, value);

        default:
          break;
        }
      }
      else if ((res->length() == 5) and (strcasecmp(res->c_ptr(), "FALSE") == 0))
      {
        return evaluate(false, value);
      }
      if ((res->length() == 4) and (strcasecmp(res->c_ptr(), "TRUE") == 0))
      {
        return evaluate(true, value);
      }
      else if ((res->length() == 5) and (strcasecmp(res->c_ptr(), "FALSE") == 0))
      {
        return evaluate(false, value);
      }
      else if ((res->length() == 3) and (strcasecmp(res->c_ptr(), "YES") == 0))
      {
        return evaluate(true, value);
      }
      else if ((res->length() == 2) and (strcasecmp(res->c_ptr(), "NO") == 0))
      {
        return evaluate(false, value);
      }

      my_error(ER_INVALID_CAST_TO_BOOLEAN, MYF(0), res->c_ptr());
      return evaluate(false, value);
    }

  case REAL_RESULT:
  case ROW_RESULT:
  case DECIMAL_RESULT:
  case INT_RESULT:
    {
      bool tmp= args[0]->val_bool();
      null_value=args[0]->null_value;

      return evaluate(tmp, value);
    }
  }

  // We should never reach this
  return evaluate(false, value);
}

String *Boolean::evaluate(const bool &result, String *val_buffer)
{
  const CHARSET_INFO * const cs= &my_charset_bin;
  uint32_t mlength= (5) * cs->mbmaxlen;

  val_buffer->alloc(mlength);
  char *buffer=(char*) val_buffer->c_ptr();

  if (result)
  {
    memcpy(buffer, "TRUE", 4);
    val_buffer->length(4);
  }
  else
  {
    memcpy(buffer, "FALSE", 5);
    val_buffer->length(5);
  }

  return val_buffer;
}

} // namespace cast
} // namespace function
} // namespace drizzled
