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
#include "file.h"

using namespace std;

QueryLoggerFile::QueryLoggerFile()
{
  _fh.setf(ios::fixed, ios::floatfield);
  _fh.precision(6);
}

QueryLoggerFile::~QueryLoggerFile()
{
  closeLogFile();
}

bool QueryLoggerFile::logEvent(const event_t *event)
{
  if (_fh.is_open())
  {
    _fh << "# start_ts=" << event->ts
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
  }
  return false; // success
}

bool QueryLoggerFile::openLogFile(const char *file)
{
  closeLogFile();

  _fh.open(file, ios::app);
  if (_fh.fail())
    return true; // error

  return false; // success
}

bool QueryLoggerFile::closeLogFile()
{
  if (_fh.is_open())
  {
    _fh.close();
    if (_fh.fail())
      return true;  // error
  }

  return false;  // success
}
