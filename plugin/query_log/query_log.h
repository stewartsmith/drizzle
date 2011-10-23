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

#pragma once

#include <drizzled/plugin/event_observer.h>
#include "file.h"
#include "event.h"

namespace drizzled {
namespace plugin {

/**
 * @brief
 *   QueryLog implements the query_log plugin.
 *
 * @details
 *   This is our event observer plugin subclass that sits between Drizzle
 *   and logger classes like QueryLoggerFile.  This class encapsulates and
 *   manages data associated with the plugin: system variables and incoming
 *   events.  The future plan is to have other logger classes, like
 *   QueryLoggerTable or QueryLoggerUDPSocket.  So implemeting those should
 *   be easy because this class abstracts plugin data management from actual
 *   logging: it gets events from Drizzle, filters and prepares them, then
 *   sends them to logger classes which only have to log them.
 */
class QueryLog : public drizzled::plugin::EventObserver
{
public:
  QueryLog(bool enabled, QueryLoggerFile *logger_file);

  void registerSessionEventsDo(Session &session, EventObserverList &observers);
  bool observeEventDo(EventData &);
  bool afterStatement(AfterStatementEventData &data);

  /**
   * These are the query_log system variables.  So sysvar_enabled is
   * query_log_enabled in SHOW VARIABLES, etc.  They are all global and dynamic.
   */
  bool sysvar_enabled;
  bool sysvar_file_enabled;
  std::string sysvar_file;
  uint32_constraint sysvar_threshold_execution_time;
  uint32_constraint sysvar_threshold_lock_time;
  uint32_constraint sysvar_threshold_rows_examined;
  uint32_constraint sysvar_threshold_rows_sent;
  uint32_constraint sysvar_threshold_tmp_tables;
  uint32_constraint sysvar_threshold_warnings;
  uint32_constraint sysvar_threshold_session_time;

private:
  QueryLoggerFile *_logger_file;
  event_t _event;
};

} /* namespace plugin */
} /* namespace drizzled */
