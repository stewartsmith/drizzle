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

#include <stdarg.h>
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

#include "logging.h"
#include "wrap.h"

namespace drizzle_plugin {

logging::Syslog::Syslog(const std::string &facility,
                        const std::string &priority,
                        uint64_t threshold_slow,
                        uint64_t threshold_big_resultset,
                        uint64_t threshold_big_examined) :
  drizzled::plugin::Logging("Syslog Logging"),
  _facility(WrapSyslog::getFacilityByName(facility.c_str())),
  _priority(WrapSyslog::getPriorityByName(priority.c_str())),
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
  }

  if (_priority < 0)
  {
    drizzled::errmsg_printf(drizzled::error::WARN,
                            _("syslog priority \"%s\" not known, using \"info\""),
                            priority.c_str());
    _priority= WrapSyslog::getPriorityByName("info");
  }
}


bool logging::Syslog::post(drizzled::Session *session)
{
  assert(session != NULL);

  // return if query was not too small
  if (session->sent_row_count < _threshold_big_resultset)
    return false;
  if (session->examined_row_count < _threshold_big_examined)
    return false;

  /*
    TODO, the session object should have a "utime command completed"
    inside itself, so be more accurate, and so this doesnt have to
    keep calling current_utime, which can be slow.
  */
  uint64_t t_mark= session->times.getCurrentTimestamp(false);

  // return if query was not too slow
  if (session->times.getElapsedTime() < _threshold_slow)
    return false;

  drizzled::Session::QueryString query_string(session->getQueryString());
  drizzled::util::string::ptr schema(session->schema());

  WrapSyslog::singleton()
    .log(_facility, _priority,
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

} /* namespsace drizzle_plugin */
