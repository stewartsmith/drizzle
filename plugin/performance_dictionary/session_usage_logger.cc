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

#include <config.h>

#include <plugin/performance_dictionary/dictionary.h>

#include <drizzled/session.h>

#include <sys/resource.h>

using namespace drizzled;
using namespace std;

namespace performance_dictionary {

bool SessionUsageLogger::pre(Session *session)
{
  session->setUsage(true);
  return false;
}

bool SessionUsageLogger::post(Session *session)
{
  QueryUsage* usage_cache= session->getProperty<QueryUsage>("query_usage");
  if (not usage_cache)
    usage_cache= session->setProperty("query_usage", new QueryUsage);
  usage_cache->push(session->getQueryString(), session->getUsage());
  return false;
}

} /* namespace performance_dictionary */
