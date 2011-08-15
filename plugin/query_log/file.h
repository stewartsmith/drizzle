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

#include <iostream>
#include <fstream>
#include "event.h"

/**
 * @brief
 *   QueryLoggerFile implements logging to a file for the QueryLog class.
 *
 * @details
 *   This class is not a plugin (class QueryLog is the plugin class), it is
 *   a utility class used by the QueryLog class to do the actual logging to
 *   the query log file.  QueryLog deals with Drizzle; this class deals with
 *   formatting the event and writing it to the log file.
 */
class QueryLoggerFile
{
public:
  QueryLoggerFile();
  ~QueryLoggerFile();

  /**
   * @brief
   *   Format and write the event to the log file.
   *
   * @details
   *   This function is called by QueryLog::afterStatement().  The given
   *   event is a uniform struct (see event.h) and has passed filtering
   *   (thresholds, etc.), so it's ready to log.
   *
   * @param[in] event Event to log
   *
   * @retval true  Error, event not logged
   * @retval false Success, event logged
   */
  bool logEvent(const event_t *event);

  /**
   * @brief
   *   Open new log file, close old log file if successful.
   *
   * @details
   *   When global system variable query_log_file is changed, update_file()
   *   in module.cc is called which calls this function, passing it the new
   *   log file name.  If opening the new log file succeeds, then the old log
   *   file is closed, else the old log if kept, and error is printed and
   *   query_log_file is not changed.
   *
   * @param[in] file New log file name to open
   *
   * @retval true  Error, new log file not opened, old log file still open
   * @retval false Success, old log file closed, new log file opened
   */
  bool openLogFile(const char *file);

  /**
   * @brief
   *   Close the log file.
   *
   * @details
   *   If query_log_file_enabled is false, then the log file is closed.
   *   However, the log file is not closed if query_log_enabled is false.
   *
   * @retval true  Error, log file may not be closed
   * @retval false Success, log file closed
   */
  bool closeLogFile();

private:
  std::ofstream _fh;  ///< File handle for open log file
};
