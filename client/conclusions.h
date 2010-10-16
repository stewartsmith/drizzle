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
#ifndef CLIENT_CONCLUSIONS_H
#define CLIENT_CONCLUSIONS_H

#include "client_priv.h"
#include <string>
#include <iostream>

class Conclusions 
{

public:
  Conclusions(char *in_engine,
              long int in_avg_timing,
              long int in_max_timing,
              long int in_min_timing,
              uint32_t in_users,
              uint32_t in_real_users,
              uint64_t in_avg_rows,
              long int in_sum_of_time,
              long int in_std_dev,
              long int in_create_avg_timing,
              long int in_create_max_timing,
              long int in_create_min_timing,
              uint64_t in_create_count,
              uint64_t in_max_rows,
              uint64_t in_min_rows) :
    engine(in_engine),
    avg_timing(in_avg_timing),
    max_timing(in_max_timing),
    min_timing(in_min_timing),
    users(in_users),
    real_users(in_real_users),
    avg_rows(in_avg_rows),
    sum_of_time(in_sum_of_time),
    std_dev(in_std_dev),
    create_avg_timing(in_create_avg_timing),
    create_max_timing(in_create_max_timing),
    create_min_timing(in_create_min_timing),
    create_count(in_create_count),
    max_rows(in_max_rows),
    min_rows(in_min_rows)
  { }

  Conclusions() :
    engine(NULL),
    avg_timing(0),
    max_timing(0),
    min_timing(0),
    users(0),
    real_users(0),
    avg_rows(0),
    sum_of_time(0),
    std_dev(0),
    create_avg_timing(0),
    create_max_timing(0),
    create_min_timing(0),
    create_count(0),
    max_rows(0),
    min_rows(0)
  { }

  char *getEngine() const
  {
    return engine;
  }
  
  long int getAvgTiming() const
  {
    return avg_timing;
  }

  long int getMaxTiming() const
  {
    return max_timing;
  }

  long int getMinTiming() const
  {
    return min_timing;
  }

  uint32_t getUsers() const
  {
    return users;
  }

  uint32_t getRealUsers() const
  {
    return real_users;
  }

  uint64_t getAvgRows() const
  {
    return avg_rows;
  }   

  long int getSumOfTime() const
  {
    return sum_of_time;
  }

  long int getStdDev() const
  {
    return std_dev;
  }

  long int getCreateAvgTiming() const
  {
    return create_avg_timing;
  }

  long int getCreateMaxTiming() const
  {
    return create_max_timing;
  }

  long int getCreateMinTiming() const
  {
    return create_min_timing;
  }
   
  uint64_t getCreateCount() const
  {
    return create_count;
  }

  uint64_t getMinRows() const
  {
    return min_rows;
  }

  uint64_t getMaxRows() const
  {
    return max_rows;
  }

  void setEngine(char *in_engine) 
  {
    engine= in_engine;
  }
  
  void setAvgTiming(long int in_avg_timing) 
  {
    avg_timing= in_avg_timing;
  }

  void setMaxTiming(long int in_max_timing) 
  {
    max_timing= in_max_timing;
  }

  void setMinTiming(long int in_min_timing) 
  {
    min_timing= in_min_timing;
  }

  void setUsers(uint32_t in_users) 
  {
    users= in_users;
  }

  void setRealUsers(uint32_t in_real_users) 
  {
    real_users= in_real_users;
  }

  void setAvgRows(uint64_t in_avg_rows) 
  {
    avg_rows= in_avg_rows;
  }   

  void setSumOfTime(long int in_sum_of_time) 
  {
    sum_of_time= in_sum_of_time;
  }

  void setStdDev(long int in_std_dev) 
  {
    std_dev= in_std_dev;
  }

  void setCreateAvgTiming(long int in_create_avg_timing) 
  {
    create_avg_timing= in_create_avg_timing;
  }

  void setCreateMaxTiming(long int in_create_max_timing) 
  {
    create_max_timing= in_create_max_timing;
  }

  void setCreateMinTiming(long int in_create_min_timing) 
  {
    create_min_timing= in_create_min_timing;
  }
   
  void setCreateCount(uint64_t in_create_count) 
  {
    create_count= in_create_count;
  }

  void setMinRows(uint64_t in_min_rows) 
  {
    min_rows= in_min_rows;
  }

  void setMaxRows(uint64_t in_max_rows) 
  {
    max_rows= in_max_rows;
  }

private:
  char *engine;
  long int avg_timing;
  long int max_timing;
  long int min_timing;
  uint32_t users;
  uint32_t real_users;
  uint64_t avg_rows;
  long int sum_of_time;
  long int std_dev;
  /* These are just for create time stats */
  long int create_avg_timing;
  long int create_max_timing;
  long int create_min_timing;
  uint64_t create_count;
  /* The following are not used yet */
  uint64_t max_rows;
  uint64_t min_rows;
};

#endif /* CLIENT_CONCLUSIONS_H */
