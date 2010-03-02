/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include <vector>

#include "drizzled/session_list.h"
#include "drizzled/session.h"
#include "drizzled/current_session.h"
#include "drizzled/plugin/authorization.h"

class Session;

using namespace std;

namespace drizzled
{

vector<Session*> session_list;

vector<Session*> &getSessionList()
{
  return session_list;
}

vector<Session *> getFilteredSessionList()
{
  vector<Session *> filtered_list;
  for (vector<Session *>::iterator iter= session_list.begin();
       iter != session_list.end();
       ++iter)
  {
    if (plugin::Authorization::isAuthorized(current_session->getSecurityContext(),
                                            *iter, false))
    {
      filtered_list.push_back(*iter);
    }
  }
  return filtered_list;
}

} /* namespace drizzled */
