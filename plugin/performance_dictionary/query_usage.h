/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#pragma once

#include <drizzled/session.h>
#include <drizzled/util/storable.h>

namespace performance_dictionary {

#define USAGE_MAX_KEPT 5

struct query_usage {
  std::string query;
  struct rusage start;
  struct rusage buffer;

  query_usage()
  {
    memset(&start, 0, sizeof(struct rusage));
    memset(&buffer, 0, sizeof(struct rusage));
  }

  void set(const std::string &sql, const struct rusage &arg)
  {
    if (getrusage(RUSAGE_THREAD, &buffer))
    {
      memset(&start, 0, sizeof(struct rusage));
      memset(&buffer, 0, sizeof(struct rusage));
      return;
    }
    query= sql.substr(0, 512);
    start= arg;

    buffer.ru_utime.tv_sec -= start.ru_utime.tv_sec;
    buffer.ru_utime.tv_usec -= start.ru_utime.tv_usec;

    buffer.ru_stime.tv_sec -= start.ru_stime.tv_sec;
    buffer.ru_stime.tv_usec -= start.ru_stime.tv_usec;

    buffer.ru_maxrss -= start.ru_maxrss;
    buffer.ru_ixrss -= start.ru_ixrss;
    buffer.ru_idrss -= start.ru_idrss;
    buffer.ru_isrss -= start.ru_isrss;
    buffer.ru_minflt -= start.ru_minflt;
    buffer.ru_majflt -= start.ru_majflt;
    buffer.ru_nswap -= start.ru_nswap;
    buffer.ru_inblock -= start.ru_inblock;
    buffer.ru_oublock -= start.ru_oublock;
    buffer.ru_msgsnd -= start.ru_msgsnd;
    buffer.ru_msgrcv -= start.ru_msgrcv;
    buffer.ru_nsignals -= start.ru_nsignals;
    buffer.ru_nvcsw -= start.ru_nvcsw;
    buffer.ru_nivcsw -= start.ru_nivcsw;
  }

  const struct rusage &delta(void) const
  {
    return buffer;
  }

  ~query_usage()
  { }
};

typedef std::list <query_usage> Query_list;

class QueryUsage : public drizzled::util::Storable 
{
public:
  Query_list query_list;

  QueryUsage()
  {
    query_list.resize(USAGE_MAX_KEPT);
  }

  void push(drizzled::Session::QueryString query_string, const struct rusage &arg);

  Query_list &list(void)
  {
    return query_list;
  }
};


} /* namespace performance_dictionary */

