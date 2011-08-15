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

/**
 * @file
 *   event.h
 *
 * @brief
 *   Defines the event_t struct that encapsulates an event.
 *
 * @details
 *   An event (i.e. a query) has the attributes defined in the event_t struct.
 *   The values come from various members of the Session class.  This is
 *   a necessary redundancy for two reasons.  First, access to this data via
 *   the Session class is not uniform; it requires various calls and
 *   calculations.  Look at QueryLog::afterStatement() to see this.  Second,
 *   because the QueryLog object controls the logger classes, i.e.
 *   QueryLoggerFile and others in the futre, event creation and filtering
 *   is done in one place (QueryLog::afterStatement()) and then acceptable
 *   events are passed to the logger classes so that all they have to do is log.
 *
 *   Since this is just a collection of variables, making this a class
 *   with accessor functions is overkill.
 */
struct event_t {
  // GMT timestamps (2002-01-31T10:00:01.123456)
  std::string ts;

  // integers
  uint32_t session_id;
  uint32_t query_id;
  uint32_t rows_examined;
  uint32_t rows_sent;
  uint32_t tmp_tables;
  uint32_t warnings;

  // times (42.123456)
  double execution_time;
  double lock_time;
  double session_time;

  // bools ("true" or "false")
  const char *error;

  // strings, not quoted or escaped
  const char *schema;

  // query, not quoted or escaped
  const char *query;
};
