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
#include <sys/stat.h>
#include <fcntl.h>
#include "file.h"

using namespace std;

QueryLoggerFile::QueryLoggerFile()
{
  _fd= LOG_FILE_CLOSED;

  // If you add something here, the number of params must match the number
  // of values pushed to the formatter in logEvent().
  _formatter.parse(
    "# %s\n"
    "# session_id=%d query_id=%d rows_examined=%d rows_sent=%d tmp_tables=%d warnings=%d\n"
    "# execution_time=%.6f lock_time=%.6f session_time=%.6f\n"
    "# error=%s\n"
    "# schema=\"%s\"\n"
    "%s;\n#\n"
  );
}

QueryLoggerFile::~QueryLoggerFile()
{
  closeLogFile();
}

bool QueryLoggerFile::logEvent(const event_t *event)
{
  if (_fd == LOG_FILE_CLOSED)
    return false;

  _formatter
    % event->ts
    % event->session_id
    % event->query_id
    % event->rows_examined
    % event->rows_sent
    % event->tmp_tables
    % event->warnings
    % event->execution_time
    % event->lock_time  // broken
    % event->session_time
    % event->error
    % event->schema
    % event->query;
  string msgbuf= _formatter.str();

  size_t wrv;
  wrv= write(_fd, msgbuf.c_str(), msgbuf.length());
  assert(wrv == msgbuf.length());

  return false; // success
}

bool QueryLoggerFile::openLogFile(const char *file)
{
  assert(file != NULL);
 
  int new_fd= open(file, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
  if (new_fd < 0)
    return true; // error

  closeLogFile();
  _fd= new_fd;

  return false; // success
}

bool QueryLoggerFile::closeLogFile()
{
  if (not _fd == LOG_FILE_CLOSED)
    close(_fd);  // TODO: catch errors
  _fd= LOG_FILE_CLOSED;
  return false;  // success
}
