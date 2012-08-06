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

#include <config.h>

#include <drizzled/gettext.h>
#include <drizzled/errmsg_print.h>

#include <cstdarg>

#include <plugin/syslog/errmsg.h>
#include <plugin/syslog/wrap.h>

namespace drizzle_plugin {
namespace syslog {

error_message::Syslog::Syslog(const std::string& facility) :
  drizzled::plugin::ErrorMessage("syslog_error_message"),
  _facility(WrapSyslog::getFacilityByName(facility.c_str()))
{
  if (_facility == -1)
  {
    drizzled::errmsg_printf(drizzled::error::WARN,
                            _("syslog facility \"%s\" not known, using \"local0\""),
                            facility.c_str());
    _facility= WrapSyslog::getFacilityByName("local0");
  }
}

bool error_message::Syslog::errmsg(drizzled::error::priority_t priority, const char *format, va_list ap)
{
  WrapSyslog::singleton().vlog(_facility, priority, format, ap);
  return false;
}

} /* namespace syslog */
} /* namespace drizzle_plugin */
