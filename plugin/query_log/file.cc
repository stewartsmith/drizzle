/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright 2011 Daniel Nichter
 *  Copyright 2013 Stewart Smith
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
#include "file.h"
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

using namespace std;

QueryLoggerFile::QueryLoggerFile()
{
  _fd= -1;
}

QueryLoggerFile::~QueryLoggerFile()
{
  closeLogFile();
}

bool QueryLoggerFile::logEvent(const event_t *event)
{
  ostringstream ss;
  ss.setf(ios::fixed, ios::floatfield);
  ss.precision(6);

  if (_fd != -1)
  {
    ss << "# start_ts=" << event->ts
       << "\n"
       << "# session_id="     << event->session_id
       <<  " query_id="       << event->query_id
       <<  " rows_examined="  << event->rows_examined
       <<  " rows_sent="      << event->rows_sent
       <<  " tmp_tables="     << event->tmp_tables
       <<  " warnings="       << event->warnings
       << "\n"
       << "# execution_time=" << event->execution_time
       <<  " lock_time="      << event->lock_time
       <<  " session_time="   << event->session_time
       << "\n"
       << "# error=" << event->error << "\n"
       << "# schema=\"" << event->schema << "\"\n"
       << event->query << ";\n#"
       << endl;

    string logmessage= ss.str();

    ssize_t r= write(_fd, logmessage.c_str(), logmessage.length());
    if (r != (ssize_t)logmessage.length())
    {
      fprintf(stderr, "query_log: Incomplete write() to log %d: %s\n",
            errno, strerror(errno));
      return true;
    }
  }
  return false;
}

bool QueryLoggerFile::openLogFile(const char *file)
{
  closeLogFile();

  _fd= open(file, O_CREAT|O_APPEND|O_WRONLY, 0600);
  if (_fd == -1)
  {
    fprintf(stderr, "query_log: Unable to lopen log file %d: %s\n",
            errno, strerror(errno));
    return true;
  }

  return false;
}

bool QueryLoggerFile::closeLogFile()
{
  if (_fd != -1)
  {
    int r= close(_fd);
    if (r == -1)
      return true;
  }

  return false;
}
