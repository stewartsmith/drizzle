/* Copyright (C) 2009 Sun Microsystems, Inc.

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
#include <libgearman/client.h>
#include <string.h>
#include <stdlib.h>

using namespace std;

/* Constructor and destructor happen during module dlopen/dlclose. */
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
  gearman_return_t ret;

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

  gearman_client_remove_servers(&(functionMap[function]));
  ret= gearman_client_add_servers(&(functionMap[function]), servers.c_str());
  pthread_mutex_unlock(&lock);
  if (ret != GEARMAN_SUCCESS)
    return false;

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

  /* Clone the object, the list of host:port pairs get cloned with it. */
  if (gearman_client_clone(client, &((*x).second)) == NULL)
  {
    pthread_mutex_unlock(&lock);
    return false;
  }

  pthread_mutex_unlock(&lock);
  return true;
}
