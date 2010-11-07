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

#include <stdarg.h>

#include "errmsg.h"
#include "wrap.h"

using namespace drizzled;

ErrorMessage_syslog::ErrorMessage_syslog()
  : drizzled::plugin::ErrorMessage("ErrorMessage_syslog")
{
  syslog_facility= WrapSyslog::getFacilityByName(syslog_module::sysvar_facility);
  if (syslog_facility == -1)
  {
    errmsg_printf(ERRMSG_LVL_WARN,
                  _("syslog facility \"%s\" not known, using \"local0\""),
                  syslog_module::sysvar_facility);
    syslog_facility= WrapSyslog::getFacilityByName("local0");
    assert (! (syslog_facility == -1));
  }

  syslog_priority= WrapSyslog::getPriorityByName(syslog_module::sysvar_errmsg_priority);
  if (syslog_priority == -1)
  {
    errmsg_printf(ERRMSG_LVL_WARN,
                  _("syslog priority \"%s\" not known, using \"warn\""),
                  syslog_module::sysvar_errmsg_priority);
    syslog_priority= WrapSyslog::getPriorityByName("warn");
    assert (! (syslog_priority == -1));
  }

  WrapSyslog::singleton().openlog(syslog_module::sysvar_ident);
}

bool ErrorMessage_syslog::errmsg(drizzled::Session *,
                                 int,
                                 const char *format, va_list ap)
{
  if (syslog_module::sysvar_errmsg_enable == false)
    return false;
  WrapSyslog::singleton().vlog(syslog_facility, syslog_priority, format, ap);
  return false;
}
