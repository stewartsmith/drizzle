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

#include "wrap.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#ifdef __sun
# include <syslog.h>
# include "names.h"
#else
# define SYSLOG_NAMES 1
# include <syslog.h>
#endif

namespace drizzle_plugin
{

WrapSyslog::WrapSyslog () :
  _check(false)
{ }

WrapSyslog::~WrapSyslog ()
{
  ::closelog();
}


/* TODO, for the sake of performance, scan through all the priority
   and facility names, and construct a stl hash, minimal perfect hash,
   or some other high performance read data structure.  This can even
   be done at compile time. */

int WrapSyslog::getPriorityByName(const char *priority_name)
{
  for (int ndx= 0; prioritynames[ndx].c_name; ndx++)
  {
    if (strcasecmp(prioritynames[ndx].c_name, priority_name) == 0)
    {
      return prioritynames[ndx].c_val;
    }
  }
  // no matching priority found
  return -1;
}

int WrapSyslog::getFacilityByName(const char *facility_name)
{
  for (int ndx= 0; facilitynames[ndx].c_name; ndx++)
  {
    if (strcasecmp(facilitynames[ndx].c_name, facility_name) == 0)
    {
      return facilitynames[ndx].c_val;
    }
  }
  // no matching facility found
  return -1;
}

void WrapSyslog::openlog(const std::string &ident)
{
  if (_check == false)
  {
    ::openlog(ident.c_str(), LOG_PID, LOG_USER);
    _check= true;
  }
}

void WrapSyslog::vlog(int facility, int priority, const char *format, va_list ap)
{
  assert(_check == true);
  vsyslog(facility | priority, format, ap);
}

void WrapSyslog::log (int facility, int priority, const char *format, ...)
{
  assert(_check == true);
  va_list ap;
  va_start(ap, format);
  vsyslog(facility | priority, format, ap);
  va_end(ap);
}

} /* namespace drizzle_plugin */
