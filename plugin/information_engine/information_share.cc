/* Copyright (C) 2009 Sun Microsystems

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "information_share.h"

#include <map>
#include <utility>

using namespace std;

typedef map<const char *, InformationShare* > mapType;
static mapType open_tables;

pthread_mutex_t share_mutex = PTHREAD_MUTEX_INITIALIZER;

InformationShare *InformationShare::get(const char *table_name)
{
  InformationShare *share;
  mapType::iterator iter;
  uint32_t length;

  length= (uint) strlen(table_name);
  pthread_mutex_lock(&share_mutex);

  iter= open_tables.find(table_name);

  if (iter!= open_tables.end())
  {
    share= (*iter).second;
    share->inc();
    return share;
  }

  share= new (nothrow) InformationShare(table_name);

  pthread_mutex_unlock(&share_mutex);

  return share;
}

void InformationShare::start(void)
{
  /* NULL */
}

void InformationShare::stop(void)
{
  pthread_mutex_destroy(&share_mutex);
  open_tables.clear();
}

void InformationShare::free(InformationShare *share)
{
  pthread_mutex_lock(&share_mutex);

  if (share->dec() == 0)
    open_tables.erase(share->name.c_str());
  pthread_mutex_unlock(&share_mutex);
}

