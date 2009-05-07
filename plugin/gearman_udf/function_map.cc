/* Copyright (C) 2009 Eric Day

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "function_map.h"
#include <string.h>
#include <stdlib.h>

using namespace std;

static GearmanFunctionMap _functionMap;

GearmanFunctionMap& GetFunctionMap(void)
{
  return _functionMap;
}

GearmanFunctionMap::GearmanFunctionMap()
{
  (void) pthread_mutex_init(&lock, NULL);
}

GearmanFunctionMap::~GearmanFunctionMap()
{
  map<string, gearman_client_st>::iterator x;

  for (x= functionMap.begin(); x != functionMap.end(); x++)
    gearman_client_free(&((*x).second));

  (void) pthread_mutex_destroy(&lock);
}

bool GearmanFunctionMap::add(string function, string servers)
{
  map<string, gearman_client_st>::iterator x;
  string host;
  string port;
  size_t begin_pos= 0;
  size_t end_pos;
  size_t port_pos;

  pthread_mutex_lock(&lock);

  x= functionMap.find(function);
  if (x == functionMap.end())
  {
    if (gearman_client_create(&(functionMap[function])) == NULL)
    {
      pthread_mutex_unlock(&lock);
      return false;
    }
  }

  while (1)
  {
    end_pos= servers.find(',', begin_pos);
    if (end_pos == string::npos)
      host= servers.substr(begin_pos);
    else
      host= servers.substr(begin_pos, end_pos - begin_pos);

    port_pos= host.find(':');
    if (port_pos == string::npos)
      port.clear();
    else
    {
      port= host.substr(port_pos + 1);
      host[port_pos]= 0;
    }

    if (gearman_client_add_server(&(functionMap[function]), host.c_str(),
                                  port.size() == 0 ?
                                  0 : atoi(port.c_str())) != GEARMAN_SUCCESS)
    {
      pthread_mutex_unlock(&lock);
      return false;
    }

    if (end_pos == string::npos)
      break;

    begin_pos= end_pos + 1;
  }

  pthread_mutex_unlock(&lock);
  return true;
}

bool GearmanFunctionMap::get(string function, gearman_client_st *client)
{
  map<string, gearman_client_st>::iterator x;

  pthread_mutex_lock(&lock);

  x= functionMap.find(function);
  if (x == functionMap.end())
  {
    x= functionMap.find(string(""));
    if (x == functionMap.end())
    {
      pthread_mutex_unlock(&lock);
      return false;
    }
  }

  if (gearman_client_clone(client, &((*x).second)) == NULL)
  {
    pthread_mutex_unlock(&lock);
    return false;
  }

  pthread_mutex_unlock(&lock);
  return true;
}
