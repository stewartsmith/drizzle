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

#include <stdarg.h>
#include <drizzled/plugin/error_message.h>

namespace drizzle_plugin
{
namespace error_message
{

class Syslog : public drizzled::plugin::ErrorMessage
{
private:
  int _facility;
  int _priority;

  Syslog();
  Syslog(const Syslog&);
  Syslog& operator=(const Syslog&);

public:
  explicit Syslog(const std::string& facility,
                  const std::string& priority);

  virtual bool errmsg(drizzled::error::level_t, const char *format, va_list ap);
};

} /* namespace error_message */
} /* namespace drizzle_plugin */

