/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Vijay Samuel
 *  Copyright (C) 2008 MySQL
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

#ifndef CLIENT_STATS_H
#define CLIENT_STATS_H

#include "client_priv.h"
#include <string>
#include <iostream>

class Stats 
{
public:
  Stats(long int in_timing,
        uint32_t in_users,
        uint32_t in_real_users,
        uint32_t in_rows,
        long int in_create_timing,
        uint64_t in_create_count) :
    timing(in_timing),
    users(in_users),
    real_users(in_real_users),
    rows(in_rows),
    create_timing(in_create_timing),
    create_count(in_create_count)
  { }

  Stats() :
    timing(0),
    users(0),
    real_users(0),
    rows(0),
    create_timing(0),
    create_count(0)
  { }

  long int getTiming() const
  {
    return timing;
  }

  uint32_t getUsers() const
  {
    return users;
  }   

  uint32_t getRealUsers() const
  {
    return real_users;
  }

  uint64_t getRows() const
  {
    return rows;
  }

  long int getCreateTiming() const
  {
    return create_timing;
  }

  uint64_t getCreateCount() const
  {
    return create_count;
  }

  void setTiming(long int in_timing)
  {
  timing= in_timing;
  }

  void setUsers(uint32_t in_users)
  {
    users= in_users;
  }

  void setRealUsers(uint32_t in_real_users)
  {
    real_users= in_real_users;
  }

  void setRows(uint64_t in_rows)
  {
    rows= in_rows;
  }
   
  void setCreateTiming(long int in_create_timing)
  {
    create_timing= in_create_timing;
  }

  void setCreateCount(uint64_t in_create_count)
  {
  create_count= in_create_count;
  }
  
private:
  long int timing;
  uint32_t users;
  uint32_t real_users;
  uint64_t rows;
  long int create_timing;
  uint64_t create_count;
};

#endif /* CLIENT_STATS_H */
