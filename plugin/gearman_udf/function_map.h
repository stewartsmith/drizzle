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

#pragma once

#include <map>
#include <string>
#include <libgearman/gearman.h>
#include <pthread.h>

class GearmanFunctionMap
{
  std::map<std::string, gearman_client_st> functionMap;
  pthread_mutex_t lock;

public:
  GearmanFunctionMap();
  ~GearmanFunctionMap();
  bool add(std::string function, std::string servers);
  bool get(std::string function, gearman_client_st *client);
};

/* This returns a reference to the global function map class. */
GearmanFunctionMap& GetFunctionMap(void);

