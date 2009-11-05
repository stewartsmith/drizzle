/*
 *  Copyright (C) 2009 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include "information_cursor.h"
#include "information_share.h"
#include "open_tables.h"

#include <string>
#include <map>

using namespace std;
using namespace drizzled;

InformationShare *OpenTables::getShare(const string &name)
{
  map<const string, InformationShare *>::iterator it;
  pthread_mutex_lock(&mutex);

  it= open_tables.find(name);
  if (it != open_tables.end())
  {
    share= (*it).second;
  }
  else
  {
    share= NULL;
  }

  if (! share)
  {
    share= new(std::nothrow) InformationShare();
    if (! share)
    {
      pthread_mutex_unlock(&mutex);
      return NULL;
    }
    /* get the I_S table */
    share->setInfoSchemaTable(name);
    open_tables[name]= share;
    share->initThreadLock();
  }
  share->incUseCount();
  pthread_mutex_unlock(&mutex);

  return share;
}

void OpenTables::freeShare()
{
  pthread_mutex_lock(&mutex);
  share->decUseCount();
  if (share->getUseCount() == 0)
  {
    open_tables.erase(share->getName());
  }
  pthread_mutex_unlock(&mutex);
}
