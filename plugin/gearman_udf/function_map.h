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

#include <map>
#include <string>
#include <libgearman/gearman.h>

class GearmanFunctionMap
{
  std::map<std::string, gearman_client_st> functionMap;
  pthread_mutex_t lock;
  std::string errorString;

public:
  GearmanFunctionMap();
  ~GearmanFunctionMap();
  bool add(std::string function, std::string servers);
  bool get(std::string function, gearman_client_st *client);
};

GearmanFunctionMap& GetFunctionMap(void);
