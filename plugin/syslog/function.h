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

#pragma once

#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

namespace drizzle_plugin
{
namespace udf
{

class Syslog : public drizzled::Item_str_func
{
private:
  Syslog(const Syslog&);
  Syslog& operator=(const Syslog&);

public:
  Syslog();

  const char *func_name() const { return "syslog"; }

  drizzled::String *val_str(drizzled::String *s);
  void fix_length_and_dec();
  bool check_argument_count(int n);
};

} /* namespace udf */
} /* namepsace drizzle_plugin */


