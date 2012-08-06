/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <cstdarg>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <boost/date_time.hpp>

#include <drizzled/gettext.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/sql_parse.h>
#include <drizzled/plugin.h>
#include <drizzled/error.h>

#include "logging.h"
#include "wrap.h"

namespace drizzle_plugin {
namespace syslog {
using namespace drizzled;

extern bool sysvar_logging_enable;

logging::Syslog::Syslog(const std::string &facility,
                        uint64_constraint threshold_slow,
                        uint64_constraint threshold_big_resultset,
                        uint64_constraint threshold_big_examined) :
  drizzled::plugin::Logging("syslog_query_log"),
  _facility(WrapSyslog::getFacilityByName(facility.c_str())),
  sysvar_facility(facility),
  _threshold_slow(threshold_slow),
  _threshold_big_resultset(threshold_big_resultset),
  _threshold_big_examined(threshold_big_examined)
{
  if (_facility < 0)
  {
    drizzled::errmsg_printf(drizzled::error::WARN,
                            _("syslog facility \"%s\" not known, using \"local0\""),
                            facility.c_str());
    _facility= WrapSyslog::getFacilityByName("local0");
    sysvar_facility= "local0";
  }
}


bool logging::Syslog::post(drizzled::Session *session)
{
  assert(session != NULL);

  // return if query was not too small
  if (session->sent_row_count < _threshold_big_resultset)
  {
    return false;
  }

  if (session->examined_row_count < _threshold_big_examined)
  {
    return false;
  }
  
  // return if query logging is not enabled
  if (sysvar_logging_enable == false)
      return false;

  
  /*
    TODO, the session object should have a "utime command completed"
    inside itself, so be more accurate, and so this doesnt have to
    keep calling current_utime, which can be slow.
  */
  uint64_t t_mark= session->times.getCurrentTimestamp(false);

  // return if query was not too slow
  if (session->times.getElapsedTime() < _threshold_slow)
  {
    return false;
  }

  drizzled::Session::QueryString query_string(session->getQueryString());
  drizzled::util::string::ptr schema(session->schema());

  WrapSyslog::singleton()
    .log(_facility, drizzled::error::INFO,
         "thread_id=%ld query_id=%ld"
         " db=\"%.*s\""
         " query=\"%.*s\""
         " command=\"%.*s\""
         " t_connect=%lld t_start=%lld t_lock=%lld"
         " rows_sent=%ld rows_examined=%ld"
         " tmp_table=%ld total_warn_count=%ld\n",
         (unsigned long) session->thread_id,
         (unsigned long) session->getQueryId(),
         (int) schema->size(),
         schema->empty() ? "" : schema->c_str(),
         (int) query_string->length(),
         query_string->empty() ? "" : query_string->c_str(),
         (int) drizzled::getCommandName(session->command).size(),
         drizzled::getCommandName(session->command).c_str(),
         (unsigned long long) (t_mark - session->times.getConnectMicroseconds()),
         (unsigned long long) (session->times.getElapsedTime()),
         (unsigned long long) (t_mark - session->times.utime_after_lock),
         (unsigned long) session->sent_row_count,
         (unsigned long) session->examined_row_count,
         (unsigned long) session->tmp_table,
         (unsigned long) session->total_warn_count);

    return false;
}

bool logging::Syslog::setFacility(std::string new_facility)
{
  int tmp_facility= WrapSyslog::getFacilityByName(new_facility.c_str());
  if(tmp_facility>0)
  {
    _facility= tmp_facility;
    sysvar_facility= new_facility;
    return true;
  }
  return false;
}

std::string& logging::Syslog::getFacility()
{
  return sysvar_facility;
}

} /* namespace syslog */
} /* namespsace drizzle_plugin */
