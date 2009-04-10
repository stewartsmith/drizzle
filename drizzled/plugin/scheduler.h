/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Definitions required for Configuration Variables plugin

 *  Copyright (C) 2008 Mark Atwood
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

#ifndef DRIZZLED_PLUGIN_SCHEDULING_H
#define DRIZZLED_PLUGIN_SCHEDULING_H

class Scheduler
{
private:
  uint32_t max_threads;
public:

  Scheduler(uint32_t threads)
    : max_threads(threads) {}

  virtual ~Scheduler() {}

  uint32_t get_max_threads()
  {
    return max_threads;
  }

  virtual uint32_t count(void)= 0;
  virtual bool add_connection(Session *session)= 0;

  virtual bool end_thread(Session *, bool) {return false;}
  virtual bool init_new_connection_thread(void)
  {
    if (my_thread_init())
      return true;
    return false;
  }

  virtual void post_kill_notification(Session *) {}
};

class SchedulerFactory
{
  std::string name;
protected:
  Scheduler *scheduler;
public:
  SchedulerFactory(std::string name_arg): name(name_arg), scheduler(NULL) {}
  SchedulerFactory(const char *name_arg): name(name_arg), scheduler(NULL) {}
  virtual ~SchedulerFactory() {}
  virtual Scheduler *operator()(void)= 0;
  std::string getName() {return name;}
};

#endif /* DRIZZLED_PLUGIN_SCHEDULING_H */
