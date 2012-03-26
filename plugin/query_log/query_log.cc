/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright 2011 Daniel Nichter
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <string>
#include <fcntl.h>
#include <drizzled/item.h>
#include <drizzled/module/option_map.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/plugin.h>
#include "query_log.h"

using namespace std;
using namespace drizzled;
using namespace plugin;

QueryLog::QueryLog(bool enabled, QueryLoggerFile *logger_file) :
  drizzled::plugin::EventObserver("file_query_log"),
  sysvar_enabled(enabled),
  _logger_file(logger_file)
{
}

void QueryLog::registerSessionEventsDo(Session &, EventObserverList &observers)
{
  registerEvent(observers, AFTER_STATEMENT);
}

bool QueryLog::observeEventDo(EventData &data)
{
  // Don't log and return successful if...
  if (not sysvar_enabled          // all logging is disabled
      || not sysvar_file_enabled) // or file logging is disabled
    return false;

  switch (data.event) {
  case AFTER_STATEMENT:
    afterStatement((AfterStatementEventData &)data);
    break;
  default:
    fprintf(stderr, "query_log: Unexpected event '%s'\n",
      EventObserver::eventName(data.event));
  }

  return false;
}

bool QueryLog::afterStatement(AfterStatementEventData &data)
{
  Session *session= &data.session;

  // For the moment we're only interestd in queries, not admin
  // command and such.
  if (session->command != COM_QUERY)
    return false;

  // Query end time (microseconds from epoch)
  uint64_t t_mark= session->times.getCurrentTimestamp(false);

  /**
   * Time values, first in microseconds so we can check the thresholds.
   */
  _event.execution_time= session->times.getElapsedTime();
  _event.lock_time= (t_mark - session->times.utime_after_lock);
  _event.session_time= (t_mark - session->times.getConnectMicroseconds());

  /**
   * Check thresholds as soon as possible, return early if possible.
   * This avoid unnecessary work; i.e. don't construct the whole event
   * only to throw it away at the end because it fails a threshold.
   */
  if (   _event.execution_time < sysvar_threshold_execution_time
      || _event.lock_time      < sysvar_threshold_lock_time
      || _event.session_time   < sysvar_threshold_session_time)
    return false;

  /**
   * Convert from microseconds to seconds, e.g. 42 to 0.000042
   * This is done for the user who may read the log.  It's a lot
   * easier to see half a second as 0.5 than 500000.
   */
  _event.execution_time= _event.execution_time * 0.000001;
  _event.lock_time= _event.lock_time * 0.000001;
  _event.session_time= _event.session_time * 0.000001;

  /**
   * Integer values
   */
  _event.session_id= session->getSessionId();
  _event.query_id= session->getQueryId();
  _event.rows_examined= session->examined_row_count;
  _event.rows_sent= session->sent_row_count;
  _event.tmp_tables= session->tmp_table;
  _event.warnings= session->total_warn_count;

  if (   _event.rows_examined < sysvar_threshold_rows_examined
      || _event.rows_sent     < sysvar_threshold_rows_sent
      || _event.tmp_tables    < sysvar_threshold_tmp_tables
      || _event.warnings      < sysvar_threshold_warnings)
    return false;

  /**
   * Boolean values, as strings
   */
  _event.error= session->is_error() ? "true" : "false"; 
 
  /**
   * String values, may be blank
   */ 
  _event.schema= session->schema()->c_str();

  /**
   * The query string
   */
  _event.query= session->getQueryString()->c_str();

  /**
   * Timestamp values, convert from microseconds from epoch to
   * ISO timestamp like 2002-01-31T10:00:01.123456
   */
  boost::posix_time::ptime t_start= session->times.start_timer();
  _event.ts= boost::posix_time::to_iso_extended_string(t_start);

  // Pass the event to the loggers.
  _logger_file->logEvent((const event_t *)&_event);

  return false; // success
}
