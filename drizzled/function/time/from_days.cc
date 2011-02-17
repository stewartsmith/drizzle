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
#include <boost/lexical_cast.hpp>
#include <drizzled/function/time/from_days.h>
#include <drizzled/error.h>
#include <drizzled/temporal.h>

#include <sstream>
#include <string>

namespace drizzled
{

/**
 * Interpret the first argument as a Julian Day Number and fill
 * our supplied temporal object.
 */
bool Item_func_from_days::get_temporal(Date &to)
{
  assert(fixed);

  /* 
   * We MUST call val_int() before checking null_value because, stupidly, 
   * a subselect does not evaluate it's scalar items as null until val_xxx()
   * has been called. :(
   */
  int64_t int_value= args[0]->val_int();

  /* We return NULL from FROM_DAYS() only when supplied a NULL argument */
  if (args[0]->null_value)
  {
    null_value= true;
    return false;
  }

  /* OK, now try to convert from our integer */
  if (! to.from_julian_day_number(int_value))
  {
    /* Bad input, throw an error */
    std::string tmp(boost::lexical_cast<std::string>(int_value));

    my_error(ER_ARGUMENT_OUT_OF_RANGE, MYF(ME_FATALERROR), tmp.c_str(), func_name());
    return false;
  }
  return true;
}

} /* namespace drizzled */
