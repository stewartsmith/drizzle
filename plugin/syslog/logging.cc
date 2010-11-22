/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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
#include <boost/date_time.hpp>
#include <drizzled/gettext.h>
#include <drizzled/session.h>

#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "logging.h"
#include "wrap.h"

using namespace drizzled;

Logging_syslog::Logging_syslog()
  : drizzled::plugin::Logging("Logging_syslog")
{
  syslog_facility= WrapSyslog::getFacilityByName(syslog_module::sysvar_facility);
  if (syslog_facility < 0)
  {
    errmsg_printf(ERRMSG_LVL_WARN,
                  _("syslog facility \"%s\" not known, using \"local0\""),
                  syslog_module::sysvar_facility);
    syslog_facility= WrapSyslog::getFacilityByName("local0");
  }

  syslog_priority= WrapSyslog::getPriorityByName(syslog_module::sysvar_logging_priority);
  if (syslog_priority < 0)
  {
    errmsg_printf(ERRMSG_LVL_WARN,
                  _("syslog priority \"%s\" not known, using \"info\""),
                  syslog_module::sysvar_logging_priority);
    syslog_priority= WrapSyslog::getPriorityByName("info");
  }

  WrapSyslog::singleton().openlog(syslog_module::sysvar_ident);
}


bool Logging_syslog::post (Session *session)
{
  assert(session != NULL);

  if (syslog_module::sysvar_logging_enable == false)
    return false;
  
  // return if query was not too small
  if (session->sent_row_count < syslog_module::sysvar_logging_threshold_big_resultset)
    return false;
  if (session->examined_row_count < syslog_module::sysvar_logging_threshold_big_examined)
    return false;
  
  /* TODO, the session object should have a "utime command completed"
     inside itself, so be more accurate, and so this doesnt have to
     keep calling current_utime, which can be slow */
  
  boost::posix_time::ptime mytime(boost::posix_time::microsec_clock::local_time());
  boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
  uint64_t t_mark= (mytime-epoch).total_microseconds();

  // return if query was not too slow
  if ((t_mark - session->start_utime) < syslog_module::sysvar_logging_threshold_slow)
    return false;
  
  Session::QueryString query_string(session->getQueryString());

  WrapSyslog::singleton()
    .log(syslog_facility, syslog_priority,
         "thread_id=%ld query_id=%ld"
         " db=\"%.*s\""
         " query=\"%.*s\""
         " command=\"%.*s\""
         " t_connect=%lld t_start=%lld t_lock=%lld"
         " rows_sent=%ld rows_examined=%ld"
         " tmp_table=%ld total_warn_count=%ld\n",
         (unsigned long) session->thread_id,
         (unsigned long) session->getQueryId(),
         (int) session->getSchema().length(),
         session->getSchema().empty() ? "" : session->getSchema().c_str(),
         (int) query_string->length(), 
         query_string->empty() ? "" : query_string->c_str(),
         (int) command_name[session->command].length,
         command_name[session->command].str,
         (unsigned long long) (t_mark - session->getConnectMicroseconds()),
         (unsigned long long) (t_mark - session->start_utime),
         (unsigned long long) (t_mark - session->utime_after_lock),
         (unsigned long) session->sent_row_count,
         (unsigned long) session->examined_row_count,
         (unsigned long) session->tmp_table,
         (unsigned long) session->total_warn_count);
  
    return false;
}
